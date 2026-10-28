#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace thod2::core::patches {

// [dgvoodoo] preset: writes the recommended dgVoodoo2 settings
// (config/dgvoodoo-recommended.ini) into the game folder's dgVoodoo.conf at
// startup, so that players only copy dgVoodoo2's files and need no setup tool.
// dgVoodoo2 reads its configuration after thod2-native initialises (first
// GetCommandLineA), so the values apply to the same run.
struct DgVoodooPreset {
  bool widescreen = false;       // ScalingMode stretched instead of stretched_ar
  std::string antialiasing = "4x";
  std::string filtering;         // empty: left as it is
};

// Returns false when the game folder has no dgVoodoo.conf.
bool ApplyDgVoodooPreset(const std::filesystem::path& game_dir, const DgVoodooPreset& preset);

// Pure helper, exposed for the tests: sets `key = value` in `[section]` of an
// ini-style text, keeping the line's alignment before '=' and its line ending.
// Returns false when the key is not in that section. `old` receives the
// previous value.
bool SetIniValue(std::string& text, std::string_view section, std::string_view key, std::string_view value,
                 std::string* old = nullptr);

}  // namespace thod2::core::patches
