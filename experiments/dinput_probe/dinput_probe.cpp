// Experiment: a logging dinput.dll proxy used to decide the loader design.
//
// Drop the built dinput.dll next to Hod2.exe. It forwards every export to the
// system dinput.dll and writes dinput_probe.log next to itself with:
//   - when DllMain runs (process attach),
//   - when Hod2.exe first calls GetCommandLineA (CRT startup, after all DllMains),
//   - CreateWindowExA / DirectDrawCreateEx / MessageBoxA calls (IAT hooks set in DllMain),
//   - when DirectInputCreateEx is called and which windows exist at that moment.
//
// Not product code. See docs/work/loader.md for the results.

#include <windows.h>
#include <objbase.h>

#include <intrin.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#pragma intrinsic(_ReturnAddress)

namespace {

HMODULE g_self = nullptr;
HMODULE g_real = nullptr;
HANDLE g_log = INVALID_HANDLE_VALUE;
LARGE_INTEGER g_freq{};
LARGE_INTEGER g_start{};

void Log(const char* format, ...) {
  if (g_log == INVALID_HANDLE_VALUE) {
    return;
  }
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  const double ms = static_cast<double>(now.QuadPart - g_start.QuadPart) * 1000.0 /
                    static_cast<double>(g_freq.QuadPart);
  char line[2048];
  int n = std::snprintf(line, sizeof(line), "[%10.3f ms] [tid %5lu] ", ms, GetCurrentThreadId());
  va_list args;
  va_start(args, format);
  n += std::vsnprintf(line + n, sizeof(line) - n - 2, format, args);
  va_end(args);
  if (n > static_cast<int>(sizeof(line)) - 3) {
    n = static_cast<int>(sizeof(line)) - 3;
  }
  line[n++] = '\r';
  line[n++] = '\n';
  DWORD written = 0;
  WriteFile(g_log, line, static_cast<DWORD>(n), &written, nullptr);
}

BOOL CALLBACK LogWindowProc(HWND hwnd, LPARAM) {
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid != GetCurrentProcessId()) {
    return TRUE;
  }
  char cls[128] = {};
  char title[256] = {};
  GetClassNameA(hwnd, cls, sizeof(cls));
  GetWindowTextA(hwnd, title, sizeof(title));
  RECT r{};
  GetWindowRect(hwnd, &r);
  Log("    window %p class='%s' title='%s' visible=%d rect=(%ld,%ld)-(%ld,%ld)", hwnd, cls, title,
      IsWindowVisible(hwnd) ? 1 : 0, r.left, r.top, r.right, r.bottom);
  return TRUE;
}

void LogWindows(const char* when) {
  Log("  windows of this process at %s:", when);
  EnumWindows(LogWindowProc, 0);
}

FARPROC Real(const char* name) {
  if (!g_real) {
    wchar_t path[MAX_PATH];
    const UINT len = GetSystemDirectoryW(path, MAX_PATH);
    wcscpy_s(path + len, MAX_PATH - len, L"\\dinput.dll");
    g_real = LoadLibraryW(path);
    Log("loaded system dinput.dll -> %p", g_real);
  }
  return g_real ? GetProcAddress(g_real, name) : nullptr;
}

// Replaces the IAT slot of `module` that currently points at `dll!func`.
// Matching by address avoids depending on the import name table.
void* PatchIat(HMODULE module, const char* dll, const char* func, void* hook) {
  HMODULE target = GetModuleHandleA(dll);
  void* original = target ? reinterpret_cast<void*>(GetProcAddress(target, func)) : nullptr;
  if (!original) {
    Log("IAT patch %s!%s: function not found", dll, func);
    return nullptr;
  }
  auto* base = reinterpret_cast<BYTE*>(module);
  auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
  for (; desc->Name; ++desc) {
    if (_stricmp(reinterpret_cast<char*>(base + desc->Name), dll) != 0) {
      continue;
    }
    auto* slot = reinterpret_cast<void**>(base + desc->FirstThunk);
    for (; *slot; ++slot) {
      if (*slot != original) {
        continue;
      }
      DWORD old = 0;
      VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old);
      *slot = hook;
      VirtualProtect(slot, sizeof(void*), old, &old);
      Log("IAT patch %s!%s: slot %p, original %p", dll, func, slot, original);
      return original;
    }
  }
  Log("IAT patch %s!%s: slot not found", dll, func);
  return nullptr;
}

// ---- hooks installed into Hod2.exe's IAT ----

using GetCommandLineAFn = LPSTR(WINAPI*)();
GetCommandLineAFn g_GetCommandLineA = nullptr;
bool g_seen_cmdline = false;

LPSTR WINAPI Hook_GetCommandLineA() {
  if (!g_seen_cmdline) {
    g_seen_cmdline = true;
    Log("first GetCommandLineA from %p (CRT startup, outside loader lock)", _ReturnAddress());
  }
  return g_GetCommandLineA();
}

using CreateWindowExAFn = HWND(WINAPI*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND,
                                        HMENU, HINSTANCE, LPVOID);
CreateWindowExAFn g_CreateWindowExA = nullptr;

HWND WINAPI Hook_CreateWindowExA(DWORD ex_style, LPCSTR cls, LPCSTR name, DWORD style, int x,
                                 int y, int w, int h, HWND parent, HMENU menu, HINSTANCE inst,
                                 LPVOID param) {
  void* from = _ReturnAddress();
  HWND hwnd = g_CreateWindowExA(ex_style, cls, name, style, x, y, w, h, parent, menu, inst, param);
  Log("CreateWindowExA class='%s' title='%s' from %p -> %p",
      (reinterpret_cast<UINT_PTR>(cls) >> 16) ? cls : "(atom)", name ? name : "", from, hwnd);
  return hwnd;
}

using DirectDrawCreateExFn = HRESULT(WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
DirectDrawCreateExFn g_DirectDrawCreateEx = nullptr;

HRESULT WINAPI Hook_DirectDrawCreateEx(GUID* guid, LPVOID* out, REFIID iid, IUnknown* outer) {
  Log("DirectDrawCreateEx from %p", _ReturnAddress());
  return g_DirectDrawCreateEx(guid, out, iid, outer);
}

using MessageBoxAFn = int(WINAPI*)(HWND, LPCSTR, LPCSTR, UINT);
MessageBoxAFn g_MessageBoxA = nullptr;

int WINAPI Hook_MessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type) {
  Log("MessageBoxA caption='%s' type=0x%X from %p text='%s'", caption ? caption : "", type,
      _ReturnAddress(), text ? text : "");
  const int result = g_MessageBoxA(owner, text, caption, type);
  Log("MessageBoxA returned %d", result);
  return result;
}

void InstallHooks() {
  HMODULE exe = GetModuleHandleW(nullptr);
  g_GetCommandLineA = static_cast<GetCommandLineAFn>(
      PatchIat(exe, "KERNEL32.dll", "GetCommandLineA", reinterpret_cast<void*>(Hook_GetCommandLineA)));
  g_CreateWindowExA = static_cast<CreateWindowExAFn>(
      PatchIat(exe, "USER32.dll", "CreateWindowExA", reinterpret_cast<void*>(Hook_CreateWindowExA)));
  g_MessageBoxA = static_cast<MessageBoxAFn>(
      PatchIat(exe, "USER32.dll", "MessageBoxA", reinterpret_cast<void*>(Hook_MessageBoxA)));
  g_DirectDrawCreateEx = static_cast<DirectDrawCreateExFn>(PatchIat(
      exe, "DDRAW.dll", "DirectDrawCreateEx", reinterpret_cast<void*>(Hook_DirectDrawCreateEx)));
}

}  // namespace

// ---- dinput.dll exports (names mapped in dinput_probe.def) ----

extern "C" HRESULT WINAPI Proxy_DirectInputCreateEx(HINSTANCE inst, DWORD version, REFIID iid,
                                                    LPVOID* out, LPUNKNOWN outer) {
  Log("DirectInputCreateEx version=0x%X from %p", version, _ReturnAddress());
  LogWindows("DirectInputCreateEx");
  using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
  auto fn = reinterpret_cast<Fn>(Real("DirectInputCreateEx"));
  const HRESULT hr = fn ? fn(inst, version, iid, out, outer) : E_FAIL;
  Log("DirectInputCreateEx -> 0x%08lX", hr);
  return hr;
}

extern "C" HRESULT WINAPI Proxy_DirectInputCreateA(HINSTANCE inst, DWORD version, LPVOID* out,
                                                   LPUNKNOWN outer) {
  Log("DirectInputCreateA from %p", _ReturnAddress());
  using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, LPVOID*, LPUNKNOWN);
  auto fn = reinterpret_cast<Fn>(Real("DirectInputCreateA"));
  return fn ? fn(inst, version, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectInputCreateW(HINSTANCE inst, DWORD version, LPVOID* out,
                                                   LPUNKNOWN outer) {
  Log("DirectInputCreateW from %p", _ReturnAddress());
  using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, LPVOID*, LPUNKNOWN);
  auto fn = reinterpret_cast<Fn>(Real("DirectInputCreateW"));
  return fn ? fn(inst, version, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DllCanUnloadNow() {
  using Fn = HRESULT(WINAPI*)();
  auto fn = reinterpret_cast<Fn>(Real("DllCanUnloadNow"));
  return fn ? fn() : S_FALSE;
}

extern "C" HRESULT WINAPI Proxy_DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* out) {
  using Fn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
  auto fn = reinterpret_cast<Fn>(Real("DllGetClassObject"));
  return fn ? fn(clsid, iid, out) : CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT WINAPI Proxy_DllRegisterServer() {
  using Fn = HRESULT(WINAPI*)();
  auto fn = reinterpret_cast<Fn>(Real("DllRegisterServer"));
  return fn ? fn() : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DllUnregisterServer() {
  using Fn = HRESULT(WINAPI*)();
  auto fn = reinterpret_cast<Fn>(Real("DllUnregisterServer"));
  return fn ? fn() : E_FAIL;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = module;
    DisableThreadLibraryCalls(module);
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_start);

    wchar_t path[MAX_PATH];
    const DWORD len = GetModuleFileNameW(module, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (len && slash) {
      wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"dinput_probe.log");
      g_log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    Log("DllMain PROCESS_ATTACH (exe base %p)", GetModuleHandleW(nullptr));
    LogWindows("DllMain");
    InstallHooks();
  } else if (reason == DLL_PROCESS_DETACH) {
    Log("DllMain PROCESS_DETACH");
    if (g_log != INVALID_HANDLE_VALUE) {
      CloseHandle(g_log);
    }
  }
  return TRUE;
}
