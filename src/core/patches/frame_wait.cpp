#include "patches/frame_wait.h"

#include "log.h"

#include <thod2/interop/hod2_addresses.h>
#include <thod2/interop/memory_patch.h>

#include <windows.h>

#include <cstdint>

namespace thod2::core::patches {
namespace {

namespace hod2 = interop::hod2;

using FrameWaitCallbackFn = void(__cdecl*)(float remaining_ms);

// Sleeps shorter than this are not worth a timer wait; the game keeps spinning.
constexpr float kMinSleepMs = 0.5f;

float g_margin_ms = 1.0f;
HANDLE g_timer = nullptr;

void SleepFor(float ms) {
  if (g_timer) {
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(ms * 10000.0f);  // relative, 100 ns units
    if (SetWaitableTimer(g_timer, &due, 0, nullptr, nullptr, FALSE)) {
      WaitForSingleObject(g_timer, INFINITE);
      return;
    }
  }
  // Fallback without a high resolution timer: Sleep() is only accurate to the
  // system tick, so stay well away from the deadline.
  if (ms >= 2.0f) {
    Sleep(static_cast<DWORD>(ms) - 1);
  }
}

// Replaces 0x49E0C0 as the frame wait callback. Same signature: the game
// pushes the remaining milliseconds as a float and cleans up the stack.
void __cdecl FrameWaitCallback(float remaining_ms) {
  reinterpret_cast<FrameWaitCallbackFn>(hod2::kFrameWaitCallback)(remaining_ms);

  // Jobs still queued: return so the game keeps processing one per spin, as before.
  const auto read = *reinterpret_cast<volatile std::uint32_t*>(hod2::kJobQueueReadIndex);
  const auto write = *reinterpret_cast<volatile std::uint32_t*>(hod2::kJobQueueWriteIndex);
  if (read != write) {
    return;
  }
  const float sleep_ms = remaining_ms - g_margin_ms;
  if (sleep_ms >= kMinSleepMs) {
    SleepFor(sleep_ms);
  }
}

}  // namespace

bool InstallFrameWaitSleep(float margin_ms) {
  g_margin_ms = margin_ms;
  g_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                   TIMER_ALL_ACCESS);
  if (!g_timer) {
    Log(LogLevel::Warn, "frame_wait_sleep: high resolution timer not available, using Sleep()");
  }

  const auto ours = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&FrameWaitCallback));
  if (!interop::PatchUint32(hod2::kFrameWaitCallbackImmediate,
                            static_cast<std::uint32_t>(hod2::kFrameWaitCallback), ours)) {
    Log(LogLevel::Warn, "frame_wait_sleep: callback store at 0x{:X} does not match, not installed",
        hod2::kFrameWaitCallbackImmediate);
    return false;
  }
  // In case the game already stored the original callback.
  auto* slot = reinterpret_cast<volatile std::uint32_t*>(hod2::kFrameWaitCallbackSlot);
  if (*slot == hod2::kFrameWaitCallback) {
    *slot = ours;
  }
  Log(LogLevel::Info, "frame_wait_sleep: installed (margin {:.2f} ms, timer {})", margin_ms,
      g_timer ? "high resolution" : "Sleep()");
  return true;
}

}  // namespace thod2::core::patches
