#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace thod2::interop {

// Overwrites the bytes at `address` with `replacement`, but only if they
// currently equal `expected` (same length). Handles page protection and the
// instruction cache. Returns false and changes nothing on a mismatch, so a
// patch written for one Hod2.exe build cannot corrupt another.
bool PatchBytes(std::uintptr_t address, std::span<const std::uint8_t> expected,
                std::span<const std::uint8_t> replacement);

// Convenience for the common case of replacing a 32-bit value (an immediate
// operand or a pointer) stored at `address`.
bool PatchUint32(std::uintptr_t address, std::uint32_t expected, std::uint32_t replacement);

}  // namespace thod2::interop
