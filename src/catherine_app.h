// catherine - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/cvar.h>
#include <rex/rex_app.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/window.h>

#include "game_folder.h"
#include "relaunch.h"
#include "settings_menu.h"

#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
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
  // Project defaults. Applied before catherine.toml is loaded; the config file
  // and command line always win over them.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    // Inside Catherine.app, keep settings in ~/Library/Application Support
    // (an app bundle must not modify itself).
    if (paths.config_path.string().find(".app/Contents/") != std::string::npos) {
      if (const char* home = std::getenv("HOME")) {
        std::filesystem::path dir =
            std::filesystem::path(home) / "Library" / "Application Support" / "Catherine";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        paths.config_path = dir / "catherine.toml";
      }
    }
    config_path_ = paths.config_path;
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
        // Present from the UI thread so the paused game stays visible behind
        // the settings menu (the GPU thread stops presenting while paused).
        {"host_present_from_non_ui_thread", "false"},
        // Runtime window resizes currently break presentation (flat color), so
        // the window size is fixed; size/fullscreen apply on restart instead.
        {"window_resizable", "false"},
        // Audio: 85 ms of buffered sound rides out short hitches (see the
        // underrun/declick work in patch 0007).
        {"audio_maxqframes", "16"},
        // Keyboard and mouse controls.
        {"mnk_mode", "true"},
        // Start windowed at 720p (fullscreen can be enabled in the settings menu).
        {"fullscreen", "false"},
        {"window_width", "1280"},
        {"window_height", "720"},
    };
    // Replace the registered *default* (not the value) so the config file and
    // command line still override it, and SaveConfig only writes real changes.
    for (const auto& [name, value] : kDefaults) {
      for (auto& entry : rex::cvar::GetRegistry()) {
        if (entry.name == name && entry.source == rex::cvar::Source::kDefault) {
          entry.default_value = std::string(value);
          entry.setter(value);
        }
      }
    }
  }

  // Game folder: command line > saved setting > ask the player (first run).
  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults, std::function<void(rex::PathConfig)> resume) override {
    (void)resume;
    rex::PathConfig paths = defaults;
    if (!catherine::LooksLikeGameFolder(paths.game_data_root)) {
      // The SDK reads the folder before the config file is loaded; use the
      // saved value now that it is.
      std::string saved = rex::cvar::GetFlagByName("game_data_root");
      if (catherine::LooksLikeGameFolder(saved)) paths.game_data_root = saved;
    }
    bool retry = false;
    while (!catherine::LooksLikeGameFolder(paths.game_data_root)) {
      auto chosen = catherine::AskForGameFolder(retry);
      if (chosen.empty()) break;  // cancelled: startup reports the missing folder
      paths.game_data_root = chosen;
      retry = true;
    }
    if (catherine::LooksLikeGameFolder(paths.game_data_root)) {
      std::string current = rex::cvar::GetFlagByName("game_data_root");
      if (current != paths.game_data_root.string()) {
        rex::cvar::SetFlagByName("game_data_root", paths.game_data_root.string());
        if (!config_path_.empty()) rex::cvar::SaveConfig(config_path_);
      }
    }
    return paths;
  }

  // ------------------------------------------------------------ settings menu
  void OnConfigureFonts(ImFontAtlas* atlas) override { catherine::LoadMenuFonts(atlas); }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    drawer_ = drawer;
    rex::ui::RegisterBind("bind_catherine_menu", "Escape", "Open/close the settings menu",
                          [this]() {
                            if (menu_) menu_->Toggle();
                          });
  }

  void OnPostLaunchModule(rex::system::XThread* thread) override {
    (void)thread;
    // Create the menu once everything (presenter, input) is running. Dialogs
    // added during overlay setup are not picked up by the presenter.
    if (!window()) return;
    window()->app_context().CallInUIThreadDeferred([this]() {
      if (menu_ || !drawer_) return;
      catherine::SettingsMenu::Callbacks cb;
      cb.close_game = [this]() {
        if (window()) window()->RequestClose();
      };
      cb.restart_game = [this]() {
        catherine::ScheduleRelaunch();
        if (window()) window()->RequestClose();
      };
      // Fullscreen changes are applied on the next launch (switching live
      // breaks presentation), so drop the SDK's live fullscreen handler.
      rex::cvar::UnregisterChangeCallbacks("fullscreen");
      menu_ = std::make_unique<catherine::SettingsMenu>(drawer_, runtime(), config_path_, cb);
      menu_->InstallInputGate();
    });
  }

  void OnWindowFocusChanged(bool focused) override {
    if (menu_) menu_->SetFocusPaused(!focused);
  }

  void OnShutdown() override { menu_.reset(); }

 private:
  std::filesystem::path config_path_;
  rex::ui::ImGuiDrawer* drawer_ = nullptr;
  std::unique_ptr<catherine::SettingsMenu> menu_;
};
