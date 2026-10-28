#pragma once

#include <cstddef>

namespace thod2::core::patches {

struct FrameInterpOptions {
  // Frames per second limit for the interpolated frames; 0 = only vsync limits them.
  int max_fps = 0;
  // Matrices that moved further than this between two ticks (camera-space units,
  // plus a quarter of their distance) are not interpolated: camera cuts, objects
  // that appeared or a pairing between two different objects.
  float snap_distance = 2.0f;
  // Logs per-interval statistics (presents per tick, interpolated matrices).
  bool trace = false;
};

// Draws frames between the game's 60 Hz ticks ([display] interpolate).
// Hod2.exe updates and draws in the same per-tick function and then waits for
// 16.67 ms in 0x4A4B30. This records the Direct3D calls of a tick instead of
// running them, and replaces that wait with a loop that replays the last two
// ticks with interpolated world-view and projection matrices until the next
// tick is due. See docs/work/platform-timing.md G4.
bool InstallFrameInterp(const FrameInterpOptions& options);

// Pure helper, exposed for the tests: interpolates two row-major 4x4 matrices
// element by element (t = 0 gives `from`, 1 gives `to`). Returns false and
// leaves `out` untouched when the pair should snap instead (see snap_distance).
bool LerpWorldViewMatrix(const float* from, const float* to, float t, float snap_distance, float* out);

}  // namespace thod2::core::patches
