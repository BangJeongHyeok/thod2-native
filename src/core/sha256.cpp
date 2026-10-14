#include "sha256.h"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <vector>

namespace thod2::core {
namespace {

// Thin RAII wrapper over a CNG SHA-256 hash object.
class Sha256 {
 public:
  Sha256() {
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg_, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
      alg_ = nullptr;
      return;
    }
    if (!BCRYPT_SUCCESS(BCryptCreateHash(alg_, &hash_, nullptr, 0, nullptr, 0, 0))) {
      hash_ = nullptr;
    }
  }
  ~Sha256() {
    if (hash_) BCryptDestroyHash(hash_);
    if (alg_) BCryptCloseAlgorithmProvider(alg_, 0);
  }
  Sha256(const Sha256&) = delete;
  Sha256& operator=(const Sha256&) = delete;

  bool ok() const { return hash_ != nullptr; }

  bool Update(const void* data, size_t size) {
    return BCRYPT_SUCCESS(BCryptHashData(
        hash_, static_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(size), 0));
  }

  std::optional<std::string> Finish() {
    std::array<UCHAR, 32> digest{};
    if (!BCRYPT_SUCCESS(BCryptFinishHash(hash_, digest.data(), static_cast<ULONG>(digest.size()), 0))) {
      return std::nullopt;
    }
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(digest.size() * 2);
    for (UCHAR b : digest) {
      out.push_back(kHex[b >> 4]);
      out.push_back(kHex[b & 0xF]);
    }
    return out;
  }

 private:
  BCRYPT_ALG_HANDLE alg_ = nullptr;
  BCRYPT_HASH_HANDLE hash_ = nullptr;
};

}  // namespace

std::optional<std::string> Sha256Bytes(std::span<const std::byte> data) {
  Sha256 sha;
  if (!sha.ok() || !sha.Update(data.data(), data.size())) {
    return std::nullopt;
  }
  return sha.Finish();
}

std::optional<std::string> Sha256File(const std::filesystem::path& path) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return std::nullopt;
  }
  Sha256 sha;
  std::vector<std::byte> buffer(64 * 1024);
  bool ok = sha.ok();
  while (ok) {
    DWORD read = 0;
    if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
      ok = false;
      break;
    }
    if (read == 0) {
      break;
    }
    ok = sha.Update(buffer.data(), read);
  }
  CloseHandle(file);
  if (!ok) {
    return std::nullopt;
  }
  return sha.Finish();
}

}  // namespace thod2::core
