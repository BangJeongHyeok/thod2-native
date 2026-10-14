#include "patches/device_dialog.h"

#include "log.h"

#include <thod2/interop/hod2_addresses.h>
#include <thod2/interop/iat_hook.h>

#include <windows.h>

#include <intrin.h>

#include <cstring>

#pragma intrinsic(_ReturnAddress)

namespace thod2::core::patches {
namespace {

using MessageBoxAFn = int(WINAPI*)(HWND, LPCSTR, LPCSTR, UINT);
MessageBoxAFn g_original_MessageBoxA = nullptr;

int WINAPI Hook_MessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type) {
  const auto from = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
  if (IsDeviceDialog(caption, from)) {
    Log(LogLevel::Info, "device confirmation dialog answered with Yes without showing it: {}",
        text ? text : "");
    return IDYES;
  }
  return g_original_MessageBoxA(owner, text, caption, type);
}

}  // namespace

bool IsDeviceDialog(const char* caption, std::uintptr_t return_address) {
  return return_address == interop::hod2::kDeviceDialogMessageBoxReturn && caption != nullptr &&
         std::strcmp(caption, "Attention") == 0;
}

bool InstallSkipDeviceDialog() {
  g_original_MessageBoxA = static_cast<MessageBoxAFn>(
      interop::HookImport(GetModuleHandleW(nullptr), "USER32.dll", "MessageBoxA",
                          reinterpret_cast<void*>(Hook_MessageBoxA)));
  if (!g_original_MessageBoxA) {
    Log(LogLevel::Warn, "skip_device_dialog: could not hook USER32!MessageBoxA");
    return false;
  }
  Log(LogLevel::Info, "skip_device_dialog: installed");
  return true;
}

}  // namespace thod2::core::patches
