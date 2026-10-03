// First-run / missing-files / wrong-version setup screen, in the same style as
// the settings menu. Shown before the game starts when the game folder can't be
// used; calls `on_ready` with the verified folder (on the UI thread).
#pragma once

#include <rex/ui/imgui_dialog.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>

#include "game_version.h"

namespace catherine {

class SetupScreen : public rex::ui::ImGuiDialog {
 public:
  SetupScreen(rex::ui::ImGuiDrawer* drawer, std::filesystem::path initial, GameCheck initial_state,
              std::function<void(std::filesystem::path)> on_ready, std::function<void()> on_quit);

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  void OpenPicker();
  void Evaluate(const std::filesystem::path& dir);

  std::filesystem::path folder_;
  GameCheck state_;
  std::string found_hash_;
  std::function<void(std::filesystem::path)> on_ready_;
  std::function<void()> on_quit_;
  int choice_ = 0;  // 0 = primary button, 1 = quit
  float anim_ = 0.0f;
  float ready_timer_ = 0.0f;
  bool done_ = false;
  bool picker_open_ = false;
  std::mutex pick_mutex_;
  std::string picked_;  // set by the picker callback
  bool picked_ready_ = false;
  uint16_t prev_buttons_ = 0;
};

}  // namespace catherine
