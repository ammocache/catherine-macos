// Which copy of the game this build supports, and how to check a player's copy.
#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#endif

namespace catherine {

// SHA-256 of default.xex from the retail Catherine (USA) Xbox 360 release that
// this project's recompiled code is generated from. Other versions (other
// regions, the demo, patched executables) are not supported.
inline constexpr const char* kSupportedXexSha256 =
    "571ae5e03bf7385d5cb4ccc244d5fcf4b1bb895fb2d8f373bd562ab59e8f9647";
inline constexpr const char* kSupportedGameName = "Catherine (USA, Xbox 360)";

// Returns the lowercase hex SHA-256 of a file, or "" if it can't be read.
inline std::string FileSha256(const std::filesystem::path& path) {
#if defined(__APPLE__)
  FILE* f = std::fopen(path.string().c_str(), "rb");
  if (!f) return {};
  CC_SHA256_CTX ctx;
  CC_SHA256_Init(&ctx);
  unsigned char buf[1 << 16];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) CC_SHA256_Update(&ctx, buf, CC_LONG(n));
  std::fclose(f);
  unsigned char digest[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256_Final(digest, &ctx);
  static const char* hex = "0123456789abcdef";
  std::string out;
  for (unsigned char b : digest) {
    out += hex[b >> 4];
    out += hex[b & 15];
  }
  return out;
#else
  (void)path;
  return {};
#endif
}

enum class GameCheck { kOk, kMissing, kWrongVersion };

inline GameCheck CheckGameFolder(const std::filesystem::path& dir, std::string* found_hash = nullptr) {
  std::error_code ec;
  if (dir.empty() || !std::filesystem::exists(dir / "default.xex", ec)) return GameCheck::kMissing;
  std::string h = FileSha256(dir / "default.xex");
  if (found_hash) *found_hash = h;
  return h == kSupportedXexSha256 ? GameCheck::kOk : GameCheck::kWrongVersion;
}

}  // namespace catherine
