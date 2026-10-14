#pragma once

namespace thod2::core::patches {

// Development aid ([debug] trace_ddraw = 1): logs how Hod2.exe drives
// DirectDraw 7 (cooperative level, display mode, surfaces, flips) without
// changing behavior. Per-frame calls are logged a few times and then
// summarized every few seconds.
// `flip_override` is an experiment for simulating other refresh rates:
// 0 = unchanged, 1 = add DDFLIP_NOVSYNC, 2 = add DDFLIP_INTERVAL2 to the primary Flip.
bool InstallDDrawTrace(int flip_override);

}  // namespace thod2::core::patches
