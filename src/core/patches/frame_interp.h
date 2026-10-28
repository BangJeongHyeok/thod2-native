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
  // Also interpolates screen-space quads (XYZRHW: crosshair, HUD, menus).
  bool screen_quads = true;
  // Logs per-interval statistics (presents per tick, interpolated matrices).
  bool trace = false;
};

// Screen-space quads with a corner that moved more than this many pixels
// (640x480 back buffer) in one tick are not interpolated.
inline constexpr float kScreenQuadSnapPixels = 64.0f;

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

// Pure helper, exposed for the tests: copies `count` pretransformed vertices of
// `stride` bytes from `to` into `out` and interpolates their screen x, y (the
// first two floats) from `from`. Returns false and leaves `out` untouched when
// a vertex moved more than `snap_pixels`.
bool LerpScreenVertices(const void* from, const void* to, std::size_t stride, std::size_t count, float t,
                        float snap_pixels, void* out);

}  // namespace thod2::core::patches
