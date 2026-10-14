#pragma once

namespace thod2::core::patches {

// Development aid ([debug] trace_ddraw = 1): logs how Hod2.exe drives
// DirectDraw 7 (cooperative level, display mode, surfaces, flips) without
// changing behavior. Per-frame calls are logged a few times and then
// summarized every few seconds.
bool InstallDDrawTrace();

}  // namespace thod2::core::patches
