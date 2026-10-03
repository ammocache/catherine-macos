// Catherine settings menu: a player-facing, controller-friendly settings screen
// styled after the game (torn paper, barbed wire, pink/crimson palette).
//
// Opens with Esc (keyboard) or Back+Start held together (controller).
// Pauses the game while open (configurable) and saves every change to the
// config file immediately.

#pragma once

#include <rex/ui/imgui_dialog.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct ImFont;
struct ImFontAtlas;
struct ImDrawList;

namespace rex {
class Runtime;
}

namespace catherine {

// Fonts are loaded once from OnConfigureFonts and shared with the menu.
void LoadMenuFonts(ImFontAtlas* atlas);

class SettingsMenu : public rex::ui::ImGuiDialog {
 public:
  struct Callbacks {
    std::function<void()> close_game;
    std::function<void()> restart_game;
  };

  SettingsMenu(rex::ui::ImGuiDrawer* drawer, rex::Runtime* runtime,
               std::filesystem::path config_path, Callbacks callbacks);
  ~SettingsMenu() override;

  void Toggle();
  bool is_open() const { return open_; }

  // Installs the input gate (game input is blocked while the menu is open).
  void InstallInputGate();
  // Pause/resume for reasons other than the menu (e.g. window focus loss).
  void SetFocusPaused(bool paused);

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  struct Option {
    std::string label;
    std::vector<std::pair<std::string, std::string>> sets;  // cvar -> value
  };
  enum class RowKind { kHeader, kChoice, kAction, kInfo };
  struct Row {
    RowKind kind = RowKind::kChoice;
    std::string label;
    std::string desc;
    std::vector<Option> options;
    bool restart = false;
    std::function<void()> action;
    std::string info;
  };
  struct Category {
    std::string name;
    std::string desc;
    std::vector<Row> rows;
  };
  struct Nav {
    bool up = false, down = false, left = false, right = false;
    bool accept = false, back = false, lb = false, rb = false, reset = false, restart = false;
    bool any_pad = false, any_key = false;
  };
  enum class Confirm { kNone, kCloseGame, kResetAll, kRestart };

  void BuildCategories();
  int CurrentOption(const Row& row) const;
  void ApplyOption(const Row& row, int index);
  void ResetRow(const Row& row);
  void Save();
  void Open();
  void Close();
  void UpdatePause();
  bool PollPad(uint16_t* buttons, int16_t* lx, int16_t* ly);
  Nav ReadNav(ImGuiIO& io, float dt);
  void HandleNav(const Nav& nav);
  void MoveRow(int dir);
  int FirstSelectable(int cat) const;
  void Draw(ImGuiIO& io);

  rex::Runtime* runtime_;
  std::filesystem::path config_path_;
  Callbacks callbacks_;
  std::vector<Category> cats_;

  bool open_ = false;
  float anim_ = 0.0f;  // 0 closed .. 1 open
  int cat_ = 0;
  int row_ = 0;
  float scroll_ = 0.0f;
  Confirm confirm_ = Confirm::kNone;
  int confirm_choice_ = 1;  // 0 = yes, 1 = no
  bool last_input_pad_ = false;
  bool restart_pending_ = false;  // a restart-only setting was changed this session

  // Controller edge detection / key repeat.
  uint16_t prev_buttons_ = 0;
  int held_dir_ = 0;  // 1 up 2 down 3 left 4 right
  float repeat_timer_ = 0.0f;
  bool combo_latched_ = false;

  // Pause state.
  bool paused_ = false;
  bool focus_paused_ = false;
};

}  // namespace catherine
