// Fixed addresses (VA) in the supported Hod2.exe build.
//
// Only valid when thod2::core::FindKnownExe() accepted the host executable.
// Every entry must also be listed in re/*.csv with the evidence it is based
// on; tools/re/check_re.py (ctest re_tables) fails otherwise.
#pragma once

#include <cstdint>

namespace thod2::interop::hod2 {

// Return address of the MessageBoxA call that shows the startup device
// confirmation ("Attention", "... device will be used.", MB_YESNO | MB_ICONEXCLAMATION).
// Evidence: dinput_probe run log (docs/work/loader.md).
constexpr std::uintptr_t kDeviceDialogMessageBoxReturn = 0x4A4758;

// Frame wait (docs/work/platform-timing.md "CPU 100% 분석 결과").
// The frame end function 0x4A4B30 busy-waits for the target frame time and
// calls the function pointer in kFrameWaitCallbackSlot with the remaining ms on
// every spin. The pointer is stored by `mov dword [0x7DCDFC], 0x49E0C0` at
// 0x49DF0B; the 32-bit immediate of that instruction is at kFrameWaitCallbackImmediate.
// Evidence: byte decoding of 0x4A4D0F..0x4A4D63 and 0x49DF0B, profiler hotspot.
constexpr std::uintptr_t kFrameWaitCallbackImmediate = 0x49DF11;
constexpr std::uintptr_t kFrameWaitCallbackSlot = 0x7DCDFC;
// Original callback: void __cdecl(float remaining_ms). Processes one queued job
// (0x41D620) when more than 1 ms remains.
constexpr std::uintptr_t kFrameWaitCallback = 0x49E0C0;

// Job queue processed by the frame wait callback: 64 entries of 16 bytes at
// 0x7D8EB0, read index and write index below. Empty when they are equal.
constexpr std::uintptr_t kJobQueueReadIndex = 0x7D8AA8;
constexpr std::uintptr_t kJobQueueWriteIndex = 0x7D8AAC;

}  // namespace thod2::interop::hod2
