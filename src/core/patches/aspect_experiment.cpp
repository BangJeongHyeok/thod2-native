#include "patches/aspect_experiment.h"

#include "log.h"
#include "patches/hud_call_sites.h"

#include <thod2/interop/hod2_addresses.h>
#include <thod2/interop/memory_patch.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <intrin.h>
#include <map>
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

// --- Wider 4:3 limits ---------------------------------------------------------
// Values the patched x87 operands point to instead of 320.0f, -320.0f and 0.5f.
float g_half_width = 320.0f;
float g_neg_half_width = -320.0f;
float g_half_ratio = 0.5f;

// --- HUD anchoring -----------------------------------------------------------
// Player: the same call draws the 1P element left of the centre and the 2P one
// right of it; each goes to its own screen edge.
enum class Anchor { None, Left, Right, Player };

// Shift that keeps a left (right) anchored element at the same distance from
// the left (right) screen edge: the virtual 4:3 view is this much wider per side.
float g_anchor_shift = 0.0f;

struct AnchoredCall {
  std::uint32_t call;
  Anchor anchor;
};
// From the trace of sprite call sites during the chapter 1 cut scene and
// gameplay (renderer.md "HUD 배치"). Everything else stays centred.
constexpr AnchoredCall kAnchoredCalls[] = {
    {0x40686C, Anchor::Player},  // 0x4067F0: sprite 0x900 at (160, 334)
    {0x40690C, Anchor::Player},  // 0x4068C0: sprite 0x8FF at x 42 / 384, y 412
    {0x4069FF, Anchor::Player},  // 0x406940: sprite 0x22E at x 82 / 424, y 427
    {0x406A86, Anchor::Player},  // 0x406940: sprite 0x62 at x 176 / 518, y 427
    {0x4175E7, Anchor::Player},  // 0x417570: life, sprite 0x5B8 at (32, 422)
    {0x417631, Anchor::Player},  // 0x417570: life, sprite 0x5B9 at (28, 412)
    {0x417741, Anchor::Player},  // 0x417570: life icons 0x6C4.. at x 60..156, y 412
    {0x417AFC, Anchor::Player},  // 0x4178A0: bullets 0xA74 at x 24..96, y 364
    {0x417ECD, Anchor::Player},  // 0x4178A0: bullets 0xA74 at x 24..132, y 364
};

Anchor AnchorOf(std::uintptr_t call) {
  for (const AnchoredCall& c : kAnchoredCalls) {
    if (c.call == call) {
      return c.anchor;
    }
  }
  return Anchor::None;
}

// Logs up to 4 distinct (sprite, x, y) per call site.
std::map<std::uint32_t, std::set<std::uint64_t>> g_sprite_trace;

void TraceSpriteCall(std::uintptr_t call, std::uint32_t sprite, float x, float y) {
  auto& seen = g_sprite_trace[static_cast<std::uint32_t>(call)];
  const std::uint64_t key = (static_cast<std::uint64_t>(sprite) << 32) ^
                            (static_cast<std::uint64_t>(static_cast<std::int32_t>(x)) << 16) ^
                            static_cast<std::uint64_t>(static_cast<std::int32_t>(y));
  if (seen.size() >= 4 || !seen.insert(key).second) {
    return;
  }
  Log(LogLevel::Info, "sprite_call: call 0x{:X} sprite 0x{:X} x {:.1f} y {:.1f}", call, sprite, x, y);
}

float AnchoredX(std::uintptr_t call, float x) {
  switch (AnchorOf(call)) {
    case Anchor::Left:
      return x - g_anchor_shift;
    case Anchor::Right:
      return x + g_anchor_shift;
    case Anchor::Player:
      return x < 320.0f ? x - g_anchor_shift : x + g_anchor_shift;
    case Anchor::None:
      break;
  }
  return x;
}

using DrawSprite2DFn = void(__cdecl*)(std::uint32_t* def);
using DrawSpriteAtFn = void(__cdecl*)(std::uint32_t sprite, float x, float y, float depth, float scale_x,
                                      float scale_y, std::uint32_t p7, std::uint32_t p8);

// Sprite definition passed to DrawSprite2D: +0 sprite, +4 x, +8 y (14 dwords read).
constexpr std::size_t kSpriteDefDwords = 14;

void __cdecl HookedDrawSprite2D(std::uint32_t* def) {
  const auto call = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - 5;
  std::array<std::uint32_t, kSpriteDefDwords> copy;
  std::memcpy(copy.data(), def, sizeof(copy));
  const float x = std::bit_cast<float>(copy[1]);
  if (g_trace) {
    TraceSpriteCall(call, copy[0], x, std::bit_cast<float>(copy[2]));
  }
  copy[1] = std::bit_cast<std::uint32_t>(AnchoredX(call, x));
  reinterpret_cast<DrawSprite2DFn>(hod2::kDrawSprite2D)(copy.data());
}

void __cdecl HookedDrawSpriteAt(std::uint32_t sprite, float x, float y, float depth, float scale_x,
                                float scale_y, std::uint32_t p7, std::uint32_t p8) {
  const auto call = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - 5;
  if (g_trace) {
    TraceSpriteCall(call, sprite, x, y);
  }
  reinterpret_cast<DrawSpriteAtFn>(hod2::kDrawSpriteAt)(sprite, AnchoredX(call, x), y, depth, scale_x,
                                                         scale_y, p7, p8);
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
  const float widen = kWideAspect / kOriginalAspect;
  if (stage >= 1) {
    g_half_width = 320.0f * widen;
    g_neg_half_width = -320.0f * widen;
    g_half_ratio = 0.5f * widen;
    const auto ptr = [](const float* f) {
      return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(f));
    };
    for (std::uintptr_t operand : hod2::kHalfWidthOperands) {
      ok &= interop::PatchUint32(operand, hod2::k320FloatConstant, ptr(&g_half_width));
    }
    for (std::uintptr_t operand : hod2::kNegHalfWidthOperands) {
      ok &= interop::PatchUint32(operand, hod2::kMinus320FloatConstant, ptr(&g_neg_half_width));
    }
    ok &= interop::PatchUint32(hod2::kIsInScreenRectHalfOperand, hod2::kHalfFloatConstant, ptr(&g_half_ratio));
    const auto max_x = static_cast<std::uint32_t>(std::lround(319.0f + 320.0f * (widen - 1.0f)));
    const auto min_x = static_cast<std::uint32_t>(-std::lround(320.0f * widen));
    for (std::uintptr_t immediate : hod2::kAimMaxXImmediates) {
      ok &= interop::PatchUint32(immediate, 0x13F, max_x);
    }
    for (std::uintptr_t immediate : hod2::kAimMinXImmediates) {
      ok &= interop::PatchUint32(immediate, 0xFFFFFEC0, min_x);
    }
    // Left of the 4:3 area the cursor x is negative: load it sign-extended
    // (movsx eax, word [esi+0x9C7240]; nop; nop).
    constexpr std::uint8_t kZeroExtend[] = {0x33, 0xC0, 0x66, 0x8B, 0x86, 0x40, 0x72, 0x9C, 0x00};
    constexpr std::uint8_t kSignExtend[] = {0x0F, 0xBF, 0x86, 0x40, 0x72, 0x9C, 0x00, 0x90, 0x90};
    for (std::uintptr_t load : hod2::kAimRawXLoads) {
      ok &= interop::PatchBytes(load, kZeroExtend, kSignExtend);
    }
    ok &= interop::PatchUint32(hod2::kMoveAimClampImmediate, std::bit_cast<std::uint32_t>(320.0f),
                               std::bit_cast<std::uint32_t>(g_half_width));
    ok &= interop::PatchUint32(hod2::kMoveAimClampNegImmediate, std::bit_cast<std::uint32_t>(-320.0f),
                               std::bit_cast<std::uint32_t>(g_neg_half_width));
  }
  if (g_squeeze || g_trace) {
    g_anchor_shift = g_squeeze ? 320.0f * (widen - 1.0f) : 0.0f;
    int patched = 0;
    const auto hook = [&](std::uintptr_t call, std::uintptr_t original, void* ours) {
      if (call == hod2::kDrawSpriteAtInnerCall) {
        return;
      }
      if (interop::PatchBytes(call, CallBytes(call, original), CallBytes(call, reinterpret_cast<std::uintptr_t>(ours)))) {
        ++patched;
      } else {
        Log(LogLevel::Warn, "aspect_experiment: sprite call at 0x{:X} does not match", call);
        ok = false;
      }
    };
    for (const auto& site : hud_sites::kDrawSprite2DCalls) {
      hook(site.call, hod2::kDrawSprite2D, reinterpret_cast<void*>(&HookedDrawSprite2D));
    }
    for (const auto& site : hud_sites::kDrawSpriteAtCalls) {
      hook(site.call, hod2::kDrawSpriteAt, reinterpret_cast<void*>(&HookedDrawSpriteAt));
    }
    Log(LogLevel::Info, "aspect_experiment: {} sprite call sites hooked, HUD shift {:.1f}", patched, g_anchor_shift);
  }
  Log(LogLevel::Info, "aspect_experiment: stage {} installed{}{}", stage,
      trace_screen_quads ? ", tracing wide screen quads" : "", ok ? "" : " (partly)");
  return ok;
}

}  // namespace thod2::core::patches
