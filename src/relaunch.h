// Relaunch Catherine after it exits (used by "Restart Game" in the settings menu).
#pragma once

#include <cstdlib>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <unistd.h>
extern char** environ;
#endif

namespace catherine {

inline std::string ShellQuote(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') out += "'\\''";
    else out += c;
  }
  return out + "'";
}

// Starts a small background shell that waits for this process to exit and
// then opens the app again with the same command-line arguments.
inline bool ScheduleRelaunch() {
#if defined(__APPLE__)
  char exe[4096];
  uint32_t size = sizeof(exe);
  if (_NSGetExecutablePath(exe, &size) != 0) return false;
  std::string exe_path(exe);
  // .../Catherine.app/Contents/MacOS/catherine -> .../Catherine.app
  std::string bundle;
  auto pos = exe_path.find(".app/Contents/MacOS/");
  if (pos != std::string::npos) bundle = exe_path.substr(0, pos + 4);

  std::string args;
  int argc = *_NSGetArgc();
  char** argv = *_NSGetArgv();
  for (int i = 1; i < argc; ++i) args += " " + ShellQuote(argv[i]);

  std::string launch;
  if (!bundle.empty()) {
    launch = "/usr/bin/open -n";
    for (const char* var : {"CATH_FPS_LOG"}) {  // test automation is not carried over
      if (const char* v = std::getenv(var)) launch += " --env " + ShellQuote(std::string(var) + "=" + v);
    }
    launch += " " + ShellQuote(bundle) + " --args" + args;
  } else {
    launch = ShellQuote(exe_path) + args + " >/dev/null 2>&1 &";
  }
  std::string script = "unset CATH_AUTOMATION CATH_AUTOMATION_INPUT; while kill -0 " + std::to_string(getpid()) +
                       " 2>/dev/null; do sleep 0.2; done; " + launch;
  const char* sh_argv[] = {"/bin/sh", "-c", script.c_str(), nullptr};
  pid_t pid;
  return posix_spawn(&pid, "/bin/sh", nullptr, nullptr, const_cast<char**>(sh_argv), environ) == 0;
#else
  return false;
#endif
}

}  // namespace catherine
