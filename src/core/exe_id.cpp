#include "exe_id.h"

#include <array>
#include <cctype>

namespace thod2::core {
namespace {

constexpr std::array kKnownExes = {
    KnownExe{
        "1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69",
        "Hod2.exe PC 2001 (linked 2001-03-22), CD check call removed at 0x4A5899",
    },
};

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::toupper(static_cast<unsigned char>(a[i])) !=
        std::toupper(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

const KnownExe* FindKnownExe(std::string_view sha256) {
  for (const KnownExe& exe : kKnownExes) {
    if (EqualsIgnoreCase(exe.sha256, sha256)) {
      return &exe;
    }
  }
  return nullptr;
}

}  // namespace thod2::core
