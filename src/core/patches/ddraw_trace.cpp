#include "patches/ddraw_trace.h"

#include "log.h"

#include <thod2/interop/iat_hook.h>
#include <thod2/interop/vtable_hook.h>

#include <windows.h>
#include <objbase.h>

#include <ddraw.h>
#include <d3d.h>

#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

#pragma intrinsic(_ReturnAddress)

namespace thod2::core::patches {
namespace {

// Vtable indices from ddraw.h (IDirectDraw7 / IDirectDrawSurface7).
constexpr std::size_t kDD_QueryInterface = 0;
constexpr std::size_t kDD_CreateSurface = 6;
constexpr std::size_t kDD_EnumDisplayModes = 8;
constexpr std::size_t kDD_FlipToGDISurface = 10;
constexpr std::size_t kDD_RestoreDisplayMode = 19;
constexpr std::size_t kDD_SetCooperativeLevel = 20;
constexpr std::size_t kDD_SetDisplayMode = 21;
constexpr std::size_t kDD_WaitForVerticalBlank = 22;
constexpr std::size_t kDD_RestoreAllSurfaces = 25;
constexpr std::size_t kDD_TestCooperativeLevel = 26;

constexpr std::size_t kSurf_Blt = 5;
constexpr std::size_t kSurf_BltFast = 7;
constexpr std::size_t kSurf_Flip = 11;
constexpr std::size_t kSurf_GetAttachedSurface = 12;
constexpr std::size_t kSurf_IsLost = 24;
constexpr std::size_t kSurf_Restore = 27;

// Detailed log lines per frequently called method before switching to summaries.
constexpr int kDetailedCalls = 3;
constexpr ULONGLONG kSummaryIntervalMs = 5000;

std::string ModuleOf(const void* address);

std::string Flags(DWORD value, std::initializer_list<std::pair<DWORD, const char*>> names) {
  std::string out;
  for (const auto& [bit, name] : names) {
    if (value & bit) {
      if (!out.empty()) out += '|';
      out += name;
      value &= ~bit;
    }
  }
  if (value) {
    if (!out.empty()) out += '|';
    out += std::format("0x{:X}", value);
  }
  return out.empty() ? "0" : out;
}

std::string CoopFlags(DWORD f) {
  return Flags(f, {{DDSCL_FULLSCREEN, "FULLSCREEN"}, {DDSCL_ALLOWREBOOT, "ALLOWREBOOT"},
                   {DDSCL_NOWINDOWCHANGES, "NOWINDOWCHANGES"}, {DDSCL_NORMAL, "NORMAL"},
                   {DDSCL_EXCLUSIVE, "EXCLUSIVE"}, {DDSCL_ALLOWMODEX, "ALLOWMODEX"},
                   {DDSCL_SETFOCUSWINDOW, "SETFOCUSWINDOW"}, {DDSCL_SETDEVICEWINDOW, "SETDEVICEWINDOW"},
                   {DDSCL_CREATEDEVICEWINDOW, "CREATEDEVICEWINDOW"}, {DDSCL_MULTITHREADED, "MULTITHREADED"},
                   {DDSCL_FPUSETUP, "FPUSETUP"}, {DDSCL_FPUPRESERVE, "FPUPRESERVE"}});
}

std::string CapsFlags(DWORD f) {
  return Flags(f, {{DDSCAPS_PRIMARYSURFACE, "PRIMARY"}, {DDSCAPS_FLIP, "FLIP"},
                   {DDSCAPS_COMPLEX, "COMPLEX"}, {DDSCAPS_BACKBUFFER, "BACKBUFFER"},
                   {DDSCAPS_FRONTBUFFER, "FRONTBUFFER"}, {DDSCAPS_OFFSCREENPLAIN, "OFFSCREENPLAIN"},
                   {DDSCAPS_3DDEVICE, "3DDEVICE"}, {DDSCAPS_TEXTURE, "TEXTURE"},
                   {DDSCAPS_ZBUFFER, "ZBUFFER"}, {DDSCAPS_VIDEOMEMORY, "VIDEOMEMORY"},
                   {DDSCAPS_LOCALVIDMEM, "LOCALVIDMEM"}, {DDSCAPS_NONLOCALVIDMEM, "NONLOCALVIDMEM"},
                   {DDSCAPS_SYSTEMMEMORY, "SYSTEMMEMORY"}, {DDSCAPS_MIPMAP, "MIPMAP"}});
}

std::string FlipFlags(DWORD f) {
  return Flags(f, {{DDFLIP_WAIT, "WAIT"}, {DDFLIP_EVEN, "EVEN"}, {DDFLIP_ODD, "ODD"},
                   {DDFLIP_NOVSYNC, "NOVSYNC"}, {DDFLIP_DONOTWAIT, "DONOTWAIT"}});
}

std::string DescribeSurface(const DDSURFACEDESC2* d) {
  if (!d) return "(null)";
  std::string s = std::format("caps={}", CapsFlags(d->ddsCaps.dwCaps));
  if (d->dwFlags & DDSD_WIDTH) s += std::format(" w={}", d->dwWidth);
  if (d->dwFlags & DDSD_HEIGHT) s += std::format(" h={}", d->dwHeight);
  if (d->dwFlags & DDSD_BACKBUFFERCOUNT) s += std::format(" backbuffers={}", d->dwBackBufferCount);
  if (d->dwFlags & DDSD_PIXELFORMAT) {
    s += std::format(" pf.flags=0x{:X} bits={}", d->ddpfPixelFormat.dwFlags,
                     d->ddpfPixelFormat.dwRGBBitCount);
  }
  if (d->dwFlags & DDSD_MIPMAPCOUNT) s += std::format(" mips={}", d->dwMipMapCount);
  return s;
}

std::string DescribeIid(REFIID iid) {
  if (iid == IID_IDirect3D7) return "IID_IDirect3D7";
  if (iid == IID_IDirectDraw7) return "IID_IDirectDraw7";
  wchar_t text[64] = {};
  StringFromGUID2(iid, text, 64);
  std::string out;
  for (const wchar_t* p = text; *p; ++p) out.push_back(static_cast<char>(*p));
  return out;
}

// Counter for a per-frame method: detailed log for the first calls, then a
// summary of how many calls happened in each interval.
struct CallCounter {
  const char* name;
  std::atomic<std::uint64_t> total{0};
  std::atomic<std::uint64_t> interval_count{0};
  std::atomic<ULONGLONG> interval_start{0};

  // Returns true when this call should be logged in detail.
  bool Hit() {
    const std::uint64_t n = ++total;
    ++interval_count;
    const ULONGLONG now = GetTickCount64();
    ULONGLONG start = interval_start.load();
    if (start == 0) {
      interval_start.compare_exchange_strong(start, now);
    } else if (now - start >= kSummaryIntervalMs &&
               interval_start.compare_exchange_strong(start, now)) {
      const std::uint64_t count = interval_count.exchange(0);
      Log(LogLevel::Info, "[ddraw] {}: {} calls in {} ms ({:.1f}/s), total {}", name, count,
          now - start, count * 1000.0 / static_cast<double>(now - start), n);
    }
    return n <= kDetailedCalls;
  }
};

CallCounter g_flip{"Surface::Flip"};
CallCounter g_blt{"Surface::Blt"};
CallCounter g_blt_fast{"Surface::BltFast"};
CallCounter g_is_lost{"Surface::IsLost"};
CallCounter g_wait_vblank{"DirectDraw::WaitForVerticalBlank"};
CallCounter g_test_coop{"DirectDraw::TestCooperativeLevel"};
std::atomic<int> g_texture_count{0};

IDirectDrawSurface7* g_primary = nullptr;
std::atomic<bool> g_surface_hooks_installed{false};

// ---- originals ----
using DirectDrawCreateExFn = HRESULT(WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
DirectDrawCreateExFn o_DirectDrawCreateEx = nullptr;

using DD_QueryInterfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDraw7*, REFIID, void**);
using DD_CreateSurfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDraw7*, LPDDSURFACEDESC2,
                                                       LPDIRECTDRAWSURFACE7*, IUnknown*);
using DD_EnumDisplayModesFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDraw7*, DWORD, LPDDSURFACEDESC2,
                                                          LPVOID, LPDDENUMMODESCALLBACK2);
using DD_VoidFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDraw7*);
using DD_SetCooperativeLevelFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDraw7*, HWND, DWORD);
using DD_SetDisplayModeFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDraw7*, DWORD, DWORD, DWORD, DWORD,
                                                        DWORD);
using DD_WaitForVerticalBlankFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDraw7*, DWORD, HANDLE);

DD_QueryInterfaceFn o_DD_QueryInterface = nullptr;
DD_CreateSurfaceFn o_DD_CreateSurface = nullptr;
DD_EnumDisplayModesFn o_DD_EnumDisplayModes = nullptr;
DD_VoidFn o_DD_FlipToGDISurface = nullptr;
DD_VoidFn o_DD_RestoreDisplayMode = nullptr;
DD_SetCooperativeLevelFn o_DD_SetCooperativeLevel = nullptr;
DD_SetDisplayModeFn o_DD_SetDisplayMode = nullptr;
DD_WaitForVerticalBlankFn o_DD_WaitForVerticalBlank = nullptr;
DD_VoidFn o_DD_RestoreAllSurfaces = nullptr;
DD_VoidFn o_DD_TestCooperativeLevel = nullptr;

using S_BltFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDrawSurface7*, LPRECT, IDirectDrawSurface7*,
                                            LPRECT, DWORD, LPDDBLTFX);
using S_BltFastFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDrawSurface7*, DWORD, DWORD,
                                                IDirectDrawSurface7*, LPRECT, DWORD);
using S_FlipFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDrawSurface7*, IDirectDrawSurface7*, DWORD);
using S_GetAttachedSurfaceFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDrawSurface7*, LPDDSCAPS2,
                                                           IDirectDrawSurface7**);
using S_VoidFn = HRESULT(STDMETHODCALLTYPE*)(IDirectDrawSurface7*);

S_BltFn o_S_Blt = nullptr;
S_BltFastFn o_S_BltFast = nullptr;
S_FlipFn o_S_Flip = nullptr;
S_GetAttachedSurfaceFn o_S_GetAttachedSurface = nullptr;
S_VoidFn o_S_IsLost = nullptr;
S_VoidFn o_S_Restore = nullptr;

// Installs `replacement` into vtable slot `index` unless it is already there.
// Windows' compatibility layer (apphelp.dll, e.g. the DWM8And16BitMitigation
// layer) rewrites some IDirectDraw7 slots every time a DirectDraw object is
// created, so this is called again after each creation. Whatever the slot held
// (ddraw.dll or the apphelp shim) becomes the function the hook forwards to,
// which keeps the shim's behavior.
template <class Fn>
void Hook(void* object, std::size_t index, void* replacement, Fn& original) {
  void** vtable = *static_cast<void***>(object);
  if (vtable[index] == replacement) {
    return;
  }
  original = reinterpret_cast<Fn>(interop::HookVtable(object, index, replacement));
}

std::string Rect(const RECT* r) {
  return r ? std::format("({},{})-({},{})", r->left, r->top, r->right, r->bottom) : "null";
}

// ---- surface hooks ----

HRESULT STDMETHODCALLTYPE H_S_Blt(IDirectDrawSurface7* self, LPRECT dst, IDirectDrawSurface7* src,
                                  LPRECT src_rect, DWORD flags, LPDDBLTFX fx) {
  if (g_blt.Hit()) {
    Log(LogLevel::Info, "[ddraw] Blt dst={}{} rect={} src={} src_rect={} flags=0x{:X}",
        static_cast<void*>(self), self == g_primary ? " (primary)" : "", Rect(dst),
        static_cast<void*>(src), Rect(src_rect), flags);
  }
  return o_S_Blt(self, dst, src, src_rect, flags, fx);
}

HRESULT STDMETHODCALLTYPE H_S_BltFast(IDirectDrawSurface7* self, DWORD x, DWORD y,
                                      IDirectDrawSurface7* src, LPRECT src_rect, DWORD flags) {
  if (g_blt_fast.Hit()) {
    Log(LogLevel::Info, "[ddraw] BltFast dst={}{} at ({},{}) src_rect={} flags=0x{:X}",
        static_cast<void*>(self), self == g_primary ? " (primary)" : "", x, y, Rect(src_rect),
        flags);
  }
  return o_S_BltFast(self, x, y, src, src_rect, flags);
}

HRESULT STDMETHODCALLTYPE H_S_Flip(IDirectDrawSurface7* self, IDirectDrawSurface7* target,
                                   DWORD flags) {
  const bool detail = g_flip.Hit();
  const HRESULT hr = o_S_Flip(self, target, flags);
  if (detail || FAILED(hr)) {
    Log(LogLevel::Info, "[ddraw] Flip on {}{} target={} flags={} from 0x{:X} -> 0x{:08X}",
        static_cast<void*>(self), self == g_primary ? " (primary)" : "",
        static_cast<void*>(target), FlipFlags(flags),
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), static_cast<unsigned>(hr));
  }
  return hr;
}

HRESULT STDMETHODCALLTYPE H_S_GetAttachedSurface(IDirectDrawSurface7* self, LPDDSCAPS2 caps,
                                                 IDirectDrawSurface7** out) {
  const HRESULT hr = o_S_GetAttachedSurface(self, caps, out);
  Log(LogLevel::Info, "[ddraw] GetAttachedSurface on {}{} caps={} -> 0x{:08X} {}",
      static_cast<void*>(self), self == g_primary ? " (primary)" : "",
      caps ? CapsFlags(caps->dwCaps) : "null", static_cast<unsigned>(hr),
      static_cast<void*>(out ? *out : nullptr));
  return hr;
}

std::atomic<HRESULT> g_last_is_lost{DD_OK};
std::atomic<HRESULT> g_last_test_coop{DD_OK};

HRESULT STDMETHODCALLTYPE H_S_IsLost(IDirectDrawSurface7* self) {
  const HRESULT hr = o_S_IsLost(self);
  // Log the first calls and every change of the result (lost <-> not lost).
  if (g_is_lost.Hit() || g_last_is_lost.exchange(hr) != hr) {
    Log(LogLevel::Info, "[ddraw] IsLost on {}{} -> 0x{:08X}", static_cast<void*>(self),
        self == g_primary ? " (primary)" : "", static_cast<unsigned>(hr));
  }
  return hr;
}

HRESULT STDMETHODCALLTYPE H_S_Restore(IDirectDrawSurface7* self) {
  const HRESULT hr = o_S_Restore(self);
  Log(LogLevel::Info, "[ddraw] Restore on {}{} -> 0x{:08X}", static_cast<void*>(self),
      self == g_primary ? " (primary)" : "", static_cast<unsigned>(hr));
  return hr;
}

// Called after every non-texture CreateSurface, for the same reason as InstallDirectDrawHooks.
void InstallSurfaceHooks(IDirectDrawSurface7* surface) {
  const bool first = !g_surface_hooks_installed.exchange(true);
  if (first) {
    void** vtable = *reinterpret_cast<void***>(surface);
    for (std::size_t i : {kSurf_Blt, kSurf_BltFast, kSurf_Flip, kSurf_GetAttachedSurface,
                          kSurf_IsLost, kSurf_Restore}) {
      Log(LogLevel::Info, "[ddraw]   surface vtable[{}] before hook = {} in {}", i, vtable[i],
          ModuleOf(vtable[i]));
    }
  }
  Hook(surface, kSurf_Blt, reinterpret_cast<void*>(H_S_Blt), o_S_Blt);
  Hook(surface, kSurf_BltFast, reinterpret_cast<void*>(H_S_BltFast), o_S_BltFast);
  Hook(surface, kSurf_Flip, reinterpret_cast<void*>(H_S_Flip), o_S_Flip);
  Hook(surface, kSurf_GetAttachedSurface, reinterpret_cast<void*>(H_S_GetAttachedSurface),
       o_S_GetAttachedSurface);
  Hook(surface, kSurf_IsLost, reinterpret_cast<void*>(H_S_IsLost), o_S_IsLost);
  Hook(surface, kSurf_Restore, reinterpret_cast<void*>(H_S_Restore), o_S_Restore);
  if (first) {
    Log(LogLevel::Info, "[ddraw] surface vtable hooks installed");
  }
}

// ---- IDirectDraw7 hooks ----

HRESULT STDMETHODCALLTYPE H_DD_QueryInterface(IDirectDraw7* self, REFIID iid, void** out) {
  const HRESULT hr = o_DD_QueryInterface(self, iid, out);
  void* result = (SUCCEEDED(hr) && out) ? *out : nullptr;
  Log(LogLevel::Info,
      "[ddraw] DirectDraw::QueryInterface on {} {} from 0x{:X} -> 0x{:08X} object={} vtable={}",
      static_cast<void*>(self), DescribeIid(iid),
      reinterpret_cast<std::uintptr_t>(_ReturnAddress()), static_cast<unsigned>(hr), result,
      result ? *static_cast<void**>(result) : nullptr);
  return hr;
}

HRESULT STDMETHODCALLTYPE H_DD_CreateSurface(IDirectDraw7* self, LPDDSURFACEDESC2 desc,
                                             LPDIRECTDRAWSURFACE7* out, IUnknown* outer) {
  const HRESULT hr = o_DD_CreateSurface(self, desc, out, outer);
  IDirectDrawSurface7* surface = (SUCCEEDED(hr) && out) ? *out : nullptr;
  const bool texture = desc && (desc->ddsCaps.dwCaps & DDSCAPS_TEXTURE);
  if (texture) {
    const int n = ++g_texture_count;
    if (n <= kDetailedCalls || n % 100 == 0) {
      Log(LogLevel::Info, "[ddraw] CreateSurface (texture #{}) {} -> 0x{:08X}", n,
          DescribeSurface(desc), static_cast<unsigned>(hr));
    }
  } else {
    Log(LogLevel::Info, "[ddraw] CreateSurface {} from 0x{:X} -> 0x{:08X} {}",
        DescribeSurface(desc), reinterpret_cast<std::uintptr_t>(_ReturnAddress()),
        static_cast<unsigned>(hr), static_cast<void*>(surface));
  }
  if (surface) {
    if (desc && (desc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)) {
      g_primary = surface;
    }
    InstallSurfaceHooks(surface);
  }
  return hr;
}

HRESULT STDMETHODCALLTYPE H_DD_EnumDisplayModes(IDirectDraw7* self, DWORD flags,
                                                LPDDSURFACEDESC2 desc, LPVOID context,
                                                LPDDENUMMODESCALLBACK2 callback) {
  Log(LogLevel::Info, "[ddraw] EnumDisplayModes flags=0x{:X} filter={}", flags,
      DescribeSurface(desc));
  return o_DD_EnumDisplayModes(self, flags, desc, context, callback);
}

HRESULT STDMETHODCALLTYPE H_DD_FlipToGDISurface(IDirectDraw7* self) {
  const HRESULT hr = o_DD_FlipToGDISurface(self);
  Log(LogLevel::Info, "[ddraw] FlipToGDISurface -> 0x{:08X}", static_cast<unsigned>(hr));
  return hr;
}

HRESULT STDMETHODCALLTYPE H_DD_RestoreDisplayMode(IDirectDraw7* self) {
  const HRESULT hr = o_DD_RestoreDisplayMode(self);
  Log(LogLevel::Info, "[ddraw] RestoreDisplayMode -> 0x{:08X}", static_cast<unsigned>(hr));
  return hr;
}

HRESULT STDMETHODCALLTYPE H_DD_SetCooperativeLevel(IDirectDraw7* self, HWND hwnd, DWORD flags) {
  const HRESULT hr = o_DD_SetCooperativeLevel(self, hwnd, flags);
  Log(LogLevel::Info, "[ddraw] SetCooperativeLevel hwnd={} flags={} from 0x{:X} -> 0x{:08X}",
      static_cast<void*>(hwnd), CoopFlags(flags),
      reinterpret_cast<std::uintptr_t>(_ReturnAddress()), static_cast<unsigned>(hr));
  return hr;
}

HRESULT STDMETHODCALLTYPE H_DD_SetDisplayMode(IDirectDraw7* self, DWORD w, DWORD h, DWORD bpp,
                                              DWORD refresh, DWORD flags) {
  const HRESULT hr = o_DD_SetDisplayMode(self, w, h, bpp, refresh, flags);
  Log(LogLevel::Info,
      "[ddraw] SetDisplayMode {}x{} {}bpp refresh={} flags=0x{:X} from 0x{:X} -> 0x{:08X}", w, h,
      bpp, refresh, flags, reinterpret_cast<std::uintptr_t>(_ReturnAddress()),
      static_cast<unsigned>(hr));
  return hr;
}

HRESULT STDMETHODCALLTYPE H_DD_WaitForVerticalBlank(IDirectDraw7* self, DWORD flags, HANDLE ev) {
  if (g_wait_vblank.Hit()) {
    Log(LogLevel::Info, "[ddraw] WaitForVerticalBlank flags=0x{:X}", flags);
  }
  return o_DD_WaitForVerticalBlank(self, flags, ev);
}

HRESULT STDMETHODCALLTYPE H_DD_RestoreAllSurfaces(IDirectDraw7* self) {
  const HRESULT hr = o_DD_RestoreAllSurfaces(self);
  Log(LogLevel::Info, "[ddraw] RestoreAllSurfaces -> 0x{:08X}", static_cast<unsigned>(hr));
  return hr;
}

HRESULT STDMETHODCALLTYPE H_DD_TestCooperativeLevel(IDirectDraw7* self) {
  static std::atomic<bool> dumped{false};
  if (!dumped.exchange(true)) {
    // Verify that the vtable entries hooked at install time are still ours.
    void** vtable = *reinterpret_cast<void***>(self);
    Log(LogLevel::Info,
        "[ddraw] vtable check: [6] CreateSurface={} (hook {}) [20] SetCooperativeLevel={} (hook {}) "
        "[21] SetDisplayMode={} (hook {}) [26] TestCooperativeLevel={} (hook {})",
        vtable[kDD_CreateSurface], reinterpret_cast<void*>(H_DD_CreateSurface),
        vtable[kDD_SetCooperativeLevel], reinterpret_cast<void*>(H_DD_SetCooperativeLevel),
        vtable[kDD_SetDisplayMode], reinterpret_cast<void*>(H_DD_SetDisplayMode),
        vtable[kDD_TestCooperativeLevel], reinterpret_cast<void*>(H_DD_TestCooperativeLevel));
    for (std::size_t i = 0; i <= 29; ++i) {
      Log(LogLevel::Info, "[ddraw]   IDirectDraw7 vtable[{}] = {} in {}", i, vtable[i],
          ModuleOf(vtable[i]));
    }
  }
  const HRESULT hr = o_DD_TestCooperativeLevel(self);
  if (g_test_coop.Hit() || g_last_test_coop.exchange(hr) != hr) {
    Log(LogLevel::Info, "[ddraw] TestCooperativeLevel on {} (vtable {}) from 0x{:X} -> 0x{:08X}",
        static_cast<void*>(self), *reinterpret_cast<void**>(self),
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), static_cast<unsigned>(hr));
  }
  return hr;
}

std::atomic<bool> g_dd_hooks_logged{false};

// Called after every DirectDrawCreateEx (see Hook() for why).
void InstallDirectDrawHooks(void* dd) {
  Hook(dd, kDD_QueryInterface, reinterpret_cast<void*>(H_DD_QueryInterface), o_DD_QueryInterface);
  Hook(dd, kDD_CreateSurface, reinterpret_cast<void*>(H_DD_CreateSurface), o_DD_CreateSurface);
  Hook(dd, kDD_EnumDisplayModes, reinterpret_cast<void*>(H_DD_EnumDisplayModes),
       o_DD_EnumDisplayModes);
  Hook(dd, kDD_FlipToGDISurface, reinterpret_cast<void*>(H_DD_FlipToGDISurface),
       o_DD_FlipToGDISurface);
  Hook(dd, kDD_RestoreDisplayMode, reinterpret_cast<void*>(H_DD_RestoreDisplayMode),
       o_DD_RestoreDisplayMode);
  Hook(dd, kDD_SetCooperativeLevel, reinterpret_cast<void*>(H_DD_SetCooperativeLevel),
       o_DD_SetCooperativeLevel);
  Hook(dd, kDD_SetDisplayMode, reinterpret_cast<void*>(H_DD_SetDisplayMode), o_DD_SetDisplayMode);
  Hook(dd, kDD_WaitForVerticalBlank, reinterpret_cast<void*>(H_DD_WaitForVerticalBlank),
       o_DD_WaitForVerticalBlank);
  Hook(dd, kDD_RestoreAllSurfaces, reinterpret_cast<void*>(H_DD_RestoreAllSurfaces),
       o_DD_RestoreAllSurfaces);
  Hook(dd, kDD_TestCooperativeLevel, reinterpret_cast<void*>(H_DD_TestCooperativeLevel),
       o_DD_TestCooperativeLevel);
  if (!g_dd_hooks_logged.exchange(true)) {
    Log(LogLevel::Info, "[ddraw] IDirectDraw7 vtable hooks installed");
  }
}

// Name of the module that contains `address`, for telling who owns a vtable entry.
std::string ModuleOf(const void* address) {
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          static_cast<LPCWSTR>(address), &module)) {
    return "?";
  }
  char path[MAX_PATH] = {};
  GetModuleFileNameA(module, path, MAX_PATH);
  return path;
}

void* VtableOf(void* object) {
  return object ? *static_cast<void**>(object) : nullptr;
}

HRESULT WINAPI H_DirectDrawCreateEx(GUID* guid, LPVOID* out, REFIID iid, IUnknown* outer) {
  const HRESULT hr = o_DirectDrawCreateEx(guid, out, iid, outer);
  void* object = (SUCCEEDED(hr) && out) ? *out : nullptr;
  Log(LogLevel::Info,
      "[ddraw] DirectDrawCreateEx guid={} iid={} from 0x{:X} -> 0x{:08X} object={} vtable={}",
      static_cast<void*>(guid), DescribeIid(iid),
      reinterpret_cast<std::uintptr_t>(_ReturnAddress()), static_cast<unsigned>(hr), object,
      VtableOf(object));
  if (object && iid == IID_IDirectDraw7) {
    InstallDirectDrawHooks(object);
  }
  return hr;
}

// Hod2.exe (or its statically linked D3DX) may resolve DirectDraw functions at
// run time instead of through its import table; log those lookups.
using LoadLibraryAFn = HMODULE(WINAPI*)(LPCSTR);
using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);
LoadLibraryAFn o_LoadLibraryA = nullptr;
GetProcAddressFn o_GetProcAddress = nullptr;

HMODULE WINAPI H_LoadLibraryA(LPCSTR name) {
  HMODULE module = o_LoadLibraryA(name);
  Log(LogLevel::Info, "[ddraw] LoadLibraryA(\"{}\") from 0x{:X} -> {}", name ? name : "(null)",
      reinterpret_cast<std::uintptr_t>(_ReturnAddress()), static_cast<void*>(module));
  return module;
}

FARPROC WINAPI H_GetProcAddress(HMODULE module, LPCSTR name) {
  FARPROC proc = o_GetProcAddress(module, name);
  const bool by_ordinal = (reinterpret_cast<std::uintptr_t>(name) >> 16) == 0;
  Log(LogLevel::Info, "[ddraw] GetProcAddress({}, {}) from 0x{:X} -> {}",
      static_cast<void*>(module),
      by_ordinal ? std::format("#{}", reinterpret_cast<std::uintptr_t>(name)) : std::string(name),
      reinterpret_cast<std::uintptr_t>(_ReturnAddress()), reinterpret_cast<void*>(proc));
  return proc;
}

}  // namespace

bool InstallDDrawTrace() {
  HMODULE exe = GetModuleHandleW(nullptr);
  o_LoadLibraryA = static_cast<LoadLibraryAFn>(interop::HookImport(
      exe, "KERNEL32.dll", "LoadLibraryA", reinterpret_cast<void*>(H_LoadLibraryA)));
  o_GetProcAddress = static_cast<GetProcAddressFn>(interop::HookImport(
      exe, "KERNEL32.dll", "GetProcAddress", reinterpret_cast<void*>(H_GetProcAddress)));
  o_DirectDrawCreateEx = static_cast<DirectDrawCreateExFn>(
      interop::HookImport(GetModuleHandleW(nullptr), "DDRAW.dll", "DirectDrawCreateEx",
                          reinterpret_cast<void*>(H_DirectDrawCreateEx)));
  if (!o_DirectDrawCreateEx) {
    Log(LogLevel::Warn, "trace_ddraw: could not hook DDRAW!DirectDrawCreateEx");
    return false;
  }
  Log(LogLevel::Info, "trace_ddraw: installed");
  return true;
}

}  // namespace thod2::core::patches
