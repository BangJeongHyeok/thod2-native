#include <thod2/interop/memory_patch.h>

#include <windows.h>

#include <cstring>

namespace thod2::interop {

bool PatchBytes(std::uintptr_t address, std::span<const std::uint8_t> expected,
                std::span<const std::uint8_t> replacement) {
  if (address == 0 || expected.size() != replacement.size() || expected.empty()) {
    return false;
  }
  void* target = reinterpret_cast<void*>(address);
  MEMORY_BASIC_INFORMATION info{};
  if (!VirtualQuery(target, &info, sizeof(info)) || info.State != MEM_COMMIT) {
    return false;
  }
  if (std::memcmp(target, expected.data(), expected.size()) != 0) {
    return false;
  }
  DWORD old_protect = 0;
  if (!VirtualProtect(target, replacement.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
    return false;
  }
  std::memcpy(target, replacement.data(), replacement.size());
  VirtualProtect(target, replacement.size(), old_protect, &old_protect);
  FlushInstructionCache(GetCurrentProcess(), target, replacement.size());
  return true;
}

bool PatchUint32(std::uintptr_t address, std::uint32_t expected, std::uint32_t replacement) {
  std::uint8_t from[4];
  std::uint8_t to[4];
  std::memcpy(from, &expected, 4);
  std::memcpy(to, &replacement, 4);
  return PatchBytes(address, from, to);
}

}  // namespace thod2::interop
