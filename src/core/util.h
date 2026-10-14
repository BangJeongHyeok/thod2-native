#pragma once

#include <windows.h>
#include <shtypes.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace thod2::core {

std::string ToUtf8(std::wstring_view text);

// Full path of the given module (nullptr = the host executable).
std::filesystem::path ModulePath(HMODULE module);

// Path of a Windows known folder (e.g. FOLDERID_LocalAppData), empty on failure.
std::filesystem::path KnownFolderPath(REFKNOWNFOLDERID id);

// %LOCALAPPDATA%\thod2-native (not created).
std::filesystem::path LocalDataDir();

}  // namespace thod2::core
