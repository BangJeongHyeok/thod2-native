#include "core.h"

#include "config.h"
#include "exe_id.h"
#include "log.h"
#include "sha256.h"
#include "util.h"

#include <thod2/core.h>

#include <atomic>

namespace thod2::core {
namespace {

std::atomic<bool> g_initialized{false};

}  // namespace

int Initialize(HMODULE self) {
  if (g_initialized.exchange(true)) {
    return THOD2_CORE_ALREADY_INITIALIZED;
  }

  const std::filesystem::path dir = ModulePath(self).parent_path();
  const CoreConfig config = LoadConfig(dir / L"thod2-native.ini");
  OpenLog(dir / L"thod2-native.log", config.log_level);
  Log(LogLevel::Info, "thod2-native core {} loaded from {}", THOD2_VERSION, ToUtf8(dir.native()));

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
    return THOD2_CORE_OK;
  }

  if (config.allow_unknown_exe) {
    Log(LogLevel::Warn, "unknown host executable, continuing because allow_unknown_exe = 1");
    return THOD2_CORE_OK;
  }
  Log(LogLevel::Error, "unknown host executable, patches disabled (set [core] allow_unknown_exe = 1 to override)");
  return THOD2_CORE_UNKNOWN_EXE;
}

}  // namespace thod2::core
