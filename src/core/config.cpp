#include "config.h"

#include <windows.h>

#include <cwchar>

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
  config.skip_device_dialog =
      GetPrivateProfileIntW(L"display", L"skip_device_dialog", 1, file) != 0;
  config.redirect_user_files =
      GetPrivateProfileIntW(L"files", L"redirect_user_files", 1, file) != 0;
  config.frame_wait_sleep = GetPrivateProfileIntW(L"timing", L"frame_wait_sleep", 1, file) != 0;
  wchar_t margin[32] = {};
  GetPrivateProfileStringW(L"timing", L"frame_wait_margin_ms", L"", margin, ARRAYSIZE(margin), file);
  if (margin[0] != L'\0') {
    wchar_t* end = nullptr;
    const float value = std::wcstof(margin, &end);
    // Keep the default for unparsable or unreasonable values.
    if (end != margin && value >= 0.0f && value <= 16.0f) {
      config.frame_wait_margin_ms = value;
    }
  }
  config.reimplement_input_update =
      GetPrivateProfileIntW(L"input", L"reimplement_update", 0, file) != 0;
  config.input_compare = GetPrivateProfileIntW(L"debug", L"input_compare", 0, file) != 0;
  config.map_cursor = GetPrivateProfileIntW(L"input", L"map_cursor", 1, file) != 0;
  config.widescreen = GetPrivateProfileIntW(L"display", L"widescreen", 0, file) != 0;
  wchar_t aspect[32] = {};
  GetPrivateProfileStringW(L"display", L"widescreen_aspect", L"auto", aspect, ARRAYSIZE(aspect), file);
  config.widescreen_aspect = aspect;
  config.replace_textures = GetPrivateProfileIntW(L"assets", L"replace_textures", 1, file) != 0;
  config.widescreen_2d = GetPrivateProfileIntW(L"debug", L"widescreen_2d", 1, file) != 0;
  config.trace_screen_quads = GetPrivateProfileIntW(L"debug", L"trace_screen_quads", 0, file) != 0;
  config.trace_ddraw = GetPrivateProfileIntW(L"debug", L"trace_ddraw", 0, file) != 0;
  config.profile = GetPrivateProfileIntW(L"debug", L"profile", 0, file) != 0;
  config.trace_files = GetPrivateProfileIntW(L"debug", L"trace_files", 0, file) != 0;
  const UINT flip_override = GetPrivateProfileIntW(L"debug", L"flip_override", 0, file);
  config.flip_override = flip_override <= 2 ? static_cast<int>(flip_override) : 0;
  return config;
}

}  // namespace thod2::core
