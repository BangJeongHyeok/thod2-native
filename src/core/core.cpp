#include "core.h"

#include "config.h"
#include "exe_id.h"
#include "log.h"
#include "patches/ddraw_trace.h"
#include "patches/device_dialog.h"
#include "patches/profiler.h"
#include "sha256.h"
#include "util.h"

#include <thod2/core.h>

#include <atomic>

namespace thod2::core {
namespace {

std::atomic<bool> g_initialized{false};

// Window class of Hod2.exe's main window (created at 0x4A588B).
constexpr char kGameWindowClass[] = "The House of the Dead 2";

// Installs the patches enabled in the config. Runs only on a supported (or
// explicitly allowed) Hod2.exe, before the game creates its main window.
void ApplyPatches(const CoreConfig& config) {
  if (config.skip_device_dialog) {
    patches::InstallSkipDeviceDialog();
  } else {
    Log(LogLevel::Info, "skip_device_dialog: disabled in config");
  }
  if (config.trace_ddraw) {
    patches::InstallDDrawTrace();
  }
  if (config.profile) {
    // Initialize runs on Hod2.exe's main thread (first GetCommandLineA).
    patches::StartProfiler(GetCurrentThreadId());
  }
}

}  // namespace

int Initialize(HMODULE self, const char* loader_info) {
  if (g_initialized.exchange(true)) {
    return THOD2_CORE_ALREADY_INITIALIZED;
  }

  const std::filesystem::path dir = ModulePath(self).parent_path();
  const CoreConfig config = LoadConfig(dir / L"thod2-native.ini");
  OpenLog(dir / L"thod2-native.log", config.log_level);
  Log(LogLevel::Info, "thod2-native core {} loaded from {}", THOD2_VERSION, ToUtf8(dir.native()));
  Log(LogLevel::Info, "initialized by: {} (thread {})", loader_info ? loader_info : "(unknown)",
      GetCurrentThreadId());
  // Tells whether initialization ran before Hod2.exe created its main window.
  Log(LogLevel::Info, "game main window exists: {}",
      FindWindowA(kGameWindowClass, nullptr) ? "yes" : "no");

  const std::filesystem::path host = ModulePath(nullptr);
  const auto hash = Sha256File(host);
  if (!hash) {
    Log(LogLevel::Error, "could not hash host executable {}", ToUtf8(host.native()));
    return THOD2_CORE_FAILED;
  }
  Log(LogLevel::Info, "host executable: {}", ToUtf8(host.native()));
  Log(LogLevel::Info, "host SHA-256: {}", *hash);

  if (const KnownExe* known = FindKnownExe(*hash)) {
    Log(LogLevel::Info, "supported build: {}", known->name);
  } else if (config.allow_unknown_exe) {
    Log(LogLevel::Warn, "unknown host executable, continuing because allow_unknown_exe = 1");
  } else {
    Log(LogLevel::Error, "unknown host executable, patches disabled (set [core] allow_unknown_exe = 1 to override)");
    return THOD2_CORE_UNKNOWN_EXE;
  }

  ApplyPatches(config);
  return THOD2_CORE_OK;
}

}  // namespace thod2::core
