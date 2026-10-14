#include <thod2/interop/vtable_hook.h>

#include <windows.h>

namespace thod2::interop {

void* HookVtable(void* com_object, std::size_t index, void* replacement) {
  if (!com_object || !replacement) {
    return nullptr;
  }
  void** vtable = *static_cast<void***>(com_object);
  void** slot = &vtable[index];
  DWORD old_protect = 0;
  if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
    return nullptr;
  }
  void* previous = *slot;
  *slot = replacement;
  VirtualProtect(slot, sizeof(void*), old_protect, &old_protect);
  FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
  return previous;
}

}  // namespace thod2::interop
