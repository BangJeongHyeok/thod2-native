#include "patches/texture_replace.h"

#include "log.h"
#include "util.h"

#include <thod2/interop/hod2_addresses.h>
#include <thod2/interop/memory_patch.h>

#include <windows.h>
#include <ddraw.h>
#include <wincodec.h>

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace thod2::core::patches {
namespace {

namespace hod2 = interop::hod2;

// Slot of g_textureFormats used for replacements; the original textures use
// the 16-bit slots 2, 5 and 6.
constexpr int kFormatSlot32 = 7;
// Fallback when the device has no 32-bit texture format: A4R4G4B4.
constexpr int kFormatSlot4444 = 6;

struct PvrEntry {
  std::int16_t width;
  std::int16_t height;
  std::uint32_t format;
  std::uint32_t offset;
  std::int32_t texture_id;
};
static_assert(sizeof(PvrEntry) == 16);

using CreateTextureFromPvrFn = void*(__cdecl*)(const PvrEntry* entry, const std::uint8_t* pixels);
using CreateTextureSurfaceFn = IDirectDrawSurface7*(__cdecl*)(int width, int height, int slot);
using UploadTextureSurfaceFn = void*(__cdecl*)(IDirectDrawSurface7* surface, std::uint8_t flags);

struct BankRange {
  std::uintptr_t begin;
  std::uintptr_t end;
  std::string name;  // lower case, without .bin
};

std::vector<BankRange> g_banks;
// (bank, texture id) -> image; bank "" = images directly in data\tex (any bank).
std::map<std::pair<std::string, std::int32_t>, std::filesystem::path> g_files;
bool g_logged_device = false;
bool g_trace_missing = false;  // [debug] trace_textures
std::set<std::pair<std::string, std::int32_t>> g_missing;

struct Image {
  UINT width = 0;
  UINT height = 0;
  std::vector<std::uint32_t> pixels;  // 0xAARRGGBB, top row first
};

// Decodes any format WIC knows (PNG, BMP, ...) to straight (not
// premultiplied) 8-bit BGRA. Returns an empty image on failure.
Image LoadImageFile(const std::filesystem::path& path) {
  Image image;
  const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  IWICImagingFactory* factory = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICFormatConverter* converter = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory));
  if (SUCCEEDED(hr)) {
    hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, &decoder);
  }
  if (SUCCEEDED(hr)) {
    hr = decoder->GetFrame(0, &frame);
  }
  if (SUCCEEDED(hr)) {
    hr = factory->CreateFormatConverter(&converter);
  }
  if (SUCCEEDED(hr)) {
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                               nullptr, 0.0, WICBitmapPaletteTypeCustom);
  }
  if (SUCCEEDED(hr)) {
    hr = converter->GetSize(&image.width, &image.height);
  }
  if (SUCCEEDED(hr)) {
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height);
    hr = converter->CopyPixels(nullptr, image.width * 4,
                               static_cast<UINT>(image.pixels.size() * 4),
                               reinterpret_cast<BYTE*>(image.pixels.data()));
  }
  if (FAILED(hr)) {
    Log(LogLevel::Warn, "replace_textures: cannot read {} (0x{:08X})", ToUtf8(path.native()),
        static_cast<std::uint32_t>(hr));
    image = {};
  }
  for (IUnknown* object : std::array<IUnknown*, 4>{converter, frame, decoder, factory}) {
    if (object) {
      object->Release();
    }
  }
  if (SUCCEEDED(init)) {
    CoUninitialize();
  }
  return image;
}

// Packs one 8-bit channel into the bits of `mask`.
std::uint32_t PackChannel(std::uint32_t value, std::uint32_t mask) {
  if (mask == 0) {
    return 0;
  }
  unsigned long shift = 0;
  _BitScanForward(&shift, mask);
  const std::uint32_t max = mask >> shift;
  return ((value * max + 127) / 255) << shift;
}

const DDPIXELFORMAT& FormatSlot(int slot) {
  return reinterpret_cast<const DDPIXELFORMAT*>(hod2::kTextureFormats)[slot];
}

void LogDevice() {
  if (g_logged_device) {
    return;
  }
  g_logged_device = true;
  for (int slot = 0; slot < 8; ++slot) {
    const DDPIXELFORMAT& f = FormatSlot(slot);
    Log(LogLevel::Info,
        "replace_textures: format slot {}: size {} flags 0x{:X} bits {} masks {:08X} {:08X} {:08X} {:08X}",
        slot, f.dwSize, f.dwFlags, f.dwRGBBitCount, f.dwRBitMask, f.dwGBitMask, f.dwBBitMask,
        f.dwRGBAlphaBitMask);
  }
  Log(LogLevel::Info, "replace_textures: device max texture size {}x{}",
      *reinterpret_cast<const std::uint32_t*>(hod2::kTextureMaxWidth),
      *reinterpret_cast<const std::uint32_t*>(hod2::kTextureMaxHeight));
}

// Builds the texture from `image` the way CreateTextureFromPvr does: a system
// memory surface filled under Lock, then UploadTextureSurface. Returns null on
// failure (the caller then creates the original texture).
void* CreateReplacement(const PvrEntry& entry, const Image& image) {
  int slot = kFormatSlot32;
  const DDPIXELFORMAT* format = &FormatSlot(slot);
  if (format->dwSize == 0 || format->dwRGBBitCount != 32 || format->dwRGBAlphaBitMask == 0) {
    slot = kFormatSlot4444;
    format = &FormatSlot(slot);
  }
  if (format->dwSize == 0) {
    return nullptr;
  }
  const auto create = reinterpret_cast<CreateTextureSurfaceFn>(hod2::kCreateTextureSurface);
  IDirectDrawSurface7* surface = create(static_cast<int>(image.width),
                                        static_cast<int>(image.height), slot);
  if (!surface) {
    Log(LogLevel::Warn, "replace_textures: texture {:X}: cannot create a {}x{} surface (slot {})",
        entry.texture_id, image.width, image.height, slot);
    return nullptr;
  }
  DDSURFACEDESC2 desc{};
  desc.dwSize = sizeof(desc);
  if (FAILED(surface->Lock(nullptr, &desc, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr))) {
    surface->Release();
    return nullptr;
  }
  const DDPIXELFORMAT& pf = desc.ddpfPixelFormat;
  const std::uint32_t bytes = pf.dwRGBBitCount / 8;
  auto* bits = static_cast<std::uint8_t*>(desc.lpSurface);
  // The game's textures are stored bottom-up (docs/formats/tex.md), so the
  // first surface row gets the last image row.
  for (UINT y = 0; y < image.height; ++y) {
    const std::uint32_t* src = &image.pixels[static_cast<std::size_t>(image.height - 1 - y) * image.width];
    std::uint8_t* dst = bits + static_cast<std::size_t>(y) * desc.lPitch;
    for (UINT x = 0; x < image.width; ++x) {
      const std::uint32_t c = src[x];
      const std::uint32_t v = PackChannel(c >> 24, pf.dwRGBAlphaBitMask) |
                              PackChannel((c >> 16) & 0xFF, pf.dwRBitMask) |
                              PackChannel((c >> 8) & 0xFF, pf.dwGBitMask) |
                              PackChannel(c & 0xFF, pf.dwBBitMask);
      std::memcpy(dst + x * bytes, &v, bytes);
    }
  }
  surface->Unlock(nullptr);
  const auto upload = reinterpret_cast<UploadTextureSurfaceFn>(hod2::kUploadTextureSurface);
  void* handle = upload(surface, 0);
  Log(LogLevel::Info, "replace_textures: texture {:X} {}x{} -> {}x{} ({}-bit){}", entry.texture_id,
      entry.width, entry.height, image.width, image.height, pf.dwRGBBitCount,
      handle ? "" : " failed");
  return handle;
}

// Bank whose entry table (g_texBankEntries) holds `entry`, or "" for entries
// that are not in a bank table (model textures, LoadModelTextures).
const std::string& BankOf(const PvrEntry* entry) {
  static const std::string kNone;
  const auto address = reinterpret_cast<std::uintptr_t>(entry);
  for (const BankRange& bank : g_banks) {
    if (address >= bank.begin && address < bank.end) {
      return bank.name;
    }
  }
  return kNone;
}

void* __cdecl ReplacedCreateTextureFromPvr(const PvrEntry* entry, const std::uint8_t* pixels) {
  const auto original = reinterpret_cast<CreateTextureFromPvrFn>(hod2::kCreateTextureFromPvr);
  if (entry->width <= 0 || entry->height <= 0) {
    return original(entry, pixels);
  }
  // data\tex\<bank>\<id>.png first: texture ids are reused by other banks.
  auto file = g_files.find({BankOf(entry), entry->texture_id});
  if (file == g_files.end()) {
    file = g_files.find({std::string(), entry->texture_id});
  }
  if (file == g_files.end()) {
    if (g_trace_missing && g_missing.insert({BankOf(entry), entry->texture_id}).second) {
      const std::string& bank = BankOf(entry);
      Log(LogLevel::Info, "replace_textures: no image for {} {:X} ({}x{}, format 0x{:X})",
          bank.empty() ? "(no bank)" : bank, entry->texture_id, entry->width, entry->height, entry->format);
    }
    return original(entry, pixels);
  }
  LogDevice();
  const Image image = LoadImageFile(file->second);
  if (image.pixels.empty()) {
    return original(entry, pixels);
  }
  // A different aspect ratio would be stretched to the original quad.
  if (static_cast<std::uint64_t>(image.width) * entry->height !=
      static_cast<std::uint64_t>(image.height) * entry->width) {
    Log(LogLevel::Warn, "replace_textures: texture {:X}: {}x{} does not have the aspect of {}x{}",
        entry->texture_id, image.width, image.height, entry->width, entry->height);
  }
  void* handle = CreateReplacement(*entry, image);
  return handle ? handle : original(entry, pixels);
}

std::array<std::uint8_t, 5> CallBytes(std::uintptr_t from, std::uintptr_t to) {
  const auto rel = static_cast<std::uint32_t>(to - (from + 5));
  return {0xE8, static_cast<std::uint8_t>(rel), static_cast<std::uint8_t>(rel >> 8),
          static_cast<std::uint8_t>(rel >> 16), static_cast<std::uint8_t>(rel >> 24)};
}

// Adds the <hex id>.png files of `dir` under `bank` ("" = any bank).
void AddImages(const std::filesystem::path& dir, const std::string& bank) {
  std::error_code ec;
  for (const auto& item : std::filesystem::directory_iterator(dir, ec)) {
    const std::wstring stem = item.path().stem().native();
    wchar_t* end = nullptr;
    const unsigned long id = std::wcstoul(stem.c_str(), &end, 16);
    if (!item.is_regular_file(ec) || stem.empty() || *end != L'\0' || id > 0x7FFFFFFF) {
      continue;
    }
    g_files[{bank, static_cast<std::int32_t>(id)}] = item.path();
  }
}

std::string Lower(std::string text) {
  for (char& c : text) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return text;
}

}  // namespace

bool InstallTextureReplace(const std::filesystem::path& folder, bool trace_missing) {
  g_trace_missing = trace_missing;
  // Entry table range and name (file name without .bin) of every bank.
  const auto* names = reinterpret_cast<const char* const*>(hod2::kTexBankNames);
  const auto* tables = reinterpret_cast<const std::uintptr_t*>(hod2::kTexBankEntries);
  const auto* counts = reinterpret_cast<const std::int16_t*>(hod2::kTexBankCounts);
  for (std::size_t bank = 1; bank <= hod2::kTexBankCount; ++bank) {
    if (!names[bank] || !tables[bank] || counts[bank] <= 0) {
      continue;
    }
    std::string name = Lower(names[bank]);
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".bin") == 0) {
      name.resize(name.size() - 4);
    }
    g_banks.push_back({tables[bank], tables[bank] + sizeof(PvrEntry) * counts[bank], name});
  }
  AddImages(folder, std::string());
  std::error_code ec;
  for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
    if (item.is_directory(ec)) {
      AddImages(item.path(), Lower(ToUtf8(item.path().filename().native())));
    }
  }
  if (g_files.empty()) {
    Log(LogLevel::Info, "replace_textures: no images in {}", ToUtf8(folder.native()));
    return true;
  }
  bool ok = true;
  const auto ours = reinterpret_cast<std::uintptr_t>(&ReplacedCreateTextureFromPvr);
  for (std::uintptr_t call : {hod2::kCreateTextureFromPvrCall1, hod2::kCreateTextureFromPvrCall2,
                              hod2::kCreateTextureFromPvrCall3}) {
    if (!interop::PatchBytes(call, CallBytes(call, hod2::kCreateTextureFromPvr), CallBytes(call, ours))) {
      Log(LogLevel::Warn, "replace_textures: call at 0x{:X} does not match", call);
      ok = false;
    }
  }
  Log(LogLevel::Info, "replace_textures: {} images in {}{}", g_files.size(), ToUtf8(folder.native()),
      ok ? "" : " (call sites not patched)");
  return ok;
}

}  // namespace thod2::core::patches
