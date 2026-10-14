#include <thod2/interop/iat_hook.h>

#include <cstring>

namespace thod2::interop {
namespace {

void* WriteSlot(void** slot, void* replacement) {
  void* previous = *slot;
  DWORD old_protect = 0;
  if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
    return nullptr;
  }
  *slot = replacement;
  VirtualProtect(slot, sizeof(void*), old_protect, &old_protect);
  FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
  return previous;
}

}  // namespace

void* HookImport(HMODULE module, const char* dll, const char* function, void* replacement) {
  if (!module || !dll || !function || !replacement) {
    return nullptr;
  }
  auto* base = reinterpret_cast<BYTE*>(module);
  auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
    return nullptr;
  }
  auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (dir.VirtualAddress == 0) {
    return nullptr;
  }

  for (auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
       desc->Name != 0; ++desc) {
    if (_stricmp(reinterpret_cast<const char*>(base + desc->Name), dll) != 0) {
      continue;
    }
    auto* slots = reinterpret_cast<void**>(base + desc->FirstThunk);

    if (desc->OriginalFirstThunk != 0) {
      auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->OriginalFirstThunk);
      for (size_t i = 0; names[i].u1.AddressOfData != 0; ++i) {
        if (IMAGE_SNAP_BY_ORDINAL(names[i].u1.Ordinal)) {
          continue;
        }
        auto* by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData);
        if (std::strcmp(reinterpret_cast<const char*>(by_name->Name), function) == 0) {
          return WriteSlot(&slots[i], replacement);
        }
      }
      continue;
    }

    // No import name table: match the slot by the export's current address.
    HMODULE target = GetModuleHandleA(dll);
    void* current = target ? reinterpret_cast<void*>(GetProcAddress(target, function)) : nullptr;
    if (!current) {
      continue;
    }
    for (size_t i = 0; slots[i] != nullptr; ++i) {
      if (slots[i] == current) {
        return WriteSlot(&slots[i], replacement);
      }
    }
  }
  return nullptr;
}

}  // namespace thod2::interop
