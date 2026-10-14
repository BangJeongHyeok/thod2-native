#include "config.h"

#include <windows.h>

namespace thod2::core {

CoreConfig LoadConfig(const std::filesystem::path& ini_path) {
  CoreConfig config;
  const wchar_t* file = ini_path.c_str();

  wchar_t level[16] = {};
  GetPrivateProfileStringW(L"core", L"log_level", L"info", level, ARRAYSIZE(level), file);
  if (auto parsed = ParseLogLevel(level)) {
    config.log_level = *parsed;
  }

  config.allow_unknown_exe = GetPrivateProfileIntW(L"core", L"allow_unknown_exe", 0, file) != 0;
  return config;
}

}  // namespace thod2::core
