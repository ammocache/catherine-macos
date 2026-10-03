// catherine - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/cvar.h>
#include <rex/rex_app.h>

#include <cstdlib>
#include <string_view>
#include <utility>

class CatherineApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<CatherineApp>(new CatherineApp(ctx, "catherine",
        PPCImageConfig));
  }

 protected:
  // Project defaults. Applied before catherine.toml is loaded, and only to
  // settings the player has not set on the command line, so the config file
  // and command line always win.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    (void)paths;
    // macOS Spaces-style fullscreen shows a flat color with this renderer;
    // use classic (non-Spaces) fullscreen instead.
    setenv("SDL_VIDEO_MAC_FULLSCREEN_SPACES", "0", 0);
    static constexpr std::pair<std::string_view, std::string_view> kDefaults[] = {
        // Graphics plugin (Xenia-derived Vulkan backend via MoltenVK).
        {"gpu_plugin", "xenos"},
        // Fix for the black screen when gameplay starts: measure the real size
        // of unclipped full-screen clears instead of assuming the whole EDRAM.
        {"execute_unclipped_draw_vs_on_cpu", "true"},
        {"execute_unclipped_draw_vs_on_cpu_with_scissor", "true"},
        // Keyboard and mouse controls.
        {"mnk_mode", "true"},
        // Fullscreen is currently broken on macOS; start windowed at 720p.
        {"fullscreen", "false"},
        {"window_width", "1280"},
        {"window_height", "720"},
    };
    for (const auto& [name, value] : kDefaults) {
      if (rex::cvar::GetFlagSource(name) == rex::cvar::Source::kDefault) {
        rex::cvar::SetFlagByName(name, value);
      }
    }
  }
};
