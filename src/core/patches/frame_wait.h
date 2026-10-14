#pragma once

namespace thod2::core::patches {

// Replaces the busy wait at the end of every frame with a sleep
// ([timing] frame_wait_sleep). Hod2.exe waits for 16.67 ms per frame by
// spinning on QueryPerformanceCounter and calling a callback on every spin.
// This installs our own callback (by patching the immediate that stores the
// callback pointer) which calls the original one and then, when the game's job
// queue is empty, sleeps until `margin_ms` before the deadline. The game's own
// loop still does the final spin, so frame timing stays exact.
// See docs/work/platform-timing.md.
bool InstallFrameWaitSleep(float margin_ms);

}  // namespace thod2::core::patches
