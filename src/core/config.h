#pragma once

#include "log.h"

#include <filesystem>
#include <string>

namespace thod2::core {

// Settings from thod2-native.ini. A missing file or key keeps the default.
//
//   [core]
//   log_level = info          ; debug | info | warn | error
//   allow_unknown_exe = 0     ; 1 = keep running on an unsupported Hod2.exe
//
//   [display]
//   skip_device_dialog = 1    ; 1 = answer the startup "Attention" device dialog with Yes
//   widescreen = 0            ; 1 = Hor+ widescreen (sets dgVoodoo2 ScalingMode = stretched)
//   widescreen_aspect = auto  ; auto = primary monitor, or W:H such as 16:9 / 21:9
//   widescreen_mirror = 1     ; 1 = fill the sides of full-screen 2D pictures with their mirrored edge
//   interpolate = 0           ; 1 = draw interpolated frames between the 60 Hz ticks (high refresh rate)
//   max_fps = 0               ; with interpolate: frame limit, 0 = vsync only
//
//   [dgvoodoo]
//   preset = 1                ; 1 = write the recommended dgVoodoo2 settings into dgVoodoo.conf at startup
//   antialiasing = 4x         ; dgVoodoo2 [DirectX] Antialiasing (appdriven, 2x, 4x, 8x)
//   filtering =               ; dgVoodoo2 [DirectX] Filtering, empty = keep (e.g. 16 for anisotropic)
//
//   [assets]
//   replace_textures = 1      ; 1 = load data\tex\<bank>\<texture id>.png instead of the game's texture
//
//   [files]
//   redirect_user_files = 1   ; 1 = keep saves, d3dx.dat and exception.log in the user's AppData
//
//   [timing]
//   frame_wait_sleep = 1      ; 1 = sleep instead of busy-waiting at the end of a frame
//   frame_wait_margin_ms = 1.0 ; time left to the game's own busy wait before the deadline
//
//   [input]
//   reimplement_update = 0    ; 1 = our input update replaces the original (0x41E120)
//   map_cursor = 1            ; 1 = map window pixels to the game's 640x480 screen (dgVoodoo2 window)
//
//   [debug]
//   input_compare = 0         ; 1 = run our input update next to the original and log differences
//   widescreen_2d = 1         ; 0 = widescreen projection only, 2D not squeezed (comparisons)
//   trace_screen_quads = 0    ; 1 = log wide/edge 2D quads, models and the letterbox call chain
//   trace_textures = 0        ; 1 = log textures created without a replacement image (replace_textures)
//   trace_ddraw = 0           ; 1 = log DirectDraw calls (display mode, surfaces, flips)
//   profile = 0               ; 1 = sample the main thread and log where CPU time goes
//   trace_files = 0           ; 1 = log the game's file system calls
//   flip_override = 0         ; with trace_ddraw: 1 = Flip without vsync, 2 = every 2nd vsync
//   trace_interp = 0          ; 1 = log interpolate statistics every 5 s
//   interp_snap = 2.0         ; matrices that move more than this (+ 1/4 of their distance) per tick snap
//   interp_2d = 1             ; 0 = interpolate only 3D, 2D quads (crosshair, HUD) move 60 times per second
struct CoreConfig {
  LogLevel log_level = LogLevel::Info;
  bool allow_unknown_exe = false;
  bool skip_device_dialog = true;
  bool redirect_user_files = true;
  bool frame_wait_sleep = true;
  float frame_wait_margin_ms = 1.0f;
  bool reimplement_input_update = false;
  bool input_compare = false;
  bool map_cursor = true;
  bool widescreen = false;
  std::wstring widescreen_aspect = L"auto";
  bool widescreen_mirror = true;
  bool dgvoodoo_preset = true;
  std::wstring dgvoodoo_antialiasing = L"4x";
  std::wstring dgvoodoo_filtering;  // empty: dgVoodoo.conf value kept
  bool interpolate = false;
  int max_fps = 0;
  bool replace_textures = true;
  bool widescreen_2d = true;
  bool trace_screen_quads = false;
  bool trace_textures = false;
  bool trace_ddraw = false;
  bool profile = false;
  bool trace_files = false;
  int flip_override = 0;
  bool trace_interp = false;
  float interp_snap = 2.0f;
  bool interp_2d = true;
};

CoreConfig LoadConfig(const std::filesystem::path& ini_path);

}  // namespace thod2::core
