#pragma once

#include <windows.h>

namespace thod2::core::patches {

// Development aid ([debug] profile = 1): a sampling profiler that runs on its
// own thread. About every millisecond it suspends `main_thread_id`, records the
// instruction pointer and the nearest Hod2.exe return address on the stack,
// and resumes it. Every few seconds it logs where the time went (per module,
// top addresses, top Hod2.exe callers) and the CPU time of every thread.
// Does not change the system timer resolution.
bool StartProfiler(DWORD main_thread_id);

}  // namespace thod2::core::patches
