#pragma once

namespace thod2::core::patches {

// Widescreen reverse-engineering experiment ([debug] aspect_experiment), not a
// finished feature. See docs/work/renderer.md "화면 비율 분석".
//   1: the two perspective setups (0x418590, 0x419590) get a 16:9 aspect instead of 4:3
//      (Hor+: vertical field of view unchanged), and the cinematic letterbox
//      model (drawn in camera space by 0x413A40) is widened by the same ratio.
//   2: 1, plus every screen-space quad drawn by 0x4A6ED0 is squeezed horizontally
//      towards the screen centre by 3/4, so that a 640x480 frame stretched to
//      16:9 shows 2D elements with their original proportions.
// Meant for a 16:9 output with dgVoodoo2 ScalingMode = stretched.
// trace_screen_quads ([debug] trace_screen_quads), with any stage, logs every
// distinct screen quad that is at least 200 px wide or touches the top/bottom
// 40 px, every distinct model with its matrix, and the call chain (return
// addresses found on the stack) of the letterbox model.
bool InstallAspectExperiment(int stage, bool trace_screen_quads);

}  // namespace thod2::core::patches
