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
//   [debug]
//   trace_ddraw = 0           ; 1 = log DirectDraw calls (display mode, surfaces, flips)
struct CoreConfig {
  LogLevel log_level = LogLevel::Info;
  bool allow_unknown_exe = false;
  bool skip_device_dialog = true;
  bool trace_ddraw = false;
};

CoreConfig LoadConfig(const std::filesystem::path& ini_path);

}  // namespace thod2::core
