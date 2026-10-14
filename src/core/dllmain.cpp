#include "core.h"
#include "log.h"

#include <thod2/core.h>

namespace {

HMODULE g_self = nullptr;

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = module;
    DisableThreadLibraryCalls(module);
  } else if (reason == DLL_PROCESS_DETACH) {
    thod2::core::CloseLog();
  }
  return TRUE;
}

extern "C" THOD2_CORE_API const char* __cdecl Thod2Core_Version(void) {
  return THOD2_VERSION;
}

extern "C" THOD2_CORE_API int __cdecl Thod2Core_Initialize(const char* loader_info) {
  return thod2::core::Initialize(g_self, loader_info);
}
