#pragma once

#include <cstdint>

namespace thod2::core::patches {

// True when a MessageBoxA call is Hod2.exe's startup device confirmation
// ("Attention" / "... will be used."). `return_address` is where the call returns to.
bool IsDeviceDialog(const char* caption, std::uintptr_t return_address);

// Hooks Hod2.exe's MessageBoxA import so the device confirmation is answered
// with "Yes" without being shown. Other message boxes are passed through.
// Must run before the dialog is shown (the loader initializes before the main window exists).
bool InstallSkipDeviceDialog();

}  // namespace thod2::core::patches
