#pragma once

namespace thod2::core::patches {

// Development aid ([debug] trace_files = 1): logs the file system calls Hod2.exe
// makes through its import table (CreateFileA, DeleteFileA, profile string
// reads, GetWindowsDirectoryA, GetFullPathNameA) with the path, access mode and
// call site. Reads of the many game data files are summarized per folder after
// the first few. Does not change behavior.
bool InstallFileTrace();

}  // namespace thod2::core::patches
