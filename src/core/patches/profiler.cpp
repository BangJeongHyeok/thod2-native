#include "patches/profiler.h"

#include "log.h"

#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace thod2::core::patches {
namespace {

// .text of the supported Hod2.exe (AGENTS.md section 3).
constexpr std::uintptr_t kHod2TextBegin = 0x401000;
constexpr std::uintptr_t kHod2TextEnd = 0x401000 + 0xC1940;

constexpr ULONGLONG kReportIntervalMs = 5000;
constexpr int kTopEntries = 12;
constexpr std::size_t kStackScanWords = 256;

DWORD g_main_thread_id = 0;

bool InHod2Text(std::uintptr_t address) {
  return address >= kHod2TextBegin && address < kHod2TextEnd;
}

// Copies the top of a (suspended) thread's stack. Kept free of C++ objects so
// it can use SEH: the range may cross into uncommitted memory.
std::size_t CopyStack(std::uintptr_t esp, std::uintptr_t* out, std::size_t words) {
  std::size_t copied = 0;
  __try {
    const auto* stack = reinterpret_cast<const std::uintptr_t*>(esp);
    for (; copied < words; ++copied) {
      out[copied] = stack[copied];
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  return copied;
}

std::string ModuleName(std::uintptr_t address, std::uintptr_t* base_out) {
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(address), &module)) {
    *base_out = 0;
    return "(no module)";
  }
  *base_out = reinterpret_cast<std::uintptr_t>(module);
  char path[MAX_PATH] = {};
  GetModuleFileNameA(module, path, MAX_PATH);
  const char* name = std::strrchr(path, '\\');
  return name ? name + 1 : path;
}

std::string Describe(std::uintptr_t address) {
  if (InHod2Text(address)) {
    return std::format("Hod2.exe 0x{:X}", address);
  }
  std::uintptr_t base = 0;
  const std::string module = ModuleName(address, &base);
  return base ? std::format("{}+0x{:X}", module, address - base) : module;
}

template <class Map>
std::vector<std::pair<typename Map::key_type, std::uint64_t>> Top(const Map& counts, int n) {
  std::vector<std::pair<typename Map::key_type, std::uint64_t>> items(counts.begin(), counts.end());
  std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
  if (items.size() > static_cast<std::size_t>(n)) items.resize(n);
  return items;
}

struct ThreadCpu {
  ULONGLONG kernel = 0;
  ULONGLONG user = 0;
};

ULONGLONG ToUll(const FILETIME& ft) {
  return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

std::map<DWORD, ThreadCpu> SnapshotThreadCpu() {
  std::map<DWORD, ThreadCpu> result;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE) return result;
  THREADENTRY32 te{sizeof(te)};
  const DWORD pid = GetCurrentProcessId();
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
    if (te.th32OwnerProcessID != pid) continue;
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
    if (!thread) continue;
    FILETIME created, exited, kernel, user;
    if (GetThreadTimes(thread, &created, &exited, &kernel, &user)) {
      result[te.th32ThreadID] = {ToUll(kernel), ToUll(user)};
    }
    CloseHandle(thread);
  }
  CloseHandle(snap);
  return result;
}

DWORD WINAPI ProfilerThread(LPVOID) {
  HANDLE target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                             FALSE, g_main_thread_id);
  if (!target) {
    Log(LogLevel::Warn, "profiler: cannot open main thread {}", g_main_thread_id);
    return 0;
  }
  // High resolution waitable timer: 1 ms period without timeBeginPeriod.
  HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                        TIMER_ALL_ACCESS);
  if (!timer) {
    Log(LogLevel::Warn, "profiler: high resolution timer not available, using Sleep(1)");
  }

  std::unordered_map<std::uintptr_t, std::uint64_t> by_ip;
  std::unordered_map<std::string, std::uint64_t> by_module;
  std::unordered_map<std::uintptr_t, std::uint64_t> by_hod2_caller;
  std::uint64_t samples = 0;
  ULONGLONG interval_start = GetTickCount64();
  auto cpu_before = SnapshotThreadCpu();
  std::uintptr_t stack[kStackScanWords];

  for (;;) {
    if (timer) {
      LARGE_INTEGER due{};
      due.QuadPart = -10000;  // 1 ms, relative
      SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
      WaitForSingleObject(timer, INFINITE);
    } else {
      Sleep(1);
    }

    // While the target is suspended: no allocation, no logging, no locks.
    if (SuspendThread(target) == static_cast<DWORD>(-1)) {
      break;  // thread is gone
    }
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_CONTROL;
    const BOOL got = GetThreadContext(target, &ctx);
    const std::size_t words = got ? CopyStack(ctx.Esp, stack, kStackScanWords) : 0;
    ResumeThread(target);
    if (!got) continue;

    const std::uintptr_t ip = ctx.Eip;
    ++samples;
    ++by_ip[ip];
    if (!InHod2Text(ip)) {
      for (std::size_t i = 0; i < words; ++i) {
        if (InHod2Text(stack[i])) {
          ++by_hod2_caller[stack[i]];
          break;
        }
      }
    }

    const ULONGLONG now = GetTickCount64();
    if (now - interval_start < kReportIntervalMs) continue;

    // ---- report ----
    for (const auto& [address, count] : by_ip) {
      std::uintptr_t base = 0;
      by_module[InHod2Text(address) ? "Hod2.exe" : ModuleName(address, &base)] += count;
    }
    Log(LogLevel::Info, "[profile] {} samples of main thread {} in {} ms", samples,
        g_main_thread_id, now - interval_start);
    for (const auto& [module, count] : Top(by_module, kTopEntries)) {
      Log(LogLevel::Info, "[profile]   module {:<20} {:5.1f}%", module, count * 100.0 / samples);
    }
    for (const auto& [address, count] : Top(by_ip, kTopEntries)) {
      Log(LogLevel::Info, "[profile]   ip     {:<36} {:5.1f}%", Describe(address),
          count * 100.0 / samples);
    }
    for (const auto& [address, count] : Top(by_hod2_caller, kTopEntries)) {
      Log(LogLevel::Info, "[profile]   caller Hod2.exe 0x{:X} (nearest on stack) {:5.1f}%", address,
          count * 100.0 / samples);
    }
    const auto cpu_after = SnapshotThreadCpu();
    const double wall_100ns = static_cast<double>(now - interval_start) * 10000.0;
    for (const auto& [tid, after] : cpu_after) {
      const auto it = cpu_before.find(tid);
      const ThreadCpu before = it != cpu_before.end() ? it->second : ThreadCpu{};
      const double user = (after.user - before.user) * 100.0 / wall_100ns;
      const double kernel = (after.kernel - before.kernel) * 100.0 / wall_100ns;
      if (user + kernel >= 0.5) {
        Log(LogLevel::Info, "[profile]   thread {:5}{} cpu {:5.1f}% (user {:5.1f}%, kernel {:5.1f}%)",
            tid,
            tid == g_main_thread_id ? " (main)" : (tid == GetCurrentThreadId() ? " (profiler)" : ""),
            user + kernel, user, kernel);
      }
    }
    cpu_before = cpu_after;
    by_ip.clear();
    by_module.clear();
    by_hod2_caller.clear();
    samples = 0;
    interval_start = now;
  }
  if (timer) CloseHandle(timer);
  CloseHandle(target);
  return 0;
}

}  // namespace

bool StartProfiler(DWORD main_thread_id) {
  g_main_thread_id = main_thread_id;
  HANDLE thread = CreateThread(nullptr, 0, ProfilerThread, nullptr, 0, nullptr);
  if (!thread) {
    Log(LogLevel::Warn, "profiler: could not start thread");
    return false;
  }
  CloseHandle(thread);
  Log(LogLevel::Info, "profiler: sampling main thread {} every ~1 ms", main_thread_id);
  return true;
}

}  // namespace thod2::core::patches
