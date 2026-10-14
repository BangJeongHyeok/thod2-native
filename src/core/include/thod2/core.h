// Public C API of thod2_core.dll.
//
// The loader calls Thod2Core_Initialize() once after the DLL is mapped into
// Hod2.exe. Initialization is not done in DllMain because it reads files and
// computes hashes, which must not run under the loader lock.
#pragma once

#ifdef THOD2_CORE_BUILD
#define THOD2_CORE_API __declspec(dllexport)
#else
#define THOD2_CORE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum Thod2CoreResult {
  THOD2_CORE_OK = 0,
  // The host executable is not in the list of supported Hod2.exe builds and
  // [core] allow_unknown_exe is not set.
  THOD2_CORE_UNKNOWN_EXE = 1,
  THOD2_CORE_ALREADY_INITIALIZED = 2,
  THOD2_CORE_FAILED = 3,
};

// Version string of the core, e.g. "0.1.0".
THOD2_CORE_API const char* __cdecl Thod2Core_Version(void);

// Reads thod2-native.ini, opens thod2-native.log (both next to the DLL) and
// identifies the host executable. Returns a Thod2CoreResult value.
THOD2_CORE_API int __cdecl Thod2Core_Initialize(void);

#ifdef __cplusplus
}
#endif
