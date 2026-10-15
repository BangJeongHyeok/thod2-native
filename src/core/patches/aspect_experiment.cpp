#include "patches/aspect_experiment.h"

#include "log.h"

#include <thod2/interop/hod2_addresses.h>
#include <thod2/interop/memory_patch.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <set>
#include <string>

namespace thod2::core::patches {
namespace {

namespace hod2 = interop::hod2;

constexpr float kOriginalAspect = 4.0f / 3.0f;
constexpr float kWideAspect = 16.0f / 9.0f;
constexpr float kSqueeze = kOriginalAspect / kWideAspect;  // 0.75
constexpr float kScreenCentreX = 320.0f;

// Sprite record passed to 0x4A6ED0 (15 dwords). Only the fields used here.
constexpr std::size_t kSpriteDwords = 15;
constexpr std::size_t kSpriteTexture = 3;
constexpr std::size_t kSpriteAnchor = 6;
constexpr std::size_t kSpriteX = 7;
constexpr std::size_t kSpriteY = 8;
constexpr std::size_t kSpriteScaleX = 9;
constexpr std::size_t kSpriteScaleY = 10;

using DrawScreenQuadFn = void(__cdecl*)(std::uint32_t* sprite);

bool g_squeeze = false;
bool g_trace = false;
constexpr float kTraceMinWidth = 200.0f;
std::set<std::uint64_t> g_traced;

// Texture width/height as DrawScreenQuad reads them: two tables of {w, h}
// pairs, the second one for ids with the high bit set.
void TextureSize(std::uint32_t id, std::uint32_t* w, std::uint32_t* h) {
  const auto table = *reinterpret_cast<const std::uint32_t* const*>(
      (id & 0x80000000) ? hod2::kTextureSizeTable2 : hod2::kTextureSizeTable);
  const std::uint32_t index = id & 0x7FFFFFFF;
  *w = table[index * 2];
  *h = table[index * 2 + 1];
}

// Logs every distinct wide quad (texture, position, scale) once.
void TraceWideQuad(const std::uint32_t* sprite) {
  std::uint32_t w = 0;
  std::uint32_t h = 0;
  TextureSize(sprite[kSpriteTexture], &w, &h);
  const float x = std::bit_cast<float>(sprite[kSpriteX]);
  const float y = std::bit_cast<float>(sprite[kSpriteY]);
  const float sx = std::bit_cast<float>(sprite[kSpriteScaleX]);
  const float sy = std::bit_cast<float>(sprite[kSpriteScaleY]);
  const float width = static_cast<float>(w) * sx;
  const float height = static_cast<float>(h) * sy;
  // Wide quads, and any quad touching the top or bottom 40 px (letterbox bars).
  const bool edge = (y - height < 40.0f) || (y + height > 440.0f);
  if (width < kTraceMinWidth && !edge) {
    return;
  }
  const std::uint64_t key = (static_cast<std::uint64_t>(sprite[kSpriteTexture]) << 32) ^
                            (static_cast<std::uint64_t>(static_cast<std::int32_t>(y)) << 16) ^
                            static_cast<std::uint64_t>(static_cast<std::int32_t>(x));
  if (!g_traced.insert(key).second || g_traced.size() > 400) {
    return;
  }
  Log(LogLevel::Info,
      "screen_quad: texture 0x{:X} ({}x{}) x {:.1f} y {:.1f} scale {:.3f} x {:.3f} -> {:.0f} x {:.0f} px, "
      "anchor 0x{:X}, flags 0x{:X}, color 0x{:08X}",
      sprite[kSpriteTexture], w, h, x, y, sx, sy, width, static_cast<float>(h) * sy,
      sprite[kSpriteAnchor], sprite[0], sprite[4]);
}

using DrawModelFn = std::uint32_t(__cdecl*)(std::uint32_t* record, std::uint32_t flag);

// Model draw record: +0xC model, +0x34 world matrix (the view matrix is a
// constant diag(1, 1, -1), so this is effectively the camera-space matrix).
constexpr std::size_t kRecordModel = 3;
constexpr std::size_t kRecordMatrix = 13;

bool g_widen_letterbox = false;
int g_letterbox_stack_logs = 0;
std::set<std::uint32_t> g_traced_models;

// The cinematic letterbox model (model table entry 0x93E), drawn twice by
// 0x413A40 in camera space at (0, +-0.35, -1). It is sized for the 4:3 view.
std::uint32_t LetterboxModel() {
  return *reinterpret_cast<const std::uint32_t*>(hod2::kModelTable +
                                                 hod2::kLetterboxModelIndex * hod2::kModelTableStride + 4);
}

// Logs each distinct model once with its matrix (diagonal, off-diagonal sum, position).
void TraceModel(const std::uint32_t* record, const char* which) {
  const float* m = reinterpret_cast<const float*>(record + kRecordMatrix);
  const float off = std::abs(m[1]) + std::abs(m[2]) + std::abs(m[4]) + std::abs(m[6]) +
                    std::abs(m[8]) + std::abs(m[9]);
  if (!g_traced_models.insert(record[kRecordModel]).second || g_traced_models.size() > 300) {
    return;
  }
  Log(LogLevel::Info,
      "model ({}): model 0x{:08X} off-diagonal {:.4f} diag ({:.3f}, {:.3f}, {:.3f}) pos ({:.2f}, {:.2f}, {:.2f}) flags 0x{:08X}",
      which, record[3], off, m[0], m[5], m[10], m[12], m[13], m[14], record[0]);
}

// Logs return addresses into Hod2.exe found on the stack (a call right before
// them), as a rough call chain for a model of interest.
void LogStackCalls(const void* frame, std::uint32_t model) {
  const auto* stack = static_cast<const std::uint32_t*>(frame);
  std::string chain;
  for (int i = 0; i < 512; ++i) {
    const std::uint32_t v = stack[i];
    if (v < 0x401005 || v >= 0x4C2000) {
      continue;
    }
    const auto* code = reinterpret_cast<const std::uint8_t*>(v);
    if (code[-5] == 0xE8 || (code[-6] == 0xFF && (code[-5] & 0x38) == 0x10) ||
        (code[-2] == 0xFF && (code[-1] & 0x38) == 0x10) || (code[-3] == 0xFF && (code[-2] & 0x38) == 0x10)) {
      chain += std::format(" 0x{:X}", v);
    }
  }
  Log(LogLevel::Info, "model 0x{:08X} stack returns:{}", model, chain);
}

// Draws a model through `original`; the letterbox model is first widened by
// the aspect ratio change so that it covers the wider view.
std::uint32_t DrawModelWith(std::uintptr_t original, const char* which, std::uint32_t* record,
                            std::uint32_t flag) {
  const bool letterbox = record[kRecordModel] == LetterboxModel();
  if (g_trace) {
    TraceModel(record, which);
    if (letterbox && g_letterbox_stack_logs < 3) {
      ++g_letterbox_stack_logs;
      LogStackCalls(&record, record[kRecordModel]);
    }
  }
  float* m = reinterpret_cast<float*>(record + kRecordMatrix);
  const std::array<float, 3> row{m[0], m[1], m[2]};
  if (g_widen_letterbox && letterbox) {
    const float widen = kWideAspect / kOriginalAspect;
    m[0] *= widen;
    m[1] *= widen;
    m[2] *= widen;
  }
  const std::uint32_t result = reinterpret_cast<DrawModelFn>(original)(record, flag);
  m[0] = row[0];
  m[1] = row[1];
  m[2] = row[2];
  return result;
}

std::uint32_t __cdecl HookedDrawModel(std::uint32_t* record, std::uint32_t flag) {
  return DrawModelWith(hod2::kDrawModel, "DrawModel", record, flag);
}

std::uint32_t __cdecl HookedDrawModelAlt(std::uint32_t* record, std::uint32_t flag) {
  return DrawModelWith(hod2::kDrawModelAlt, "DrawModelAlt", record, flag);
}

void __cdecl SqueezedDrawScreenQuad(std::uint32_t* sprite) {
  if (g_trace) {
    TraceWideQuad(sprite);
  }
  if (!g_squeeze) {
    reinterpret_cast<DrawScreenQuadFn>(hod2::kDrawScreenQuad)(sprite);
    return;
  }
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

bool InstallAspectExperiment(int stage, bool trace_screen_quads) {
  bool ok = true;
  g_squeeze = stage >= 2;
  g_trace = trace_screen_quads;
  g_widen_letterbox = stage >= 1;
  const auto original = std::bit_cast<std::uint32_t>(kOriginalAspect);
  const auto wide = std::bit_cast<std::uint32_t>(kWideAspect);
  for (std::uintptr_t immediate : {hod2::kPerspectiveAspectImmediate1, hod2::kPerspectiveAspectImmediate2}) {
    if (stage == 0) {
      break;
    }
    if (!interop::PatchUint32(immediate, original, wide)) {
      Log(LogLevel::Warn, "aspect_experiment: aspect at 0x{:X} does not match", immediate);
      ok = false;
    }
  }
  if (g_squeeze || g_trace) {
    const auto ours = reinterpret_cast<std::uintptr_t>(&SqueezedDrawScreenQuad);
    for (std::uintptr_t call : {hod2::kDrawScreenQuadCall1, hod2::kDrawScreenQuadCall2}) {
      if (!interop::PatchBytes(call, CallBytes(call, hod2::kDrawScreenQuad), CallBytes(call, ours))) {
        Log(LogLevel::Warn, "aspect_experiment: call at 0x{:X} does not match", call);
        ok = false;
      }
    }
  }
  if (g_widen_letterbox || g_trace) {
    const struct {
      std::uintptr_t call;
      std::uintptr_t original;
      void* ours;
    } model_calls[] = {
        {hod2::kDrawModelCall1, hod2::kDrawModel, reinterpret_cast<void*>(&HookedDrawModel)},
        {hod2::kDrawModelCall2, hod2::kDrawModel, reinterpret_cast<void*>(&HookedDrawModel)},
        {hod2::kDrawModelAltCall1, hod2::kDrawModelAlt, reinterpret_cast<void*>(&HookedDrawModelAlt)},
        {hod2::kDrawModelAltCall2, hod2::kDrawModelAlt, reinterpret_cast<void*>(&HookedDrawModelAlt)},
    };
    for (const auto& c : model_calls) {
      const auto ours = reinterpret_cast<std::uintptr_t>(c.ours);
      if (!interop::PatchBytes(c.call, CallBytes(c.call, c.original), CallBytes(c.call, ours))) {
        Log(LogLevel::Warn, "aspect_experiment: call at 0x{:X} does not match", c.call);
        ok = false;
      }
    }
  }
  Log(LogLevel::Info, "aspect_experiment: stage {} installed{}{}", stage,
      trace_screen_quads ? ", tracing wide screen quads" : "", ok ? "" : " (partly)");
  return ok;
}

}  // namespace thod2::core::patches
