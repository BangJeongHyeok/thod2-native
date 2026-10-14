// Fixed addresses (VA) in the supported Hod2.exe build.
//
// Only valid when thod2::core::FindKnownExe() accepted the host executable.
// Every entry must also be listed in docs/work/reverse-engineering.md
// ("확인된 주소") with the evidence it is based on.
#pragma once

#include <cstdint>

namespace thod2::interop::hod2 {

// Return address of the MessageBoxA call that shows the startup device
// confirmation ("Attention", "... device will be used.", MB_YESNO | MB_ICONEXCLAMATION).
// Evidence: dinput_probe run log (docs/work/loader.md).
constexpr std::uintptr_t kDeviceDialogMessageBoxReturn = 0x4A4758;

}  // namespace thod2::interop::hod2
