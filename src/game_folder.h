// Finding the player's Catherine game files (their own Xbox 360 dump).
#pragma once

#include <cstdio>
#include <filesystem>
#include <string>

namespace catherine {

inline bool LooksLikeGameFolder(const std::filesystem::path& dir) {
  std::error_code ec;
  return !dir.empty() && std::filesystem::exists(dir / "default.xex", ec);
}

// Shows the standard macOS "choose folder" window. Returns an empty path if
// the player cancels.
inline std::filesystem::path AskForGameFolder(bool retry) {
#if defined(__APPLE__)
  std::string prompt = retry
      ? "That folder does not contain default.xex. Please choose your Catherine (Xbox 360) game folder."
      : "Choose your Catherine (Xbox 360) game folder - the one that contains default.xex.";
  std::string cmd = "/usr/bin/osascript -e 'POSIX path of (choose folder with prompt \"" + prompt +
                    "\")' 2>/dev/null";
  FILE* p = popen(cmd.c_str(), "r");
  if (!p) return {};
  char buf[4096];
  std::string out;
  while (fgets(buf, sizeof(buf), p)) out += buf;
  pclose(p);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  return out;
#else
  (void)retry;
  return {};
#endif
}

}  // namespace catherine
