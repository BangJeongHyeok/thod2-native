#pragma once

#include <filesystem>

namespace thod2::core::patches {

// Texture replacement ([assets] replace_textures, docs/work/assets.md G1).
// A PNG named after a texture id (hex, e.g. 10B1.png for "PRESS START BUTTON")
// in `folder` replaces that texture when the game creates it. The image may be
// larger than the original: 2D sprites take their screen size from the
// original width/height (g_textureSizes) and always use UV 0..1, so only the
// detail changes. The image is stored as 32-bit ARGB when the device offers it.
//
// Hooks the three calls of CreateTextureFromPvr (0x4AB690). Returns false if
// the call sites do not match. An empty or missing folder changes nothing.
bool InstallTextureReplace(const std::filesystem::path& folder);

}  // namespace thod2::core::patches
