#pragma once

#include <filesystem>

namespace thod2::core::patches {

// Hor+ widescreen ([display] widescreen). The game keeps rendering 640x480 and
// dgVoodoo2 stretches it over the whole screen (ScalingMode = stretched); the
// game side is adjusted to that (docs/work/renderer.md "화면 비율 분석"):
//   - the two perspective setups (0x418590, 0x419590) get the display aspect
//     instead of 4:3, so the view is wider at the same vertical field of view;
//   - the cinematic letterbox model (0x413A40) is widened by the same ratio;
//   - 4:3 horizontal limits in software checks and in the aim code are widened;
//   - every screen-space quad (0x4A6ED0) is squeezed towards the centre so 2D
//     keeps its proportions, and player HUD functions are moved to the edges.
struct WidescreenOptions {
  float aspect = 0.0f;      // display width / height; 4:3 or less leaves the game unchanged
  bool squeeze_2d = true;   // false: projection only ([debug] widescreen_2d = 0, comparisons)
  bool trace = false;       // [debug] trace_screen_quads
};

// trace logs every distinct screen quad that is at least 200 px wide or touches
// the top/bottom 40 px, every distinct model with its matrix, the call chain of
// the letterbox model and up to 4 (sprite, x, y) per sprite call site.
bool InstallWidescreen(const WidescreenOptions& options);

// Display aspect for [display] widescreen_aspect: "auto" (or empty) uses the
// primary monitor, otherwise "W:H" or a number such as 1.7778. Returns 0 when
// the value cannot be parsed.
float ParseWidescreenAspect(const std::wstring& value);

// Keeps dgVoodoo.conf in `game_dir` in line with [display] widescreen:
// ScalingMode = stretched when on, and "stretched" back to "stretched_ar" when
// off (other modes are left alone). dgVoodoo2 reads its config after the core
// initializes, so the change applies to this run. Returns true if changed.
bool SyncDgVoodooScaling(const std::filesystem::path& game_dir, bool widescreen);

}  // namespace thod2::core::patches
