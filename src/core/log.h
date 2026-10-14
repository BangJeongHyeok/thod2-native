#pragma once

#include <filesystem>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

namespace thod2::core {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

std::optional<LogLevel> ParseLogLevel(std::wstring_view text);

// Opens (and truncates) the log file. Lines below min_level are dropped.
bool OpenLog(const std::filesystem::path& path, LogLevel min_level);
void CloseLog();
bool ShouldLog(LogLevel level);
void LogLine(LogLevel level, std::string_view message);

template <class... Args>
void Log(LogLevel level, std::format_string<Args...> format, Args&&... args) {
  if (!ShouldLog(level)) {
    return;
  }
  LogLine(level, std::format(format, std::forward<Args>(args)...));
}

}  // namespace thod2::core
