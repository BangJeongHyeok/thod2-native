#pragma once

#include "log.h"

#include <filesystem>

namespace thod2::core {

// Settings from thod2-native.ini. A missing file or key keeps the default.
//
//   [core]
//   log_level = info          ; debug | info | warn | error
//   allow_unknown_exe = 0     ; 1 = keep running on an unsupported Hod2.exe
//
//   [display]
//   skip_device_dialog = 1    ; 1 = answer the startup "Attention" device dialog with Yes
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
//
//   [debug]
//   input_compare = 0         ; 1 = run our input update next to the original and log differences
//   trace_ddraw = 0           ; 1 = log DirectDraw calls (display mode, surfaces, flips)
//   profile = 0               ; 1 = sample the main thread and log where CPU time goes
//   trace_files = 0           ; 1 = log the game's file system calls
//   flip_override = 0         ; with trace_ddraw: 1 = Flip without vsync, 2 = every 2nd vsync
struct CoreConfig {
  LogLevel log_level = LogLevel::Info;
  bool allow_unknown_exe = false;
  bool skip_device_dialog = true;
  bool redirect_user_files = true;
  bool frame_wait_sleep = true;
  float frame_wait_margin_ms = 1.0f;
  bool reimplement_input_update = false;
  bool input_compare = false;
  bool trace_ddraw = false;
  bool profile = false;
  bool trace_files = false;
  int flip_override = 0;
};

CoreConfig LoadConfig(const std::filesystem::path& ini_path);

}  // namespace thod2::core
