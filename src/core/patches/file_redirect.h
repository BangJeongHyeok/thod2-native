#pragma once

#include <filesystem>
#include <optional>

namespace thod2::core::patches {

// Folders used to map the files Hod2.exe writes ([files] redirect_user_files).
struct RedirectPaths {
  std::filesystem::path game_dir;     // folder of Hod2.exe (the game's working folder)
  std::filesystem::path windows_dir;  // GetWindowsDirectory()
  std::filesystem::path save_dir;     // %APPDATA%\thod2-native\save
  std::filesystem::path local_dir;    // %LOCALAPPDATA%\thod2-native
};

// Returns where a file the game opens should really be, or nullopt to leave it
// alone. `full_path` must be absolute. Comparison ignores case.
//   <game>\pol\bg_adv19.bin (and the other 3 save files) -> <save_dir>\pol\bg_adv19.bin
//   <windows>\d3dx.dat                                   -> <local_dir>\d3dx.dat
//   <game>\exception.log                                 -> <local_dir>\exception.log
std::optional<std::filesystem::path> MapUserFile(const std::filesystem::path& full_path,
                                                 const RedirectPaths& paths);

// Moves the files Hod2.exe writes out of the game folder and C:\WINDOWS into the
// user's profile: hooks the game's CreateFileA and, on first use, copies the
// existing save files from the game folder so progress is kept.
// See docs/work/platform-filesystem.md.
bool InstallFileRedirect();

}  // namespace thod2::core::patches
