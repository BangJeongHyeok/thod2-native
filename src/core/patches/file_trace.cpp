#include "patches/file_trace.h"

#include "log.h"

#include <thod2/interop/iat_hook.h>

#include <windows.h>

#include <intrin.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

#pragma intrinsic(_ReturnAddress)

namespace thod2::core::patches {
namespace {

// Detailed lines for read-only opens per folder before they are only counted.
constexpr int kDetailedReadsPerFolder = 3;

std::mutex g_mutex;
std::map<std::string, int> g_reads_per_folder;

std::uintptr_t Caller(void* address) {
  return reinterpret_cast<std::uintptr_t>(address);
}

std::string Access(DWORD access) {
  std::string out;
  if (access & GENERIC_READ) out += "R";
  if (access & GENERIC_WRITE) out += "W";
  if (out.empty()) out = std::format("0x{:X}", access);
  return out;
}

const char* Disposition(DWORD disposition) {
  switch (disposition) {
    case CREATE_NEW: return "CREATE_NEW";
    case CREATE_ALWAYS: return "CREATE_ALWAYS";
    case OPEN_EXISTING: return "OPEN_EXISTING";
    case OPEN_ALWAYS: return "OPEN_ALWAYS";
    case TRUNCATE_EXISTING: return "TRUNCATE_EXISTING";
  }
  return "?";
}

std::string Folder(const char* path) {
  std::string p = path ? path : "";
  const auto slash = p.find_last_of("\\/");
  return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
}

using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                      HANDLE);
using DeleteFileAFn = BOOL(WINAPI*)(LPCSTR);
using GetPrivateProfileStringAFn = DWORD(WINAPI*)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
using GetPrivateProfileIntAFn = UINT(WINAPI*)(LPCSTR, LPCSTR, INT, LPCSTR);
using GetWindowsDirectoryAFn = UINT(WINAPI*)(LPSTR, UINT);
using GetFullPathNameAFn = DWORD(WINAPI*)(LPCSTR, DWORD, LPSTR, LPSTR*);

CreateFileAFn o_CreateFileA = nullptr;
DeleteFileAFn o_DeleteFileA = nullptr;
GetPrivateProfileStringAFn o_GetPrivateProfileStringA = nullptr;
GetPrivateProfileIntAFn o_GetPrivateProfileIntA = nullptr;
GetWindowsDirectoryAFn o_GetWindowsDirectoryA = nullptr;
GetFullPathNameAFn o_GetFullPathNameA = nullptr;

HANDLE WINAPI H_CreateFileA(LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
                            DWORD disposition, DWORD flags, HANDLE templ) {
  const auto from = Caller(_ReturnAddress());
  HANDLE h = o_CreateFileA(path, access, share, sa, disposition, flags, templ);
  const DWORD error = h == INVALID_HANDLE_VALUE ? GetLastError() : 0;

  const bool read_only = (access & GENERIC_WRITE) == 0 && disposition == OPEN_EXISTING;
  bool detail = true;
  if (read_only && h != INVALID_HANDLE_VALUE) {
    std::lock_guard lock(g_mutex);
    const int n = ++g_reads_per_folder[Folder(path)];
    detail = n <= kDetailedReadsPerFolder || n % 100 == 0;
    if (!detail) {
      SetLastError(error);
      return h;
    }
    if (n > kDetailedReadsPerFolder) {
      Log(LogLevel::Info, "[files] {} read-only opens so far in {}", n, Folder(path));
      SetLastError(error);
      return h;
    }
  }
  Log(LogLevel::Info, "[files] CreateFileA \"{}\" {} {} from 0x{:X} -> {}{}", path ? path : "(null)",
      Access(access), Disposition(disposition), from,
      h == INVALID_HANDLE_VALUE ? "FAILED" : "ok",
      h == INVALID_HANDLE_VALUE ? std::format(" (error {})", error) : std::string());
  SetLastError(error);
  return h;
}

BOOL WINAPI H_DeleteFileA(LPCSTR path) {
  const BOOL ok = o_DeleteFileA(path);
  const DWORD error = ok ? 0 : GetLastError();
  Log(LogLevel::Info, "[files] DeleteFileA \"{}\" from 0x{:X} -> {}", path ? path : "(null)",
      Caller(_ReturnAddress()), ok ? "ok" : std::format("FAILED (error {})", error));
  SetLastError(error);
  return ok;
}

DWORD WINAPI H_GetPrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR def, LPSTR out,
                                        DWORD size, LPCSTR file) {
  const DWORD n = o_GetPrivateProfileStringA(section, key, def, out, size, file);
  Log(LogLevel::Info, "[files] GetPrivateProfileStringA [{}] {} file=\"{}\" -> \"{}\"",
      section ? section : "(null)", key ? key : "(null)", file ? file : "(null)",
      out ? std::string(out, n) : std::string());
  return n;
}

UINT WINAPI H_GetPrivateProfileIntA(LPCSTR section, LPCSTR key, INT def, LPCSTR file) {
  const UINT v = o_GetPrivateProfileIntA(section, key, def, file);
  Log(LogLevel::Info, "[files] GetPrivateProfileIntA [{}] {} file=\"{}\" -> {}",
      section ? section : "(null)", key ? key : "(null)", file ? file : "(null)", v);
  return v;
}

UINT WINAPI H_GetWindowsDirectoryA(LPSTR out, UINT size) {
  const UINT n = o_GetWindowsDirectoryA(out, size);
  Log(LogLevel::Info, "[files] GetWindowsDirectoryA from 0x{:X} -> \"{}\"",
      Caller(_ReturnAddress()), out && n < size ? out : "");
  return n;
}

DWORD WINAPI H_GetFullPathNameA(LPCSTR path, DWORD size, LPSTR out, LPSTR* file_part) {
  const DWORD n = o_GetFullPathNameA(path, size, out, file_part);
  Log(LogLevel::Info, "[files] GetFullPathNameA \"{}\" from 0x{:X} -> \"{}\"", path ? path : "",
      Caller(_ReturnAddress()), out && n < size ? out : "");
  return n;
}

template <class Fn>
void Hook(const char* dll, const char* name, void* replacement, Fn& original) {
  original = reinterpret_cast<Fn>(
      interop::HookImport(GetModuleHandleW(nullptr), dll, name, replacement));
  if (!original) {
    Log(LogLevel::Warn, "trace_files: could not hook {}!{}", dll, name);
  }
}

}  // namespace

bool InstallFileTrace() {
  Hook("KERNEL32.dll", "CreateFileA", reinterpret_cast<void*>(H_CreateFileA), o_CreateFileA);
  Hook("KERNEL32.dll", "DeleteFileA", reinterpret_cast<void*>(H_DeleteFileA), o_DeleteFileA);
  Hook("KERNEL32.dll", "GetPrivateProfileStringA",
       reinterpret_cast<void*>(H_GetPrivateProfileStringA), o_GetPrivateProfileStringA);
  Hook("KERNEL32.dll", "GetPrivateProfileIntA", reinterpret_cast<void*>(H_GetPrivateProfileIntA),
       o_GetPrivateProfileIntA);
  Hook("KERNEL32.dll", "GetWindowsDirectoryA", reinterpret_cast<void*>(H_GetWindowsDirectoryA),
       o_GetWindowsDirectoryA);
  Hook("KERNEL32.dll", "GetFullPathNameA", reinterpret_cast<void*>(H_GetFullPathNameA),
       o_GetFullPathNameA);
  Log(LogLevel::Info, "trace_files: installed");
  return o_CreateFileA != nullptr;
}

}  // namespace thod2::core::patches
