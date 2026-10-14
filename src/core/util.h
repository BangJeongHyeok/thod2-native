#pragma once

#include <windows.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace thod2::core {

std::string ToUtf8(std::wstring_view text);

// Full path of the given module (nullptr = the host executable).
std::filesystem::path ModulePath(HMODULE module);

}  // namespace thod2::core
