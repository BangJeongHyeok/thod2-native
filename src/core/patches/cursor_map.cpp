#include "patches/cursor_map.h"

#include "log.h"

#include <thod2/interop/hod2_addresses.h>
#include <thod2/interop/memory_patch.h>

#include <windows.h>

#include <array>
#include <cmath>
#include <fstream>
#include <string>

namespace thod2::core::patches {
namespace {

namespace hod2 = interop::hod2;

constexpr float kGameWidth = 640.0f;
constexpr float kGameHeight = 480.0f;

bool g_installed = false;
bool g_keep_aspect = true;
float g_widen = 1.0f;

using GetCursorClientPosFn = void(__cdecl*)(std::int32_t* xy);

void __cdecl MappedGetCursorClientPos(std::int32_t* xy) {
  reinterpret_cast<GetCursorClientPosFn>(hod2::kGetCursorClientPos)(xy);
  MapCursorToGame(xy);
}

// Value of the first "ScalingMode = ..." line of dgVoodoo.conf, or "" if none.
std::string ReadScalingMode(const std::filesystem::path& conf) {
  std::ifstream in(conf);
  std::string line;
  while (std::getline(in, line)) {
    const auto key = line.find_first_not_of(" \t");
    if (key == std::string::npos || line.compare(key, 11, "ScalingMode") != 0) {
      continue;
    }
    const auto eq = line.find('=', key);
    if (eq == std::string::npos) {
      continue;
    }
    const auto begin = line.find_first_not_of(" \t", eq + 1);
    const auto end = line.find_last_not_of(" \t\r");
    return begin == std::string::npos ? std::string() : line.substr(begin, end - begin + 1);
  }
  return {};
}

std::array<std::uint8_t, 5> CallBytes(std::uintptr_t from, std::uintptr_t to) {
  const auto rel = static_cast<std::uint32_t>(to - (from + 5));
  return {0xE8, static_cast<std::uint8_t>(rel), static_cast<std::uint8_t>(rel >> 8),
          static_cast<std::uint8_t>(rel >> 16), static_cast<std::uint8_t>(rel >> 24)};
}

}  // namespace

void MapCursorToGame(std::int32_t* xy) {
  if (!g_installed) {
    return;
  }
  RECT client{};
  if (!GetClientRect(*reinterpret_cast<HWND*>(hod2::kMainWindow), &client) || client.right <= 0 ||
      client.bottom <= 0) {
    return;
  }
  float x0 = 0.0f;
  float y0 = 0.0f;
  float w = static_cast<float>(client.right);
  float h = static_cast<float>(client.bottom);
  if (g_keep_aspect) {
    const float fit_w = std::fmin(w, h * kGameWidth / kGameHeight);
    const float fit_h = fit_w * kGameHeight / kGameWidth;
    x0 = (w - fit_w) * 0.5f;
    y0 = (h - fit_h) * 0.5f;
    w = fit_w;
    h = fit_h;
  }
  float x = (static_cast<float>(xy[0]) - x0) * kGameWidth / w;
  const float y = (static_cast<float>(xy[1]) - y0) * kGameHeight / h;
  x = kGameWidth * 0.5f + (x - kGameWidth * 0.5f) * g_widen;
  xy[0] = static_cast<std::int32_t>(std::lround(x));
  xy[1] = static_cast<std::int32_t>(std::lround(y));
}

bool InstallCursorMap(const std::filesystem::path& game_dir, float widen) {
  const std::string mode = ReadScalingMode(game_dir / "dgVoodoo.conf");
  g_keep_aspect = mode != "stretched";
  g_widen = widen;
  bool ok = true;
  const auto ours = reinterpret_cast<std::uintptr_t>(&MappedGetCursorClientPos);
  for (std::uintptr_t call : {hod2::kGetCursorClientPosCall1, hod2::kGetCursorClientPosCall2}) {
    if (!interop::PatchBytes(call, CallBytes(call, hod2::kGetCursorClientPos), CallBytes(call, ours))) {
      Log(LogLevel::Warn, "map_cursor: call at 0x{:X} does not match", call);
      ok = false;
    }
  }
  g_installed = true;
  Log(LogLevel::Info, "map_cursor: dgVoodoo2 ScalingMode '{}' -> {}, widen {:.3f}{}", mode,
      g_keep_aspect ? "4:3 image centred in the window" : "image fills the window", widen,
      ok ? "" : " (call sites not patched)");
  return ok;
}

}  // namespace thod2::core::patches
