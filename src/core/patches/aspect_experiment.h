#pragma once

namespace thod2::core::patches {

// Widescreen reverse-engineering experiment ([debug] aspect_experiment), not a
// finished feature. See docs/work/renderer.md "화면 비율 분석".
//   1: the two perspective setups (0x418590, 0x419590) get a 16:9 aspect instead of 4:3
//      (Hor+: vertical field of view unchanged).
//   2: 1, plus every screen-space quad drawn by 0x4A6ED0 is squeezed horizontally
//      towards the screen centre by 3/4, so that a 640x480 frame stretched to
//      16:9 shows 2D elements with their original proportions.
// Meant for a 16:9 output with dgVoodoo2 ScalingMode = stretched.
bool InstallAspectExperiment(int stage);

}  // namespace thod2::core::patches
