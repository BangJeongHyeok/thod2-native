// Tests for the dinput.dll loader and the IAT hook helper.
// Same style as core_tests: the exit code is the failure count.

#include <thod2/interop/iat_hook.h>
#include <thod2/interop/memory_patch.h>
#include <thod2/interop/vtable_hook.h>

#include <windows.h>

#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
  std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", what);
  if (!condition) {
    ++g_failures;
  }
}

std::filesystem::path TestDir() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return std::filesystem::path(path).parent_path();
}

std::string ReadText(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

size_t CountOccurrences(const std::string& text, const std::string& needle) {
  size_t count = 0;
  for (size_t pos = text.find(needle); pos != std::string::npos;
       pos = text.find(needle, pos + needle.size())) {
    ++count;
  }
  return count;
}

// ---- HookImport ----

using GetTickCountFn = DWORD(WINAPI*)();
GetTickCountFn g_original_GetTickCount = nullptr;
int g_hook_calls = 0;

DWORD WINAPI Hook_GetTickCount() {
  ++g_hook_calls;
  return 12345;
}

// Calls GetTickCount through the IAT. Kept out of line because an optimized
// build may otherwise load the IAT entry once and reuse it across calls.
__declspec(noinline) DWORD CallGetTickCount() {
  return GetTickCount();
}

void TestHookImport() {
  HMODULE self = GetModuleHandleW(nullptr);
  // This test executable imports KERNEL32!GetTickCount through its IAT.
  g_original_GetTickCount = static_cast<GetTickCountFn>(thod2::interop::HookImport(
      self, "KERNEL32.dll", "GetTickCount", reinterpret_cast<void*>(Hook_GetTickCount)));
  Check(g_original_GetTickCount != nullptr, "HookImport returns the original function");
  Check(CallGetTickCount() == 12345 && g_hook_calls == 1, "hooked import is called");

  thod2::interop::HookImport(self, "KERNEL32.dll", "GetTickCount",
                             reinterpret_cast<void*>(g_original_GetTickCount));
  CallGetTickCount();
  Check(g_hook_calls == 1, "restoring the original removes the hook");

  Check(thod2::interop::HookImport(self, "KERNEL32.dll", "NoSuchFunction",
                                   reinterpret_cast<void*>(Hook_GetTickCount)) == nullptr,
        "HookImport on a missing import returns null");
  Check(thod2::interop::HookImport(self, "nosuch.dll", "GetTickCount",
                                   reinterpret_cast<void*>(Hook_GetTickCount)) == nullptr,
        "HookImport on a missing DLL returns null");
}

// ---- HookVtable ----

struct Counter {
  virtual int Value() { return 1; }
  virtual ~Counter() = default;
};

using ValueFn = int(__thiscall*)(Counter*);
ValueFn g_original_Value = nullptr;

int __fastcall Hook_Value(Counter* self, void* /*edx*/) {
  return g_original_Value(self) + 41;
}

// Virtual call through a pointer the compiler cannot see through.
__declspec(noinline) int CallValue(Counter* c) {
  return c->Value();
}

void TestHookVtable() {
  Counter counter;
  Check(CallValue(&counter) == 1, "virtual call before HookVtable");
  g_original_Value = reinterpret_cast<ValueFn>(
      thod2::interop::HookVtable(&counter, 0, reinterpret_cast<void*>(Hook_Value)));
  Check(g_original_Value != nullptr, "HookVtable returns the original entry");
  Check(CallValue(&counter) == 42, "hooked virtual function is called");
  Counter other;
  Check(CallValue(&other) == 42, "vtable hook applies to every object of the class");
  thod2::interop::HookVtable(&counter, 0, reinterpret_cast<void*>(g_original_Value));
  Check(CallValue(&counter) == 1, "restoring the vtable entry removes the hook");
  Check(thod2::interop::HookVtable(nullptr, 0, reinterpret_cast<void*>(Hook_Value)) == nullptr,
        "HookVtable on null object returns null");
}

// ---- PatchBytes ----

// Lives in read-only data, like code bytes of Hod2.exe.
const std::uint8_t kPatchTarget[4] = {0xC0, 0xE0, 0x49, 0x00};

std::uint32_t ReadTarget() {
  return *reinterpret_cast<const volatile std::uint32_t*>(kPatchTarget);
}

void TestPatchBytes() {
  const auto address = reinterpret_cast<std::uintptr_t>(kPatchTarget);
  Check(!thod2::interop::PatchUint32(address, 0x12345678, 0xDEADBEEF) &&
            ReadTarget() == 0x0049E0C0,
        "PatchUint32 with wrong expected value changes nothing");
  Check(thod2::interop::PatchUint32(address, 0x0049E0C0, 0xDEADBEEF) && ReadTarget() == 0xDEADBEEF,
        "PatchUint32 replaces read-only bytes when they match");
  Check(thod2::interop::PatchUint32(address, 0xDEADBEEF, 0x0049E0C0) && ReadTarget() == 0x0049E0C0,
        "PatchUint32 can restore the original bytes");
  const std::uint8_t two[2] = {1, 2};
  const std::uint8_t three[3] = {1, 2, 3};
  Check(!thod2::interop::PatchBytes(address, two, three), "PatchBytes rejects length mismatch");
  Check(!thod2::interop::PatchUint32(0, 0, 1), "PatchUint32 rejects address 0");
}

// ---- dinput.dll loader ----

void TestLoader() {
  const auto dir = TestDir();
  std::filesystem::remove(dir / L"thod2-native.ini");
  std::filesystem::remove(dir / L"thod2-native.log");

  HMODULE loader = LoadLibraryW((dir / L"dinput.dll").c_str());
  Check(loader != nullptr, "LoadLibrary(dinput.dll loader)");
  if (!loader) {
    return;
  }
  const char* exports[] = {"DirectInputCreateA", "DirectInputCreateEx",  "DirectInputCreateW",
                           "DllCanUnloadNow",    "DllGetClassObject",    "DllRegisterServer",
                           "DllUnregisterServer"};
  bool all_exports = true;
  for (const char* name : exports) {
    all_exports = all_exports && GetProcAddress(loader, name) != nullptr;
  }
  Check(all_exports, "loader exports all 7 dinput.dll functions");

  using CreateExFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
  auto create = reinterpret_cast<CreateExFn>(GetProcAddress(loader, "DirectInputCreateEx"));
  for (int i = 0; i < 2 && create; ++i) {
    IUnknown* di = nullptr;
    const HRESULT hr = create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput7A,
                              reinterpret_cast<LPVOID*>(&di), nullptr);
    Check(SUCCEEDED(hr) && di != nullptr, "DirectInputCreateEx is forwarded to the system DLL");
    if (di) {
      di->Release();
    }
  }

  // The test executable called GetCommandLineA before loading the loader, so the
  // DirectInputCreateEx fallback is what initializes the core here.
  const std::string log = ReadText(dir / L"thod2-native.log");
  Check(log.find("initialized by: dinput.dll loader, DirectInputCreateEx fallback") !=
            std::string::npos,
        "loader initializes the core (fallback path)");
  Check(CountOccurrences(log, "initialized by:") == 1, "core is initialized only once");
  Check(GetModuleHandleW((dir / L"thod2_core.dll").c_str()) != nullptr,
        "thod2_core.dll is loaded from the loader's folder");
}

}  // namespace

int main() {
  TestHookImport();
  TestHookVtable();
  TestPatchBytes();
  TestLoader();
  std::printf("%d failure(s)\n", g_failures);
  return g_failures;
}
