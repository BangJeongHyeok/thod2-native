#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace thod2::core {

// SHA-256 as 64 uppercase hex characters (same format as PowerShell Get-FileHash).
std::optional<std::string> Sha256Bytes(std::span<const std::byte> data);
std::optional<std::string> Sha256File(const std::filesystem::path& path);

}  // namespace thod2::core
