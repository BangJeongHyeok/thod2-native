#pragma once

#include "log.h"

#include <filesystem>

namespace thod2::core {

// Settings from thod2-native.ini. A missing file or key keeps the default.
//
//   [core]
//   log_level = info          ; debug | info | warn | error
//   allow_unknown_exe = 0     ; 1 = keep running on an unsupported Hod2.exe
struct CoreConfig {
  LogLevel log_level = LogLevel::Info;
  bool allow_unknown_exe = false;
};

CoreConfig LoadConfig(const std::filesystem::path& ini_path);

}  // namespace thod2::core
