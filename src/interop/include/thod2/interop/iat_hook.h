#pragma once

#include <windows.h>

namespace thod2::interop {

// Replaces the import address table entry of `module` for `dll!function` with
// `replacement` and returns the previous target, or nullptr if the import was
// not found. The entry is looked up by name through the import name table; if
// the module has none, it falls back to matching the current address of the
// export. Writing the IAT does not load libraries, so this is safe in DllMain.
void* HookImport(HMODULE module, const char* dll, const char* function, void* replacement);

}  // namespace thod2::interop
