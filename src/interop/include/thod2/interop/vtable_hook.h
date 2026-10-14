#pragma once

#include <cstddef>

namespace thod2::interop {

// Replaces entry `index` of the virtual function table of `com_object` with
// `replacement` and returns the previous entry, or nullptr on failure.
// The table is shared by every object of the same implementation class, so the
// hook sees calls on all of them; compare `this` inside the hook when needed.
void* HookVtable(void* com_object, std::size_t index, void* replacement);

}  // namespace thod2::interop
