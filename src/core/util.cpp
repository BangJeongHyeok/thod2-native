#include "util.h"

#include <objbase.h>
#include <shlobj.h>
#include <knownfolders.h>

#include <vector>

namespace thod2::core {

std::string ToUtf8(std::wstring_view text) {
  if (text.empty()) {
    return {};
  }
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                       nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size,
                      nullptr, nullptr);
  return out;
}

std::filesystem::path ModulePath(HMODULE module) {
  std::vector<wchar_t> buffer(MAX_PATH);
  for (;;) {
    const DWORD length =
        GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return {};
    }
    if (length < buffer.size()) {
      return std::filesystem::path(std::wstring(buffer.data(), length));
    }
    buffer.resize(buffer.size() * 2);
  }
}

std::filesystem::path KnownFolderPath(REFKNOWNFOLDERID id) {
  PWSTR raw = nullptr;
  std::filesystem::path out;
  if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw))) {
    out = raw;
  }
  CoTaskMemFree(raw);
  return out;
}

std::filesystem::path LocalDataDir() {
  return KnownFolderPath(FOLDERID_LocalAppData) / L"thod2-native";
}

}  // namespace thod2::core
