#pragma once

namespace thod2::core::patches {

enum class InputUpdateMode {
  // Our UpdateInput (thod2/platform/input.h) replaces the original 0x41E120.
  Reimplemented,
  // The original runs and decides the result. Every frame our UpdateInput then
  // runs on the same starting state, replaying the DirectInput and cursor calls
  // the original made, and the input memory of both runs is compared. Mismatches
  // are logged ([debug] input_compare).
  Compare,
};

// Redirects the only call of the original per-frame input update (at
// 0x40E5F0) to our code. See docs/work/platform-input.md.
bool InstallInputUpdate(InputUpdateMode mode);

}  // namespace thod2::core::patches
