#pragma once

#include <string_view>

namespace thod2::core {

// A Hod2.exe build that the hooks and patches are written against.
// Fixed addresses (see docs/work/reverse-engineering.md) are only valid for these files.
struct KnownExe {
  std::string_view sha256;  // uppercase hex
  std::string_view name;
};

// Returns nullptr when the hash is not a supported build. Case-insensitive.
const KnownExe* FindKnownExe(std::string_view sha256);

}  // namespace thod2::core
