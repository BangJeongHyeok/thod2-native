#include "patches/file_redirect.h"

#include "log.h"
#include "util.h"

#include <thod2/interop/iat_hook.h>

#include <windows.h>
#include <knownfolders.h>

#include <array>
#include <string>
#include <system_error>
#include <vector>

namespace thod2::core::patches {
namespace {

// The save data lives in four files disguised as game data (987 bytes each),
// read at start-up and rewritten on a normal exit. Paths relative to the game folder.
constexpr std::array<const wchar_t*, 4> kSaveFiles = {
    L"pol\\bg_adv19.bin",
    L"tex\\scr_tod_itm_itamidome2.bin",
    L"pol\\komono_0.bin",
    L"pol\\tv2.bin",
};

bool SamePath(const std::filesystem::path& a, const std::filesystem::path& b) {
  const std::wstring& x = a.native();
  const std::wstring& y = b.native();
  return CompareStringOrdinal(x.c_str(), static_cast<int>(x.size()), y.c_str(),
                              static_cast<int>(y.size()), TRUE) == CSTR_EQUAL;
}

RedirectPaths g_paths;

using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                      HANDLE);
CreateFileAFn o_CreateFileA = nullptr;

std::wstring AnsiToWide(const char* text) {
  const int size = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
  if (size <= 0) return {};
  std::wstring out(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_ACP, 0, text, -1, out.data(), size);
  out.resize(static_cast<std::size_t>(size) - 1);
  return out;
}

std::filesystem::path FullPath(const std::wstring& path) {
  const DWORD size = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
  if (size == 0) return {};
  std::wstring out(size, L'\0');
  const DWORD written = GetFullPathNameW(path.c_str(), size, out.data(), nullptr);
  out.resize(written);
  return std::filesystem::path(out);
}

HANDLE WINAPI H_CreateFileA(LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
                            DWORD disposition, DWORD flags, HANDLE templ) {
  if (path) {
    const std::filesystem::path full = FullPath(AnsiToWide(path));
    if (!full.empty()) {
      if (const auto target = MapUserFile(full, g_paths)) {
        // Wide API so that profile folders with non-ANSI characters work.
        HANDLE h = CreateFileW(target->c_str(), access, share, sa, disposition, flags, templ);
        const DWORD error = h == INVALID_HANDLE_VALUE ? GetLastError() : 0;
        Log(LogLevel::Info, "redirect_user_files: \"{}\" -> \"{}\" ({})", path,
            ToUtf8(target->native()), h == INVALID_HANDLE_VALUE ? std::format("error {}", error) : "ok");
        SetLastError(error);
        return h;
      }
    }
  }
  return o_CreateFileA(path, access, share, sa, disposition, flags, templ);
}

// Copies the existing save files from the game folder the first time, so that
// switching the redirect on keeps the player's progress.
void MigrateSaves() {
  for (const wchar_t* relative : kSaveFiles) {
    const auto user = g_paths.save_dir / relative;
    const auto game = g_paths.game_dir / relative;
    std::error_code ec;
    if (std::filesystem::exists(user, ec)) continue;
    std::filesystem::create_directories(user.parent_path(), ec);
    if (!std::filesystem::exists(game, ec)) {
      Log(LogLevel::Warn, "redirect_user_files: no save file to copy at {}", ToUtf8(game.native()));
      continue;
    }
    // CopyFileW keeps the file times (the game sets them back to 2001-02-07 itself).
    if (CopyFileW(game.c_str(), user.c_str(), TRUE)) {
      Log(LogLevel::Info, "redirect_user_files: copied existing save {} -> {}",
          ToUtf8(game.native()), ToUtf8(user.native()));
    } else {
      Log(LogLevel::Warn, "redirect_user_files: could not copy {} (error {})",
          ToUtf8(game.native()), GetLastError());
    }
  }
}

}  // namespace

std::optional<std::filesystem::path> MapUserFile(const std::filesystem::path& full_path,
                                                 const RedirectPaths& paths) {
  const auto path = full_path.lexically_normal();
  for (const wchar_t* relative : kSaveFiles) {
    if (SamePath(path, (paths.game_dir / relative).lexically_normal())) {
      return paths.save_dir / relative;
    }
  }
  if (SamePath(path, (paths.windows_dir / L"d3dx.dat").lexically_normal())) {
    return paths.local_dir / L"d3dx.dat";
  }
  if (SamePath(path, (paths.game_dir / L"exception.log").lexically_normal())) {
    return paths.local_dir / L"exception.log";
  }
  return std::nullopt;
}

bool InstallFileRedirect() {
  wchar_t windows[MAX_PATH] = {};
  GetWindowsDirectoryW(windows, MAX_PATH);
  const auto roaming = KnownFolderPath(FOLDERID_RoamingAppData);
  if (roaming.empty() || KnownFolderPath(FOLDERID_LocalAppData).empty()) {
    Log(LogLevel::Warn, "redirect_user_files: could not find the user's AppData folders");
    return false;
  }
  g_paths.game_dir = ModulePath(nullptr).parent_path();
  g_paths.windows_dir = windows;
  g_paths.save_dir = roaming / L"thod2-native" / L"save";
  g_paths.local_dir = LocalDataDir();

  std::error_code ec;
  std::filesystem::create_directories(g_paths.save_dir, ec);
  std::filesystem::create_directories(g_paths.local_dir, ec);
  MigrateSaves();

  o_CreateFileA = static_cast<CreateFileAFn>(interop::HookImport(
      GetModuleHandleW(nullptr), "KERNEL32.dll", "CreateFileA", reinterpret_cast<void*>(H_CreateFileA)));
  if (!o_CreateFileA) {
    Log(LogLevel::Warn, "redirect_user_files: could not hook KERNEL32!CreateFileA");
    return false;
  }
  Log(LogLevel::Info, "redirect_user_files: installed (saves in {}, cache and crash log in {})",
      ToUtf8(g_paths.save_dir.native()), ToUtf8(g_paths.local_dir.native()));
  return true;
}

}  // namespace thod2::core::patches
