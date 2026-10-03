// catherine - ReXGlue Recompiled Project

#include "generated/default/catherine_init.h"

#include "catherine_app.h"

#include <cstdlib>

// macOS Spaces-style fullscreen shows a flat color with this renderer, so use
// classic fullscreen. Must run before SDL initializes (static init, pre-main).
namespace {
struct FullscreenEnv {
  FullscreenEnv() { setenv("SDL_VIDEO_MAC_FULLSCREEN_SPACES", "0", 0); }
} g_fullscreen_env;
}  // namespace

REX_DEFINE_APP(catherine, CatherineApp::Create)
