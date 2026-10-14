// Smoke tests for the core library and DLL. No test framework on purpose:
// each check prints its result and the process exit code is the failure count.

#include "config.h"
#include "exe_id.h"
#include "patches/device_dialog.h"
#include "patches/file_redirect.h"
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
  Check(missing.log_level == LogLevel::Info && !missing.allow_unknown_exe &&
            missing.skip_device_dialog && !missing.trace_ddraw && !missing.profile &&
            missing.frame_wait_sleep && missing.frame_wait_margin_ms == 1.0f &&
            !missing.trace_files && missing.redirect_user_files &&
            missing.flip_override == 0,
        "LoadConfig defaults");

  const auto file = TestDir() / L"core_tests_config.ini";
  WriteText(file,
            "[core]\r\nlog_level = debug\r\nallow_unknown_exe = 1\r\n"
            "[display]\r\nskip_device_dialog = 0\r\n"
            "[files]\r\nredirect_user_files = 0\r\n"
            "[timing]\r\nframe_wait_sleep = 0\r\nframe_wait_margin_ms = 2.5\r\n"
            "[debug]\r\ntrace_ddraw = 1\r\nprofile = 1\r\ntrace_files = 1\r\nflip_override = 2\r\n");
  const auto parsed = thod2::core::LoadConfig(file);
  Check(parsed.log_level == LogLevel::Debug, "LoadConfig log_level = debug");
  Check(parsed.allow_unknown_exe, "LoadConfig allow_unknown_exe = 1");
  Check(!parsed.skip_device_dialog, "LoadConfig skip_device_dialog = 0");
  Check(parsed.trace_ddraw, "LoadConfig trace_ddraw = 1");
  Check(parsed.profile, "LoadConfig profile = 1");
  Check(parsed.trace_files, "LoadConfig trace_files = 1");
  Check(!parsed.redirect_user_files, "LoadConfig redirect_user_files = 0");
  Check(parsed.flip_override == 2, "LoadConfig flip_override = 2");
  Check(!parsed.frame_wait_sleep, "LoadConfig frame_wait_sleep = 0");
  Check(parsed.frame_wait_margin_ms == 2.5f, "LoadConfig frame_wait_margin_ms = 2.5");

  WriteText(file, "[timing]\r\nframe_wait_margin_ms = abc\r\n");
  Check(thod2::core::LoadConfig(file).frame_wait_margin_ms == 1.0f,
        "LoadConfig keeps default margin for unparsable value");
  WriteText(file, "[timing]\r\nframe_wait_margin_ms = 50\r\n");
  Check(thod2::core::LoadConfig(file).frame_wait_margin_ms == 1.0f,
        "LoadConfig keeps default margin for out-of-range value");
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

void TestDeviceDialog() {
  using thod2::core::patches::IsDeviceDialog;
  Check(IsDeviceDialog("Attention", 0x4A4758), "IsDeviceDialog: device dialog call site");
  Check(!IsDeviceDialog("Attention", 0x4A4000), "IsDeviceDialog: other call site is ignored");
  Check(!IsDeviceDialog("The House of the Dead 2", 0x4A4758),
        "IsDeviceDialog: other caption is ignored");
  Check(!IsDeviceDialog(nullptr, 0x4A4758), "IsDeviceDialog: null caption is ignored");
}

void TestMapUserFile() {
  using thod2::core::patches::MapUserFile;
  thod2::core::patches::RedirectPaths p;
  p.game_dir = L"C:\\Games\\THOTD2";
  p.windows_dir = L"C:\\WINDOWS";
  p.save_dir = L"C:\\Users\\u\\AppData\\Roaming\\thod2-native\\save";
  p.local_dir = L"C:\\Users\\u\\AppData\\Local\\thod2-native";

  Check(MapUserFile(L"C:\\Games\\THOTD2\\pol\\tv2.bin", p) == p.save_dir / L"pol\\tv2.bin",
        "MapUserFile: save file goes to the save folder");
  Check(MapUserFile(L"c:\\games\\thotd2\\TEX\\SCR_TOD_ITM_ITAMIDOME2.BIN", p) ==
            p.save_dir / L"tex\\scr_tod_itm_itamidome2.bin",
        "MapUserFile: comparison ignores case");
  Check(MapUserFile(L"C:\\Games\\THOTD2\\pol/../pol/bg_adv19.bin", p) ==
            p.save_dir / L"pol\\bg_adv19.bin",
        "MapUserFile: forward slashes and .. are normalized");
  Check(MapUserFile(L"C:\\WINDOWS\\d3dx.dat", p) == p.local_dir / L"d3dx.dat",
        "MapUserFile: d3dx.dat goes to the local folder");
  Check(MapUserFile(L"C:\\Games\\THOTD2\\exception.log", p) == p.local_dir / L"exception.log",
        "MapUserFile: exception.log goes to the local folder");
  Check(!MapUserFile(L"C:\\Games\\THOTD2\\pol\\b6boss1z.bin", p),
        "MapUserFile: other game data is not redirected");
  Check(!MapUserFile(L"D:\\Other\\pol\\tv2.bin", p),
        "MapUserFile: same name in another folder is not redirected");
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
  TestDeviceDialog();
  TestMapUserFile();
  TestDll();
  std::printf("%d failure(s)\n", g_failures);
  return g_failures;
}
