#include "patches/dgvoodoo_preset.h"

#include "log.h"

#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace thod2::core::patches {
namespace {

std::string_view Trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
  return s;
}

}  // namespace

bool SetIniValue(std::string& text, std::string_view section, std::string_view key, std::string_view value,
                 std::string* old) {
  bool in_section = false;
  std::size_t line = 0;
  while (line < text.size()) {
    const std::size_t end = text.find('\n', line);
    const std::size_t stop = end == std::string::npos ? text.size() : end;
    const std::string_view content = Trim(std::string_view(text).substr(line, stop - line));
    if (!content.empty() && content.front() == '[') {
      const std::size_t close = content.find(']');
      in_section = close != std::string_view::npos && content.substr(1, close - 1) == section;
    } else if (in_section) {
      const std::size_t eq = content.find('=');
      if (eq != std::string_view::npos && Trim(content.substr(0, eq)) == key) {
        // Replace everything after '=' up to the line ending ('\r' kept).
        const std::size_t value_begin = text.find('=', line) + 1;
        std::size_t value_end = stop;
        if (value_end > value_begin && text[value_end - 1] == '\r') --value_end;
        if (old) *old = std::string(Trim(std::string_view(text).substr(value_begin, value_end - value_begin)));
        text.replace(value_begin, value_end - value_begin, " " + std::string(value));
        return true;
      }
    }
    line = stop + 1;
  }
  return false;
}

bool ApplyDgVoodooPreset(const std::filesystem::path& game_dir, const DgVoodooPreset& preset) {
  const auto conf = game_dir / "dgVoodoo.conf";
  std::ifstream in(conf, std::ios::binary);
  if (!in) {
    Log(LogLevel::Warn, "dgvoodoo_preset: no dgVoodoo.conf in the game folder (copy it from the dgVoodoo2 zip)");
    return false;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  in.close();
  std::string text = buffer.str();

  // config/dgvoodoo-recommended.ini
  std::vector<std::pair<const char*, std::pair<const char*, std::string>>> values = {
      {"General", {"FullScreenMode", "false"}},
      {"General", {"ScalingMode", preset.widescreen ? "stretched" : "stretched_ar"}},
      {"GeneralExt", {"WindowedAttributes", "borderless,fullscreensize"}},
      {"DirectX", {"Resolution", "max"}},
      {"DirectX", {"Antialiasing", preset.antialiasing}},
      {"DirectX", {"AppControlledScreenMode", "false"}},
      {"DirectX", {"DisableAltEnterToToggleScreenMode", "false"}},
      {"DirectX", {"dgVoodooWatermark", "false"}},
  };
  if (!preset.filtering.empty()) {
    values.push_back({"DirectX", {"Filtering", preset.filtering}});
  }
  int changed = 0;
  for (const auto& [section, pair] : values) {
    std::string old;
    if (!SetIniValue(text, section, pair.first, pair.second, &old)) {
      Log(LogLevel::Warn, "dgvoodoo_preset: [{}] {} not found in dgVoodoo.conf", section, pair.first);
      continue;
    }
    if (old != pair.second) {
      Log(LogLevel::Info, "dgvoodoo_preset: [{}] {} '{}' -> '{}'", section, pair.first, old, pair.second);
      ++changed;
    }
  }
  if (changed == 0) {
    Log(LogLevel::Info, "dgvoodoo_preset: dgVoodoo.conf already has the recommended settings");
    return true;
  }
  std::ofstream out(conf, std::ios::binary | std::ios::trunc);
  out << text;
  if (!out) {
    Log(LogLevel::Warn, "dgvoodoo_preset: cannot write dgVoodoo.conf");
    return false;
  }
  return true;
}

}  // namespace thod2::core::patches
