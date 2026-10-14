#pragma once

#include <windows.h>

namespace thod2::core {

// Implementation of Thod2Core_Initialize(). `self` is the core DLL module;
// its folder holds thod2-native.ini and thod2-native.log.
// Returns a Thod2CoreResult value (include/thod2/core.h).
int Initialize(HMODULE self);

}  // namespace thod2::core
