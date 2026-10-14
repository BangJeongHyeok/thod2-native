#include "log.h"

#include <windows.h>

#include <mutex>
#include <string>

namespace thod2::core {
namespace {

std::mutex g_mutex;
HANDLE g_file = INVALID_HANDLE_VALUE;
LogLevel g_min_level = LogLevel::Info;

const char* LevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
  }
  return "?";
}

}  // namespace

std::optional<LogLevel> ParseLogLevel(std::wstring_view text) {
  if (text == L"debug") return LogLevel::Debug;
  if (text == L"info") return LogLevel::Info;
  if (text == L"warn") return LogLevel::Warn;
  if (text == L"error") return LogLevel::Error;
  return std::nullopt;
}

bool OpenLog(const std::filesystem::path& path, LogLevel min_level) {
  std::lock_guard lock(g_mutex);
  if (g_file != INVALID_HANDLE_VALUE) {
    CloseHandle(g_file);
  }
  // Each line goes straight to WriteFile, so the log survives a crash of the game.
  g_file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
  g_min_level = min_level;
  return g_file != INVALID_HANDLE_VALUE;
}

void CloseLog() {
  std::lock_guard lock(g_mutex);
  if (g_file != INVALID_HANDLE_VALUE) {
    CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
  }
}

bool ShouldLog(LogLevel level) {
  return level >= g_min_level;
}

void LogLine(LogLevel level, std::string_view message) {
  SYSTEMTIME now;
  GetLocalTime(&now);
  const std::string line =
      std::format("[{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}] [{}] {}\r\n", now.wYear,
                  now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
                  now.wMilliseconds, LevelName(level), message);

  std::lock_guard lock(g_mutex);
  if (g_file == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD written = 0;
  WriteFile(g_file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
}

}  // namespace thod2::core
