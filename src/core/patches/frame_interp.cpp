#include "patches/frame_interp.h"

#include "log.h"

#include <thod2/interop/hod2_addresses.h>
#include <thod2/interop/memory_patch.h>
#include <thod2/interop/vtable_hook.h>

#include <windows.h>

#include <ddraw.h>
#include <d3d.h>

#include <intrin.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma intrinsic(_ReturnAddress, _AddressOfReturnAddress)

namespace thod2::core::patches {
namespace {

namespace hod2 = interop::hod2;

// IDirect3DDevice7 vtable indices (d3d.h).
constexpr std::size_t kDev_BeginScene = 5;
constexpr std::size_t kDev_EndScene = 6;
constexpr std::size_t kDev_Clear = 10;
constexpr std::size_t kDev_SetTransform = 11;
constexpr std::size_t kDev_SetViewport = 13;
constexpr std::size_t kDev_SetMaterial = 16;
constexpr std::size_t kDev_SetLight = 18;
constexpr std::size_t kDev_SetRenderState = 20;
constexpr std::size_t kDev_DrawPrimitive = 25;
constexpr std::size_t kDev_DrawIndexedPrimitive = 26;
constexpr std::size_t kDev_SetTexture = 35;
constexpr std::size_t kDev_SetTextureStageState = 37;
constexpr std::size_t kDev_LightEnable = 44;

// ID3DXContext vtable indices (d3dxcore.h of the DirectX 7 SDK). The game
// calls UpdateFrame at +0x4C, Clear at +0x54 and SetClearColor at +0x58.
constexpr std::size_t kCtx_RestoreSurfaces = 17;
constexpr std::size_t kCtx_UpdateFrame = 19;
constexpr std::size_t kCtx_Clear = 21;
constexpr std::size_t kCtx_SetClearColor = 22;
// IDirectDraw7::TestCooperativeLevel (+0x68), called by WaitFrameEnd after a lost frame.
constexpr std::size_t kDD_TestCooperativeLevel = 26;

constexpr HRESULT kSurfaceLost = static_cast<HRESULT>(0x887601C2);    // DDERR_SURFACELOST
constexpr HRESULT kNoExclusiveMode = static_cast<HRESULT>(0x887601AE);  // DDERR_NOEXCLUSIVEMODE

// More than this many ticks behind (a loading stall): restart the tick clock
// instead of catching up.
constexpr int kResyncTicks = 8;
// WaitFrameEnd skips the drawing of at most 4 ticks ([0x597438]) after a late frame.
constexpr int kCatchUpTicks = 4;
constexpr ULONGLONG kTraceIntervalMs = 5000;

using BeginSceneFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*);
using ClearFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, DWORD, LPD3DRECT, DWORD, D3DCOLOR,
                                            D3DVALUE, DWORD);
using SetTransformFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, D3DTRANSFORMSTATETYPE,
                                                   LPD3DMATRIX);
using SetViewportFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, LPD3DVIEWPORT7);
using SetMaterialFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, LPD3DMATERIAL7);
using SetLightFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, DWORD, LPD3DLIGHT7);
using SetRenderStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, D3DRENDERSTATETYPE, DWORD);
using DrawPrimitiveFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, D3DPRIMITIVETYPE, DWORD,
                                                    LPVOID, DWORD, DWORD);
using DrawIndexedPrimitiveFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, D3DPRIMITIVETYPE,
                                                           DWORD, LPVOID, DWORD, LPWORD, DWORD,
                                                           DWORD);
using SetTextureFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, DWORD, LPDIRECTDRAWSURFACE7);
using SetTextureStageStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, DWORD,
                                                           D3DTEXTURESTAGESTATETYPE, DWORD);
using LightEnableFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice7*, DWORD, BOOL);

using CtxDwordFn = HRESULT(STDMETHODCALLTYPE*)(void*, DWORD);
using ComVoidFn = HRESULT(STDMETHODCALLTYPE*)(void*);
using WaitFrameEndFn = bool(__cdecl*)();
using FrameWaitCallbackFn = void(__cdecl*)(float remaining_ms);

BeginSceneFn o_BeginScene = nullptr;
BeginSceneFn o_EndScene = nullptr;
ClearFn o_Clear = nullptr;
SetTransformFn o_SetTransform = nullptr;
SetViewportFn o_SetViewport = nullptr;
SetMaterialFn o_SetMaterial = nullptr;
SetLightFn o_SetLight = nullptr;
SetRenderStateFn o_SetRenderState = nullptr;
DrawPrimitiveFn o_DrawPrimitive = nullptr;
DrawIndexedPrimitiveFn o_DrawIndexedPrimitive = nullptr;
SetTextureFn o_SetTexture = nullptr;
SetTextureStageStateFn o_SetTextureStageState = nullptr;
LightEnableFn o_LightEnable = nullptr;
CtxDwordFn o_CtxClear = nullptr;
CtxDwordFn o_CtxSetClearColor = nullptr;

enum class Op : std::uint8_t {
  BeginScene,
  EndScene,
  Clear,
  CtxClear,
  SetTransform,
  SetViewport,
  SetMaterial,
  SetLight,
  SetRenderState,
  DrawPrimitive,
  DrawIndexedPrimitive,
  SetTexture,
  SetTextureStageState,
  LightEnable,
};

struct Cmd {
  Op op;
  std::uint32_t a = 0, b = 0, c = 0, d = 0;
  float f = 0.0f;
  std::uint32_t offset = 0;  // into Frame::data, or Frame::matrices for SetTransform
};

struct MatrixSlot {
  std::uint32_t state;
  std::array<float, 16> value;
  std::int32_t prev = -1;  // index of the same matrix in the previous tick
};

// The Direct3D calls of one tick.
struct Frame {
  std::vector<Cmd> cmds;
  std::vector<std::uint8_t> data;
  std::vector<MatrixSlot> matrices;
  std::vector<IDirectDrawSurface7*> textures;  // referenced until the frame is cleared
  // (transform state << 32 | model) -> matrix indices in call order.
  std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> by_key;
  bool has_scene = false;
  std::uint64_t tick = 0;

  void Clear() {
    for (auto* t : textures) {
      t->Release();
    }
    cmds.clear();
    data.clear();
    matrices.clear();
    textures.clear();
    by_key.clear();
    has_scene = false;
  }

  std::uint32_t Push(const void* bytes, std::size_t size) {
    const auto offset = static_cast<std::uint32_t>(data.size());
    const auto* p = static_cast<const std::uint8_t*>(bytes);
    data.insert(data.end(), p, p + size);
    return offset;
  }
};

// Device state set through the hooks. The game caches render states, texture
// stage states and the texture (0x7E5B94, 0x7E5BAC, 0x7DCF50) and only sets
// them when they change, so a tick relies on the state the previous tick left.
// A replay starts from the end of its own tick instead: every recorded tick
// therefore begins with the state at its start.
struct DeviceState {
  static constexpr std::size_t kRenderStates = 256, kStages = 8, kStageStates = 32;
  std::array<DWORD, kRenderStates> render{};
  std::array<bool, kRenderStates> render_set{};
  std::array<std::array<DWORD, kStageStates>, kStages> stage{};
  std::array<std::array<bool, kStageStates>, kStages> stage_set{};
  std::array<IDirectDrawSurface7*, kStages> texture{};  // referenced while set
  std::array<bool, kStages> texture_set{};
  bool material_set = false;
  D3DMATERIAL7 material{};
  bool viewport_set = false;
  D3DVIEWPORT7 viewport{};
  std::vector<std::pair<DWORD, D3DLIGHT7>> lights;
  std::vector<std::pair<DWORD, BOOL>> light_enable;
};

FrameInterpOptions g_options;
DeviceState g_state;
Frame g_frames[3];
Frame* g_record = &g_frames[0];
Frame* g_cur = &g_frames[1];
Frame* g_prev = &g_frames[2];
std::uint64_t g_tick = 0;
bool g_armed = false;
// Set while we replay or present: hooked calls then go straight to Direct3D.
bool g_passthrough = false;
D3DCOLOR g_clear_color = 0;

LARGE_INTEGER g_freq{};
LONGLONG g_next_tick = 0;
LONGLONG g_last_present = 0;
HANDLE g_timer = nullptr;

struct Stats {
  std::uint64_t ticks = 0, presents = 0, skipped = 0, resyncs = 0, fallback = 0;
  std::uint64_t matrices = 0, linked = 0, snapped = 0;
  // Matrices paired with another draw of the same model than by call order.
  std::uint64_t reordered = 0;
  // Pairs dropped because another model took the previous place.
  std::uint64_t replaced = 0;
  // Ticks whose start state differs from the state at their end.
  std::uint64_t state_changed = 0;
  // Translation change of linked matrices: < 0.01, < 0.1, < 1, < 10, >= 10.
  std::array<std::uint64_t, 5> delta{};
  ULONGLONG since = 0;
} g_stats;

IDirect3DDevice7* Device() { return *reinterpret_cast<IDirect3DDevice7**>(hod2::kD3dDevice); }
void* Context() { return *reinterpret_cast<void**>(hod2::kD3dxContext); }

template <class Fn>
Fn VtableEntry(void* object, std::size_t index) {
  return reinterpret_cast<Fn>((*static_cast<void***>(object))[index]);
}

LONGLONG Now() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

std::uint32_t FvfStride(DWORD fvf) {
  std::uint32_t size = 0;
  switch (fvf & 0x00E) {  // D3DFVF_POSITION_MASK
    case 0x002: size = 12; break;  // XYZ
    case 0x004: size = 16; break;  // XYZRHW
    case 0x006: size = 16; break;  // XYZB1
    case 0x008: size = 20; break;  // XYZB2
    case 0x00A: size = 24; break;  // XYZB3
    case 0x00C: size = 28; break;  // XYZB4
    case 0x00E: size = 32; break;  // XYZB5
  }
  if (fvf & 0x010) size += 12;  // NORMAL
  if (fvf & 0x020) size += 4;   // RESERVED1
  if (fvf & 0x040) size += 4;   // DIFFUSE
  if (fvf & 0x080) size += 4;   // SPECULAR
  const DWORD tex_count = (fvf >> 8) & 0xF;
  for (DWORD i = 0; i < tex_count; ++i) {
    static constexpr std::uint32_t kTexSize[] = {8, 12, 16, 4};
    size += kTexSize[(fvf >> (16 + 2 * i)) & 3];
  }
  return size;
}

// Matrix key: transform state, model, and whether the draw is the second one
// of a translucent model (sorted by depth in FlushSortedDraws).
constexpr std::uint64_t kSortedPass = 1ull << 48;
constexpr std::uint64_t kStateKeyMask = (1ull << 48) - 1;

// SetTransform(WORLD) is the first call of DrawModel / DrawModelAlt, with
// record + 0x34; record + 0xC is the model. Their arguments are (record, pass):
// pass is 0 from SubmitModel and 1 from FlushSortedDraws. At the call, the
// stack holds 0x38 bytes of DrawModel below its return address.
std::uint64_t KeyOf(D3DTRANSFORMSTATETYPE state, std::uintptr_t caller, const std::uint32_t* return_slot,
                    const D3DMATRIX* matrix) {
  const std::uint64_t key = static_cast<std::uint64_t>(state) << 32;
  const bool from_model = (caller >= hod2::kDrawModel && caller < hod2::kDrawModel + hod2::kDrawModelSize) ||
                          (caller >= hod2::kDrawModelAlt &&
                           caller < hod2::kDrawModelAlt + hod2::kDrawModelAltSize);
  if (!from_model) {
    return key;
  }
  const auto record = reinterpret_cast<std::uintptr_t>(matrix) - 0x34;
  const std::uint32_t model = *reinterpret_cast<const std::uint32_t*>(record + 0xC);
  const std::uint32_t* args = return_slot + 0x38 / 4 + 1;
  const bool sorted = args[0] == record && args[1] != 0;
  return key | model | (sorted ? kSortedPass : 0);
}

// ---- hooks: while armed, record the call; state changes also go through so
// that the game's own queries (ComputeSphereVisibility) see them ----

HRESULT STDMETHODCALLTYPE H_BeginScene(IDirect3DDevice7* self) {
  if (g_passthrough || !g_armed) return o_BeginScene(self);
  g_record->cmds.push_back({Op::BeginScene});
  g_record->has_scene = true;
  return D3D_OK;
}

HRESULT STDMETHODCALLTYPE H_EndScene(IDirect3DDevice7* self) {
  if (g_passthrough || !g_armed) return o_EndScene(self);
  g_record->cmds.push_back({Op::EndScene});
  return D3D_OK;
}

HRESULT STDMETHODCALLTYPE H_Clear(IDirect3DDevice7* self, DWORD count, LPD3DRECT rects, DWORD flags,
                                  D3DCOLOR color, D3DVALUE z, DWORD stencil) {
  if (g_passthrough || !g_armed) return o_Clear(self, count, rects, flags, color, z, stencil);
  Cmd cmd{Op::Clear, count, flags, color, stencil, z};
  if (rects && count) {
    cmd.offset = g_record->Push(rects, sizeof(D3DRECT) * count);
  } else {
    cmd.a = 0;
  }
  g_record->cmds.push_back(cmd);
  return D3D_OK;
}

HRESULT STDMETHODCALLTYPE H_SetTransform(IDirect3DDevice7* self, D3DTRANSFORMSTATETYPE state,
                                         LPD3DMATRIX matrix) {
  if (g_passthrough || !g_armed || !matrix) return o_SetTransform(self, state, matrix);
  const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
  Frame& f = *g_record;
  const auto index = static_cast<std::uint32_t>(f.matrices.size());
  MatrixSlot slot{static_cast<std::uint32_t>(state)};
  std::memcpy(slot.value.data(), matrix, sizeof(slot.value));
  f.matrices.push_back(slot);
  const auto* return_slot = static_cast<const std::uint32_t*>(_AddressOfReturnAddress());
  f.by_key[KeyOf(state, caller, return_slot, matrix)].push_back(index);
  f.cmds.push_back({Op::SetTransform, static_cast<std::uint32_t>(state), 0, 0, 0, 0.0f, index});
  return o_SetTransform(self, state, matrix);
}

template <class T>
void SetIndexed(std::vector<std::pair<DWORD, T>>& list, DWORD index, const T& value) {
  for (auto& [i, v] : list) {
    if (i == index) {
      v = value;
      return;
    }
  }
  list.emplace_back(index, value);
}

HRESULT STDMETHODCALLTYPE H_SetViewport(IDirect3DDevice7* self, LPD3DVIEWPORT7 viewport) {
  if (g_passthrough || !g_armed || !viewport) return o_SetViewport(self, viewport);
  Cmd cmd{Op::SetViewport};
  cmd.offset = g_record->Push(viewport, sizeof(*viewport));
  g_record->cmds.push_back(cmd);
  g_state.viewport_set = true;
  g_state.viewport = *viewport;
  return o_SetViewport(self, viewport);
}

HRESULT STDMETHODCALLTYPE H_SetMaterial(IDirect3DDevice7* self, LPD3DMATERIAL7 material) {
  if (g_passthrough || !g_armed || !material) return o_SetMaterial(self, material);
  Cmd cmd{Op::SetMaterial};
  cmd.offset = g_record->Push(material, sizeof(*material));
  g_record->cmds.push_back(cmd);
  g_state.material_set = true;
  g_state.material = *material;
  return o_SetMaterial(self, material);
}

HRESULT STDMETHODCALLTYPE H_SetLight(IDirect3DDevice7* self, DWORD index, LPD3DLIGHT7 light) {
  if (g_passthrough || !g_armed || !light) return o_SetLight(self, index, light);
  Cmd cmd{Op::SetLight, index};
  cmd.offset = g_record->Push(light, sizeof(*light));
  g_record->cmds.push_back(cmd);
  SetIndexed(g_state.lights, index, *light);
  return o_SetLight(self, index, light);
}

HRESULT STDMETHODCALLTYPE H_SetRenderState(IDirect3DDevice7* self, D3DRENDERSTATETYPE state,
                                           DWORD value) {
  if (g_passthrough || !g_armed) return o_SetRenderState(self, state, value);
  g_record->cmds.push_back({Op::SetRenderState, static_cast<std::uint32_t>(state), value});
  if (static_cast<std::size_t>(state) < DeviceState::kRenderStates) {
    g_state.render[state] = value;
    g_state.render_set[state] = true;
  }
  return o_SetRenderState(self, state, value);
}

HRESULT STDMETHODCALLTYPE H_DrawPrimitive(IDirect3DDevice7* self, D3DPRIMITIVETYPE type, DWORD fvf,
                                          LPVOID vertices, DWORD count, DWORD flags) {
  if (g_passthrough || !g_armed) return o_DrawPrimitive(self, type, fvf, vertices, count, flags);
  if (!vertices || count == 0) return D3D_OK;
  Cmd cmd{Op::DrawPrimitive, static_cast<std::uint32_t>(type), fvf, count, flags};
  cmd.offset = g_record->Push(vertices, static_cast<std::size_t>(FvfStride(fvf)) * count);
  g_record->cmds.push_back(cmd);
  return D3D_OK;
}

HRESULT STDMETHODCALLTYPE H_DrawIndexedPrimitive(IDirect3DDevice7* self, D3DPRIMITIVETYPE type,
                                                 DWORD fvf, LPVOID vertices, DWORD vertex_count,
                                                 LPWORD indices, DWORD index_count, DWORD flags) {
  if (g_passthrough || !g_armed) {
    return o_DrawIndexedPrimitive(self, type, fvf, vertices, vertex_count, indices, index_count, flags);
  }
  if (!vertices || !indices || vertex_count == 0 || index_count == 0) return D3D_OK;
  Cmd cmd{Op::DrawIndexedPrimitive, static_cast<std::uint32_t>(type), fvf, vertex_count, flags};
  cmd.f = std::bit_cast<float>(index_count);
  cmd.offset = g_record->Push(vertices, static_cast<std::size_t>(FvfStride(fvf)) * vertex_count);
  g_record->Push(indices, sizeof(WORD) * index_count);
  g_record->cmds.push_back(cmd);
  return D3D_OK;
}

HRESULT STDMETHODCALLTYPE H_SetTexture(IDirect3DDevice7* self, DWORD stage,
                                       LPDIRECTDRAWSURFACE7 texture) {
  if (g_passthrough || !g_armed) return o_SetTexture(self, stage, texture);
  if (texture) {
    texture->AddRef();
    g_record->textures.push_back(texture);
  }
  g_record->cmds.push_back(
      {Op::SetTexture, stage, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(texture))});
  if (stage < DeviceState::kStages) {
    if (texture) texture->AddRef();
    if (g_state.texture[stage]) g_state.texture[stage]->Release();
    g_state.texture[stage] = texture;
    g_state.texture_set[stage] = true;
  }
  return o_SetTexture(self, stage, texture);
}

HRESULT STDMETHODCALLTYPE H_SetTextureStageState(IDirect3DDevice7* self, DWORD stage,
                                                 D3DTEXTURESTAGESTATETYPE state, DWORD value) {
  if (g_passthrough || !g_armed) return o_SetTextureStageState(self, stage, state, value);
  g_record->cmds.push_back(
      {Op::SetTextureStageState, stage, static_cast<std::uint32_t>(state), value});
  if (stage < DeviceState::kStages && static_cast<std::size_t>(state) < DeviceState::kStageStates) {
    g_state.stage[stage][state] = value;
    g_state.stage_set[stage][state] = true;
  }
  return o_SetTextureStageState(self, stage, state, value);
}

HRESULT STDMETHODCALLTYPE H_LightEnable(IDirect3DDevice7* self, DWORD index, BOOL enable) {
  if (g_passthrough || !g_armed) return o_LightEnable(self, index, enable);
  g_record->cmds.push_back({Op::LightEnable, index, static_cast<std::uint32_t>(enable)});
  SetIndexed(g_state.light_enable, index, enable);
  return o_LightEnable(self, index, enable);
}

HRESULT STDMETHODCALLTYPE H_CtxClear(void* self, DWORD flags) {
  if (g_passthrough || !g_armed) return o_CtxClear(self, flags);
  g_record->cmds.push_back({Op::CtxClear, 0, flags, g_clear_color});
  return S_OK;
}

HRESULT STDMETHODCALLTYPE H_CtxSetClearColor(void* self, DWORD color) {
  if (!g_passthrough) g_clear_color = color;
  return o_CtxSetClearColor(self, color);
}

template <class Fn>
void Hook(void* object, std::size_t index, void* replacement, Fn& original) {
  void** vtable = *static_cast<void***>(object);
  if (vtable[index] == replacement) {
    return;
  }
  original = reinterpret_cast<Fn>(interop::HookVtable(object, index, replacement));
}

bool Arm() {
  IDirect3DDevice7* device = Device();
  void* context = Context();
  if (!device || !context) {
    return false;
  }
  Hook(device, kDev_BeginScene, reinterpret_cast<void*>(&H_BeginScene), o_BeginScene);
  Hook(device, kDev_EndScene, reinterpret_cast<void*>(&H_EndScene), o_EndScene);
  Hook(device, kDev_Clear, reinterpret_cast<void*>(&H_Clear), o_Clear);
  Hook(device, kDev_SetTransform, reinterpret_cast<void*>(&H_SetTransform), o_SetTransform);
  Hook(device, kDev_SetViewport, reinterpret_cast<void*>(&H_SetViewport), o_SetViewport);
  Hook(device, kDev_SetMaterial, reinterpret_cast<void*>(&H_SetMaterial), o_SetMaterial);
  Hook(device, kDev_SetLight, reinterpret_cast<void*>(&H_SetLight), o_SetLight);
  Hook(device, kDev_SetRenderState, reinterpret_cast<void*>(&H_SetRenderState), o_SetRenderState);
  Hook(device, kDev_DrawPrimitive, reinterpret_cast<void*>(&H_DrawPrimitive), o_DrawPrimitive);
  Hook(device, kDev_DrawIndexedPrimitive, reinterpret_cast<void*>(&H_DrawIndexedPrimitive),
       o_DrawIndexedPrimitive);
  Hook(device, kDev_SetTexture, reinterpret_cast<void*>(&H_SetTexture), o_SetTexture);
  Hook(device, kDev_SetTextureStageState, reinterpret_cast<void*>(&H_SetTextureStageState),
       o_SetTextureStageState);
  Hook(device, kDev_LightEnable, reinterpret_cast<void*>(&H_LightEnable), o_LightEnable);
  Hook(context, kCtx_Clear, reinterpret_cast<void*>(&H_CtxClear), o_CtxClear);
  Hook(context, kCtx_SetClearColor, reinterpret_cast<void*>(&H_CtxSetClearColor), o_CtxSetClearColor);
  const bool ok = o_BeginScene && o_EndScene && o_Clear && o_SetTransform && o_SetViewport &&
                  o_SetMaterial && o_SetLight && o_SetRenderState && o_DrawPrimitive &&
                  o_DrawIndexedPrimitive && o_SetTexture && o_SetTextureStageState && o_LightEnable &&
                  o_CtxClear && o_CtxSetClearColor;
  Log(ok ? LogLevel::Info : LogLevel::Error, "frame_interp: device hooks {} (device {}, context {})",
      ok ? "installed" : "failed", static_cast<void*>(device), context);
  return ok;
}

// Above this many candidate pairs for one model, pair by call order.
constexpr std::size_t kMaxPairs = 16384;

float MatrixDistance(const std::array<float, 16>& a, const std::array<float, 16>& b) {
  float sum = 0.0f;
  for (int i = 0; i < 15; ++i) {
    const float d = a[i] - b[i];
    sum += d * d;
  }
  return sum;
}

std::uint64_t PositionHash(const std::array<float, 16>& m) {
  std::uint64_t h = 1469598103934665603ull;
  for (int i = 12; i < 15; ++i) {
    h = (h ^ std::bit_cast<std::uint32_t>(m[i])) * 1099511628211ull;
  }
  return h;
}

std::uint64_t MatrixHash(std::uint64_t key, const std::array<float, 16>& m) {
  std::uint64_t h = 1469598103934665603ull ^ key;
  for (const float v : m) {
    h = (h ^ std::bit_cast<std::uint32_t>(v)) * 1099511628211ull;
  }
  return h;
}

// Pairs every matrix of the current tick with the same matrix of the previous
// tick, among the draws of the same transform state and model:
// - first draws (SubmitModel): a draw that did not move pairs with its exact
//   copy; the rest come in the order of the game's objects, so they pair by
//   call order while their number stays the same, and by distance (closest
//   pairs first) when objects appeared or went away;
// - the second draw of a translucent model (FlushSortedDraws, sorted by depth)
//   repeats the matrix of its first draw and follows that draw's pair.
//
// A pair is dropped when another model now sits exactly where the previous
// matrix was: score digits are one model per digit value at fixed places, so
// when "39" becomes "40" the old 4 must not slide into its new place.
void Link(Frame& cur, const Frame& prev) {
  if (prev.tick + 1 != cur.tick) {
    return;
  }
  constexpr std::uint64_t kSeveral = ~0ull;
  std::unordered_map<std::uint64_t, std::uint64_t> taken;  // position -> key there
  std::unordered_map<std::uint64_t, std::uint32_t> first_draws;  // matrix -> index
  taken.reserve(cur.matrices.size());
  for (const auto& [key, indices] : cur.by_key) {
    const std::uint64_t place_key = key & kStateKeyMask;
    for (const std::uint32_t i : indices) {
      const auto [it, inserted] = taken.emplace(PositionHash(cur.matrices[i].value), place_key);
      if (!inserted && it->second != place_key) it->second = kSeveral;
    }
  }
  const auto pair_with = [&](std::uint64_t key, MatrixSlot& slot, std::uint32_t p) {
    const auto& from = prev.matrices[p].value;
    if (from[12] != slot.value[12] || from[13] != slot.value[13] || from[14] != slot.value[14]) {
      const auto it = taken.find(PositionHash(from));
      if (it != taken.end() && it->second != (key & kStateKeyMask)) {
        ++g_stats.replaced;
        return;
      }
    }
    slot.prev = static_cast<std::int32_t>(p);
  };

  struct Pair {
    float cost;
    std::uint32_t c, p;
  };
  std::vector<Pair> pairs;
  std::vector<bool> used, done;
  const auto pair_closest = [&](std::uint64_t key, const std::vector<std::uint32_t>& indices,
                                const std::vector<std::uint32_t>& candidates) {
    if (indices.size() * candidates.size() > kMaxPairs) {
      const std::size_t n = std::min(indices.size(), candidates.size());
      for (std::size_t i = 0; i < n; ++i) {
        pair_with(key, cur.matrices[indices[i]], candidates[i]);
      }
      return;
    }
    pairs.clear();
    for (std::uint32_t c = 0; c < indices.size(); ++c) {
      for (std::uint32_t p = 0; p < candidates.size(); ++p) {
        pairs.push_back({MatrixDistance(cur.matrices[indices[c]].value, prev.matrices[candidates[p]].value), c, p});
      }
    }
    std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) { return a.cost < b.cost; });
    used.assign(candidates.size(), false);
    done.assign(indices.size(), false);
    for (const Pair& pair : pairs) {
      if (done[pair.c] || used[pair.p]) continue;
      used[pair.p] = true;
      done[pair.c] = true;
      if (pair.c != pair.p) ++g_stats.reordered;
      pair_with(key, cur.matrices[indices[pair.c]], candidates[pair.p]);
    }
  };

  std::unordered_multimap<std::uint64_t, std::uint32_t> still;  // matrix -> position in candidates
  std::vector<std::uint32_t> rest_cur, rest_prev;
  for (const auto& [key, indices] : cur.by_key) {
    if (key & kSortedPass) continue;
    const auto it = prev.by_key.find(key);
    if (it != prev.by_key.end()) {
      const auto& candidates = it->second;
      // Draws that did not move (text, still objects) pair with their exact
      // copy, so that one appearing or going away does not shift the others.
      still.clear();
      for (std::uint32_t p = 0; p < candidates.size(); ++p) {
        still.emplace(MatrixHash(key, prev.matrices[candidates[p]].value), p);
      }
      used.assign(candidates.size(), false);
      rest_cur.clear();
      for (const std::uint32_t i : indices) {
        MatrixSlot& slot = cur.matrices[i];
        bool paired = false;
        const auto [first, last] = still.equal_range(MatrixHash(key, slot.value));
        for (auto s = first; s != last; ++s) {
          if (!used[s->second] && prev.matrices[candidates[s->second]].value == slot.value) {
            used[s->second] = true;
            slot.prev = static_cast<std::int32_t>(candidates[s->second]);
            paired = true;
            break;
          }
        }
        if (!paired) rest_cur.push_back(i);
      }
      rest_prev.clear();
      for (std::uint32_t p = 0; p < candidates.size(); ++p) {
        if (!used[p]) rest_prev.push_back(candidates[p]);
      }
      if (rest_cur.size() == rest_prev.size()) {
        for (std::size_t i = 0; i < rest_cur.size(); ++i) {
          pair_with(key, cur.matrices[rest_cur[i]], rest_prev[i]);
        }
      } else if (!rest_cur.empty() && !rest_prev.empty()) {
        pair_closest(key, rest_cur, rest_prev);
      }
    }
    for (const std::uint32_t i : indices) {
      first_draws.emplace(MatrixHash(key, cur.matrices[i].value), i);
    }
  }
  for (const auto& [key, indices] : cur.by_key) {
    if (!(key & kSortedPass)) continue;
    std::vector<std::uint32_t> unmatched;
    for (const std::uint32_t i : indices) {
      MatrixSlot& slot = cur.matrices[i];
      const auto first = first_draws.find(MatrixHash(key & ~kSortedPass, slot.value));
      if (first != first_draws.end() && cur.matrices[first->second].value == slot.value) {
        slot.prev = cur.matrices[first->second].prev;
      } else {
        unmatched.push_back(i);
      }
    }
    const auto it = prev.by_key.find(key);
    if (!unmatched.empty() && it != prev.by_key.end()) {
      pair_closest(key, unmatched, it->second);
    }
  }
}

// Commands that put the device into `state`, at the start of a recorded tick.
void EmitState(Frame& f, const DeviceState& state) {
  if (state.viewport_set) {
    Cmd cmd{Op::SetViewport};
    cmd.offset = f.Push(&state.viewport, sizeof(state.viewport));
    f.cmds.push_back(cmd);
  }
  if (state.material_set) {
    Cmd cmd{Op::SetMaterial};
    cmd.offset = f.Push(&state.material, sizeof(state.material));
    f.cmds.push_back(cmd);
  }
  for (const auto& [index, light] : state.lights) {
    Cmd cmd{Op::SetLight, index};
    cmd.offset = f.Push(&light, sizeof(light));
    f.cmds.push_back(cmd);
  }
  for (const auto& [index, enable] : state.light_enable) {
    f.cmds.push_back({Op::LightEnable, index, static_cast<std::uint32_t>(enable)});
  }
  for (std::uint32_t i = 0; i < DeviceState::kRenderStates; ++i) {
    if (state.render_set[i]) f.cmds.push_back({Op::SetRenderState, i, state.render[i]});
  }
  for (std::uint32_t s = 0; s < DeviceState::kStages; ++s) {
    for (std::uint32_t i = 0; i < DeviceState::kStageStates; ++i) {
      if (state.stage_set[s][i]) f.cmds.push_back({Op::SetTextureStageState, s, i, state.stage[s][i]});
    }
    if (state.texture_set[s]) {
      IDirectDrawSurface7* texture = state.texture[s];
      if (texture) {
        texture->AddRef();
        f.textures.push_back(texture);
      }
      f.cmds.push_back(
          {Op::SetTexture, s, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(texture))});
    }
  }
}

bool SameState(const DeviceState& a, const DeviceState& b) {
  return a.render == b.render && a.render_set == b.render_set && a.stage == b.stage &&
         a.stage_set == b.stage_set && a.texture == b.texture && a.texture_set == b.texture_set &&
         std::memcmp(&a.material, &b.material, sizeof(a.material)) == 0;
}

DeviceState g_tick_start;  // compared with g_state when tracing (no references held)

void FinishTick() {
  ++g_tick;
  ++g_stats.ticks;
  if (g_options.trace && g_record->has_scene) {
    if (!SameState(g_tick_start, g_state)) ++g_stats.state_changed;
    g_tick_start = g_state;
  }
  if (!g_record->has_scene) {
    g_record->Clear();
    EmitState(*g_record, g_state);
    return;
  }
  g_record->tick = g_tick;
  std::swap(g_prev, g_cur);
  std::swap(g_cur, g_record);
  g_record->Clear();
  EmitState(*g_record, g_state);
  Link(*g_cur, *g_prev);
}

void Replay(float t) {
  IDirect3DDevice7* device = Device();
  void* context = Context();
  const Frame& f = *g_cur;
  const Frame& prev = *g_prev;
  const std::uint8_t* data = f.data.data();
  for (const Cmd& c : f.cmds) {
    switch (c.op) {
      case Op::BeginScene: o_BeginScene(device); break;
      case Op::EndScene: o_EndScene(device); break;
      case Op::Clear:
        o_Clear(device, c.a, c.a ? reinterpret_cast<LPD3DRECT>(const_cast<std::uint8_t*>(data + c.offset)) : nullptr,
                c.b, c.c, c.f, c.d);
        break;
      case Op::CtxClear:
        o_CtxSetClearColor(context, c.c);
        o_CtxClear(context, c.b);
        break;
      case Op::SetTransform: {
        const MatrixSlot& slot = f.matrices[c.offset];
        std::array<float, 16> m = slot.value;
        if (g_options.trace) ++g_stats.matrices;
        if (slot.prev >= 0) {
          const auto& from = prev.matrices[slot.prev].value;
          if (!LerpWorldViewMatrix(from.data(), slot.value.data(), t, g_options.snap_distance, m.data()) &&
              g_options.trace) {
            ++g_stats.snapped;
          }
          if (g_options.trace) {
            ++g_stats.linked;
            const float dx = slot.value[12] - from[12], dy = slot.value[13] - from[13],
                        dz = slot.value[14] - from[14];
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            const int bucket = dist < 0.01f ? 0 : dist < 0.1f ? 1 : dist < 1.0f ? 2 : dist < 10.0f ? 3 : 4;
            ++g_stats.delta[bucket];
          }
        }
        o_SetTransform(device, static_cast<D3DTRANSFORMSTATETYPE>(c.a), reinterpret_cast<LPD3DMATRIX>(m.data()));
        break;
      }
      case Op::SetViewport:
        o_SetViewport(device, reinterpret_cast<LPD3DVIEWPORT7>(const_cast<std::uint8_t*>(data + c.offset)));
        break;
      case Op::SetMaterial:
        o_SetMaterial(device, reinterpret_cast<LPD3DMATERIAL7>(const_cast<std::uint8_t*>(data + c.offset)));
        break;
      case Op::SetLight:
        o_SetLight(device, c.a, reinterpret_cast<LPD3DLIGHT7>(const_cast<std::uint8_t*>(data + c.offset)));
        break;
      case Op::SetRenderState:
        o_SetRenderState(device, static_cast<D3DRENDERSTATETYPE>(c.a), c.b);
        break;
      case Op::DrawPrimitive:
        o_DrawPrimitive(device, static_cast<D3DPRIMITIVETYPE>(c.a), c.b,
                        const_cast<std::uint8_t*>(data + c.offset), c.c, c.d);
        break;
      case Op::DrawIndexedPrimitive: {
        const auto index_count = std::bit_cast<std::uint32_t>(c.f);
        auto* vertices = const_cast<std::uint8_t*>(data + c.offset);
        auto* indices = reinterpret_cast<LPWORD>(vertices + static_cast<std::size_t>(FvfStride(c.b)) * c.c);
        o_DrawIndexedPrimitive(device, static_cast<D3DPRIMITIVETYPE>(c.a), c.b, vertices, c.c, indices,
                               index_count, c.d);
        break;
      }
      case Op::SetTexture:
        o_SetTexture(device, c.a, reinterpret_cast<LPDIRECTDRAWSURFACE7>(static_cast<std::uintptr_t>(c.b)));
        break;
      case Op::SetTextureStageState:
        o_SetTextureStageState(device, c.a, static_cast<D3DTEXTURESTAGESTATETYPE>(c.b), c.c);
        break;
      case Op::LightEnable:
        o_LightEnable(device, c.a, static_cast<BOOL>(c.b));
        break;
    }
  }
  o_CtxSetClearColor(context, g_clear_color);
}

// Same presentation as the start of WaitFrameEnd: UpdateFrame (the flip), and
// on a lost surface TestCooperativeLevel then RestoreSurfaces.
void Present() {
  void* context = Context();
  const HRESULT hr = VtableEntry<CtxDwordFn>(context, kCtx_UpdateFrame)(context, 0);
  if (hr == kSurfaceLost || hr == kNoExclusiveMode) {
    void* dd = *reinterpret_cast<void**>(hod2::kDirectDraw);
    if (dd && VtableEntry<ComVoidFn>(dd, kDD_TestCooperativeLevel)(dd) >= 0) {
      VtableEntry<ComVoidFn>(context, kCtx_RestoreSurfaces)(context);
    }
  }
}

void SleepUntil(LONGLONG deadline) {
  const LONGLONG now = Now();
  if (deadline <= now) return;
  const double ms = static_cast<double>(deadline - now) * 1000.0 / static_cast<double>(g_freq.QuadPart);
  if (g_timer && ms > 0.5) {
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>((ms - 0.3) * 10000.0);
    if (SetWaitableTimer(g_timer, &due, 0, nullptr, nullptr, FALSE)) {
      WaitForSingleObject(g_timer, INFINITE);
    }
  }
  while (Now() < deadline) {
    YieldProcessor();
  }
}

bool JobsQueued() {
  return *reinterpret_cast<volatile std::uint32_t*>(hod2::kJobQueueReadIndex) !=
         *reinterpret_cast<volatile std::uint32_t*>(hod2::kJobQueueWriteIndex);
}

void TraceStats() {
  const ULONGLONG now = GetTickCount64();
  if (g_stats.since == 0) {
    g_stats.since = now;
    return;
  }
  if (now - g_stats.since < kTraceIntervalMs) return;
  const double seconds = static_cast<double>(now - g_stats.since) / 1000.0;
  Log(LogLevel::Info,
      "frame_interp: {:.1f} ticks/s, {:.1f} presents/s, skipped {}, resyncs {}, fallback {}; "
      "matrices {} linked {} snapped {} reordered {} replaced {}; state changed in {} ticks; "
      "moved <0.01 {} <0.1 {} <1 {} <10 {} >=10 {}",
      g_stats.ticks / seconds, g_stats.presents / seconds, g_stats.skipped, g_stats.resyncs,
      g_stats.fallback, g_stats.matrices, g_stats.linked, g_stats.snapped, g_stats.reordered,
      g_stats.replaced, g_stats.state_changed, g_stats.delta[0], g_stats.delta[1], g_stats.delta[2], g_stats.delta[3],
      g_stats.delta[4]);
  g_stats = Stats{};
  g_stats.since = now;
}

bool OriginalWaitFrameEnd() {
  g_passthrough = true;
  const bool result = reinterpret_cast<WaitFrameEndFn>(hod2::kWaitFrameEnd)();
  g_passthrough = false;
  return result;
}

// Replaces the call to WaitFrameEnd at the end of every tick (0x4AB2F0).
bool __cdecl InterpWaitFrameEnd() {
  const float frame_ms = *reinterpret_cast<const float*>(hod2::kFrameTimeMs);
  if (!g_armed) {
    g_armed = Arm();
    return OriginalWaitFrameEnd();
  }
  FinishTick();
  if (g_options.trace) TraceStats();
  if (g_cur->cmds.empty() || frame_ms <= 0.0f) {
    ++g_stats.fallback;
    return OriginalWaitFrameEnd();
  }

  const double tick_qpc = static_cast<double>(g_freq.QuadPart) * frame_ms / 1000.0;
  const auto tick = static_cast<LONGLONG>(tick_qpc);
  auto* skip = reinterpret_cast<volatile std::int32_t*>(hod2::kFrameSkipCount);
  LONGLONG now = Now();
  if (g_next_tick == 0) {
    g_next_tick = now;
  }
  const LONGLONG late = now - g_next_tick;
  if (late > tick * kResyncTicks) {
    // Like the game's own frame skip, catch up at most kCatchUpTicks after a stall.
    ++g_stats.resyncs;
    g_next_tick = now - tick * kCatchUpTicks;
  }
  if (now - g_next_tick > tick) {
    // A full tick behind: run the next tick without drawing, like the game's own frame skip.
    ++g_stats.skipped;
    g_next_tick += tick;
    *skip = 1;
    return true;
  }
  *skip = 0;

  g_passthrough = true;
  const LONGLONG min_interval =
      g_options.max_fps > 0 ? g_freq.QuadPart / g_options.max_fps : 0;
  bool presented = false;
  for (;;) {
    now = Now();
    const double remaining_ms = static_cast<double>(g_next_tick - now) / tick_qpc * frame_ms;
    // Queued jobs (loading) get the time first, as in the game's own wait; the
    // tick is then shown once at its end instead of interpolated.
    if (JobsQueued() && remaining_ms > 1.0) {
      reinterpret_cast<FrameWaitCallbackFn>(hod2::kFrameWaitCallback)(static_cast<float>(remaining_ms));
      continue;
    }
    if (presented && now >= g_next_tick) break;
    if (min_interval && presented) SleepUntil(g_last_present + min_interval);
    const double t = 1.0 - static_cast<double>(g_next_tick - Now()) / tick_qpc;
    Replay(static_cast<float>(std::clamp(t, 0.0, 1.0)));
    Present();
    presented = true;
    g_last_present = Now();
    ++g_stats.presents;
    if (g_last_present >= g_next_tick) break;
  }
  g_passthrough = false;
  g_next_tick += tick;
  return true;
}

std::array<std::uint8_t, 5> CallBytes(std::uintptr_t from, std::uintptr_t to) {
  const auto rel = static_cast<std::uint32_t>(to - (from + 5));
  return {0xE8, static_cast<std::uint8_t>(rel), static_cast<std::uint8_t>(rel >> 8),
          static_cast<std::uint8_t>(rel >> 16), static_cast<std::uint8_t>(rel >> 24)};
}

}  // namespace

bool LerpWorldViewMatrix(const float* from, const float* to, float t, float snap_distance, float* out) {
  const float dx = to[12] - from[12], dy = to[13] - from[13], dz = to[14] - from[14];
  const float moved = std::sqrt(dx * dx + dy * dy + dz * dz);
  const float distance = std::sqrt(to[12] * to[12] + to[13] * to[13] + to[14] * to[14]);
  if (moved > snap_distance + 0.25f * distance) {
    return false;
  }
  // The rows of the upper 3x3 (rotation and scale) must point the same way.
  for (int row = 0; row < 3; ++row) {
    const float* a = from + row * 4;
    const float* b = to + row * 4;
    const float la = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    const float lb = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
    if (la < 1e-6f || lb < 1e-6f) {
      if (std::abs(la - lb) > 1e-6f) return false;
      continue;
    }
    const float cosine = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (la * lb);
    if (cosine < 0.9f || std::abs(la - lb) > 0.25f * lb) {
      return false;
    }
  }
  for (int i = 0; i < 16; ++i) {
    out[i] = from[i] + (to[i] - from[i]) * t;
  }
  return true;
}

bool InstallFrameInterp(const FrameInterpOptions& options) {
  g_options = options;
  QueryPerformanceFrequency(&g_freq);
  g_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                   TIMER_ALL_ACCESS);
  const auto expected = CallBytes(hod2::kWaitFrameEndCall, hod2::kWaitFrameEnd);
  const auto ours = CallBytes(hod2::kWaitFrameEndCall, reinterpret_cast<std::uintptr_t>(&InterpWaitFrameEnd));
  if (!interop::PatchBytes(hod2::kWaitFrameEndCall, expected, ours)) {
    Log(LogLevel::Warn, "frame_interp: call at 0x{:X} does not match, not installed", hod2::kWaitFrameEndCall);
    return false;
  }
  Log(LogLevel::Info, "frame_interp: installed (max_fps {}, snap_distance {:.2f})", options.max_fps,
      options.snap_distance);
  return true;
}

}  // namespace thod2::core::patches
