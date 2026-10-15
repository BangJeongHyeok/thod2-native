#pragma once

#include <cstdint>
#include <filesystem>

namespace thod2::core::patches {

// Maps the cursor the game reads (client pixels of its window, through
// GetCursorClientPos 0x41E670) to the game's 640x480 screen ([input] map_cursor).
//
// The game treats client pixels as 640x480 coordinates. That only holds in its
// own 640x480 full-screen mode: with dgVoodoo2 the window is as large as the
// screen and only the top-left 640x480 pixels could be aimed at. The image
// rectangle follows dgVoodoo2's ScalingMode (read from dgVoodoo.conf next to
// the game): "stretched" fills the client area, the others keep 4:3.
//
// `widen` > 1 additionally maps the 640-wide picture to the wider virtual 4:3
// coordinates of the widescreen experiment (x = 320 + (x - 320) * widen).
// See docs/work/renderer.md "조준".
bool InstallCursorMap(const std::filesystem::path& game_dir, float widen);

// Applies the mapping to a cursor position in client pixels (no-op when not installed).
void MapCursorToGame(std::int32_t* xy);

}  // namespace thod2::core::patches
