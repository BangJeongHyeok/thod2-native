#include "patches/aspect_experiment.h"

#include "log.h"

#include <thod2/interop/hod2_addresses.h>
#include <thod2/interop/memory_patch.h>

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

namespace thod2::core::patches {
namespace {

namespace hod2 = interop::hod2;

constexpr float kOriginalAspect = 4.0f / 3.0f;
constexpr float kWideAspect = 16.0f / 9.0f;
constexpr float kSqueeze = kOriginalAspect / kWideAspect;  // 0.75
constexpr float kScreenCentreX = 320.0f;

// Sprite record passed to 0x4A6ED0 (15 dwords). Only the fields used here.
constexpr std::size_t kSpriteDwords = 15;
constexpr std::size_t kSpriteX = 7;
constexpr std::size_t kSpriteScaleX = 9;

using DrawScreenQuadFn = void(__cdecl*)(std::uint32_t* sprite);

void __cdecl SqueezedDrawScreenQuad(std::uint32_t* sprite) {
  std::array<std::uint32_t, kSpriteDwords> copy;
  std::memcpy(copy.data(), sprite, sizeof(copy));
  const float x = std::bit_cast<float>(copy[kSpriteX]);
  const float scale_x = std::bit_cast<float>(copy[kSpriteScaleX]);
  copy[kSpriteX] = std::bit_cast<std::uint32_t>(kScreenCentreX + (x - kScreenCentreX) * kSqueeze);
  copy[kSpriteScaleX] = std::bit_cast<std::uint32_t>(scale_x * kSqueeze);
  reinterpret_cast<DrawScreenQuadFn>(hod2::kDrawScreenQuad)(copy.data());
}

std::array<std::uint8_t, 5> CallBytes(std::uintptr_t from, std::uintptr_t to) {
  const auto rel = static_cast<std::uint32_t>(to - (from + 5));
  return {0xE8, static_cast<std::uint8_t>(rel), static_cast<std::uint8_t>(rel >> 8),
          static_cast<std::uint8_t>(rel >> 16), static_cast<std::uint8_t>(rel >> 24)};
}

}  // namespace

bool InstallAspectExperiment(int stage) {
  bool ok = true;
  const auto original = std::bit_cast<std::uint32_t>(kOriginalAspect);
  const auto wide = std::bit_cast<std::uint32_t>(kWideAspect);
  for (std::uintptr_t immediate : {hod2::kPerspectiveAspectImmediate1, hod2::kPerspectiveAspectImmediate2}) {
    if (!interop::PatchUint32(immediate, original, wide)) {
      Log(LogLevel::Warn, "aspect_experiment: aspect at 0x{:X} does not match", immediate);
      ok = false;
    }
  }
  if (stage >= 2) {
    const auto ours = reinterpret_cast<std::uintptr_t>(&SqueezedDrawScreenQuad);
    for (std::uintptr_t call : {hod2::kDrawScreenQuadCall1, hod2::kDrawScreenQuadCall2}) {
      if (!interop::PatchBytes(call, CallBytes(call, hod2::kDrawScreenQuad), CallBytes(call, ours))) {
        Log(LogLevel::Warn, "aspect_experiment: call at 0x{:X} does not match", call);
        ok = false;
      }
    }
  }
  Log(LogLevel::Info, "aspect_experiment: stage {} installed{}", stage, ok ? "" : " (partly)");
  return ok;
}

}  // namespace thod2::core::patches
