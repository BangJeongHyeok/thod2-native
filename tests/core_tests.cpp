// Smoke tests for the core library and DLL. No test framework on purpose:
// each check prints its result and the process exit code is the failure count.

#include "config.h"
#include "exe_id.h"
#include "sha256.h"
#include "util.h"

#include <thod2/core.h>

#include <windows.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
  std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", what);
  if (!condition) {
    ++g_failures;
  }
}

std::filesystem::path TestDir() {
  return thod2::core::ModulePath(nullptr).parent_path();
}

void WriteText(const std::filesystem::path& path, std::string_view text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

std::string ReadText(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

constexpr std::string_view kAbcSha256 =
    "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD";

void TestSha256() {
  const std::string_view abc = "abc";
  const auto bytes = std::as_bytes(std::span(abc.data(), abc.size()));
  Check(thod2::core::Sha256Bytes(bytes) == std::string(kAbcSha256), "Sha256Bytes(\"abc\")");

  const auto file = TestDir() / L"core_tests_abc.txt";
  WriteText(file, abc);
  Check(thod2::core::Sha256File(file) == std::string(kAbcSha256), "Sha256File(abc file)");
  std::filesystem::remove(file);

  Check(!thod2::core::Sha256File(TestDir() / L"does_not_exist.bin"), "Sha256File(missing) fails");
}

void TestConfig() {
  using thod2::core::LogLevel;
  const auto missing = thod2::core::LoadConfig(TestDir() / L"no_such_config.ini");
  Check(missing.log_level == LogLevel::Info && !missing.allow_unknown_exe, "LoadConfig defaults");

  const auto file = TestDir() / L"core_tests_config.ini";
  WriteText(file, "[core]\r\nlog_level = debug\r\nallow_unknown_exe = 1\r\n");
  const auto parsed = thod2::core::LoadConfig(file);
  Check(parsed.log_level == LogLevel::Debug, "LoadConfig log_level = debug");
  Check(parsed.allow_unknown_exe, "LoadConfig allow_unknown_exe = 1");
  std::filesystem::remove(file);
}

void TestExeId() {
  constexpr std::string_view known =
      "1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69";
  constexpr std::string_view known_lower =
      "1d71265096661cf034568ff71184fcbbed0cd9aea0e5e2f041801c39a0ffce69";
  Check(thod2::core::FindKnownExe(known) != nullptr, "FindKnownExe(known build)");
  Check(thod2::core::FindKnownExe(known_lower) != nullptr, "FindKnownExe is case-insensitive");
  Check(thod2::core::FindKnownExe(kAbcSha256) == nullptr, "FindKnownExe(unknown) is null");
}

void TestDll() {
  const auto dir = TestDir();
  std::filesystem::remove(dir / L"thod2-native.ini");
  std::filesystem::remove(dir / L"thod2-native.log");

  HMODULE dll = LoadLibraryW((dir / L"thod2_core.dll").c_str());
  Check(dll != nullptr, "LoadLibrary(thod2_core.dll)");
  if (!dll) {
    return;
  }
  using VersionFn = const char*(__cdecl*)();
  using InitFn = int(__cdecl*)(const char*);
  auto version = reinterpret_cast<VersionFn>(GetProcAddress(dll, "Thod2Core_Version"));
  auto init = reinterpret_cast<InitFn>(GetProcAddress(dll, "Thod2Core_Initialize"));
  Check(version != nullptr && init != nullptr, "exports Thod2Core_Version / Thod2Core_Initialize");
  if (version && init) {
    Check(std::string_view(version()) == THOD2_VERSION, "Thod2Core_Version matches build");
    // The host here is core_tests.exe, which is not a supported Hod2.exe.
    Check(init("core_tests") == THOD2_CORE_UNKNOWN_EXE,
          "Initialize on unknown host returns UNKNOWN_EXE");
    Check(init("core_tests") == THOD2_CORE_ALREADY_INITIALIZED,
          "second Initialize returns ALREADY_INITIALIZED");
  }
  FreeLibrary(dll);

  const std::string log = ReadText(dir / L"thod2-native.log");
  Check(log.find("initialized by: core_tests") != std::string::npos, "log records loader info");
  Check(log.find("host SHA-256:") != std::string::npos, "log records host hash");
  Check(log.find("unknown host executable") != std::string::npos, "log records unknown host");
}

}  // namespace

int main() {
  TestSha256();
  TestConfig();
  TestExeId();
  TestDll();
  std::printf("%d failure(s)\n", g_failures);
  return g_failures;
}
