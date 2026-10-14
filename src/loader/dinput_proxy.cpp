// dinput.dll proxy: the loader of thod2-native.
//
// Hod2.exe imports dinput.dll by name, and dinput.dll is not a KnownDLL, so a
// dinput.dll in the game folder is loaded instead of the system one.
//
// Startup (measured in docs/work/loader.md):
//   1. DllMain: hook Hod2.exe's import of GetCommandLineA. Nothing else, since
//      DllMain runs under the loader lock.
//   2. First GetCommandLineA call (CRT startup, before the main window exists):
//      load thod2_core.dll from this folder and call Thod2Core_Initialize().
//   3. Fallback: if step 2 never happened, initialize on DirectInputCreateEx.
// All dinput exports are forwarded to the system dinput.dll.

#include <thod2/interop/iat_hook.h>

#include <windows.h>
#include <objbase.h>

#include <cwchar>

namespace {

HMODULE g_self = nullptr;

INIT_ONCE g_system_once = INIT_ONCE_STATIC_INIT;
HMODULE g_system_dinput = nullptr;

INIT_ONCE g_core_once = INIT_ONCE_STATIC_INIT;

using GetCommandLineAFn = LPSTR(WINAPI*)();
GetCommandLineAFn g_original_GetCommandLineA = nullptr;

// Path of a file next to this DLL.
bool SiblingPath(const wchar_t* name, wchar_t* out, size_t out_size) {
  const DWORD len = GetModuleFileNameW(g_self, out, static_cast<DWORD>(out_size));
  if (len == 0 || len >= out_size) {
    return false;
  }
  wchar_t* slash = std::wcsrchr(out, L'\\');
  if (!slash) {
    return false;
  }
  return wcscpy_s(slash + 1, out_size - static_cast<size_t>(slash + 1 - out), name) == 0;
}

BOOL CALLBACK LoadSystemDinput(PINIT_ONCE, PVOID, PVOID*) {
  wchar_t path[MAX_PATH];
  const UINT len = GetSystemDirectoryW(path, MAX_PATH);
  if (len > 0 && len < MAX_PATH &&
      wcscpy_s(path + len, MAX_PATH - len, L"\\dinput.dll") == 0) {
    g_system_dinput = LoadLibraryW(path);
  }
  if (!g_system_dinput) {
    OutputDebugStringW(L"thod2 loader: could not load the system dinput.dll\n");
  }
  return TRUE;
}

FARPROC SystemExport(const char* name) {
  InitOnceExecuteOnce(&g_system_once, LoadSystemDinput, nullptr, nullptr);
  return g_system_dinput ? GetProcAddress(g_system_dinput, name) : nullptr;
}

BOOL CALLBACK InitializeCore(PINIT_ONCE, PVOID context, PVOID*) {
  const char* trigger = static_cast<const char*>(context);
  wchar_t path[MAX_PATH];
  HMODULE core = SiblingPath(L"thod2_core.dll", path, MAX_PATH) ? LoadLibraryW(path) : nullptr;
  if (!core) {
    // The game keeps running without thod2-native.
    OutputDebugStringW(L"thod2 loader: thod2_core.dll not found next to dinput.dll\n");
    return TRUE;
  }
  using InitializeFn = int(__cdecl*)(const char*);
  auto initialize = reinterpret_cast<InitializeFn>(GetProcAddress(core, "Thod2Core_Initialize"));
  if (initialize) {
    initialize(trigger);
  }
  return TRUE;
}

void EnsureCoreInitialized(const char* trigger) {
  InitOnceExecuteOnce(&g_core_once, InitializeCore, const_cast<char*>(trigger), nullptr);
}

LPSTR WINAPI Hook_GetCommandLineA() {
  EnsureCoreInitialized("dinput.dll loader, first GetCommandLineA");
  return g_original_GetCommandLineA();
}

}  // namespace

// ---- dinput.dll exports (names mapped in dinput_proxy.def) ----

extern "C" HRESULT WINAPI Proxy_DirectInputCreateEx(HINSTANCE inst, DWORD version, REFIID iid,
                                                    LPVOID* out, LPUNKNOWN outer) {
  EnsureCoreInitialized("dinput.dll loader, DirectInputCreateEx fallback");
  using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
  auto fn = reinterpret_cast<Fn>(SystemExport("DirectInputCreateEx"));
  return fn ? fn(inst, version, iid, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectInputCreateA(HINSTANCE inst, DWORD version, LPVOID* out,
                                                   LPUNKNOWN outer) {
  using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, LPVOID*, LPUNKNOWN);
  auto fn = reinterpret_cast<Fn>(SystemExport("DirectInputCreateA"));
  return fn ? fn(inst, version, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectInputCreateW(HINSTANCE inst, DWORD version, LPVOID* out,
                                                   LPUNKNOWN outer) {
  using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, LPVOID*, LPUNKNOWN);
  auto fn = reinterpret_cast<Fn>(SystemExport("DirectInputCreateW"));
  return fn ? fn(inst, version, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DllCanUnloadNow() {
  using Fn = HRESULT(WINAPI*)();
  auto fn = reinterpret_cast<Fn>(SystemExport("DllCanUnloadNow"));
  return fn ? fn() : S_FALSE;
}

extern "C" HRESULT WINAPI Proxy_DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* out) {
  using Fn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
  auto fn = reinterpret_cast<Fn>(SystemExport("DllGetClassObject"));
  return fn ? fn(clsid, iid, out) : CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT WINAPI Proxy_DllRegisterServer() {
  using Fn = HRESULT(WINAPI*)();
  auto fn = reinterpret_cast<Fn>(SystemExport("DllRegisterServer"));
  return fn ? fn() : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DllUnregisterServer() {
  using Fn = HRESULT(WINAPI*)();
  auto fn = reinterpret_cast<Fn>(SystemExport("DllUnregisterServer"));
  return fn ? fn() : E_FAIL;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = module;
    DisableThreadLibraryCalls(module);
    g_original_GetCommandLineA = static_cast<GetCommandLineAFn>(thod2::interop::HookImport(
        GetModuleHandleW(nullptr), "KERNEL32.dll", "GetCommandLineA",
        reinterpret_cast<void*>(Hook_GetCommandLineA)));
    // If the hook could not be installed, DirectInputCreateEx initializes the core instead.
  }
  return TRUE;
}
