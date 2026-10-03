// Catherine settings menu. See settings_menu.h.

#include "settings_menu.h"
#include "ui_style.h"

#include <rex/audio/audio_system.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/input/input.h>
#include <rex/input/input_system.h>
#include <rex/runtime.h>
#include <rex/system/guest_pause.h>
#include <rex/system/kernel_state.h>
#include <rex/ui/imgui_drawer.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

REXCVAR_DEFINE_BOOL(cath_pause_in_menu, true, "Catherine",
                    "Pause the game while the settings menu is open");
REXCVAR_DEFINE_BOOL(cath_pause_on_focus_loss, false, "Catherine",
                    "Pause the game when its window loses focus");

namespace catherine {
using namespace catherine::ui;
namespace {

bool ParseNumber(const std::string& s, double* out) {
  if (s.empty()) return false;
  char* end = nullptr;
  double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str() || *end != '\0') return false;
  *out = v;
  return true;
}

bool ValuesMatch(const std::string& a, const std::string& b) {
  double x, y;
  if (ParseNumber(a, &x) && ParseNumber(b, &y)) return std::fabs(x - y) < 1e-4;
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
  return true;
}

thread_local bool t_menu_polling = false;

}  // namespace

// ------------------------------------------------------------------ fonts --
void LoadMenuFonts(ImFontAtlas* atlas) { ui::LoadFonts(atlas); }

// ------------------------------------------------------------- lifecycle --
SettingsMenu::SettingsMenu(rex::ui::ImGuiDrawer* drawer, rex::Runtime* runtime,
                           std::filesystem::path config_path, Callbacks callbacks)
    : ImGuiDialog(drawer),
      runtime_(runtime),
      config_path_(std::move(config_path)),
      callbacks_(std::move(callbacks)) {
  BuildCategories();
  row_ = FirstSelectable(0);
}

SettingsMenu::~SettingsMenu() {
  paused_ = true;  // force resume of anything we suspended
  focus_paused_ = false;
  open_ = false;
  UpdatePause();
}

void SettingsMenu::InstallInputGate() {
  if (!runtime_) return;
  auto* input = static_cast<rex::input::InputSystem*>(runtime_->input_system());
  if (!input) return;
  auto* drawer = imgui_drawer();
  input->SetActiveCallback([this, drawer]() {
    if (t_menu_polling) return true;   // the menu itself is reading the pad
    if (open_) return false;           // block the game while the menu is open
    return !drawer->GetIO().WantCaptureMouse;
  });
}

void SettingsMenu::Toggle() {
  if (open_) {
    Close();
  } else {
    Open();
  }
}

void SettingsMenu::Open() {
  REXLOG_ERROR("MENU: open");
  open_ = true;
  confirm_ = Confirm::kNone;
  if (cat_ >= int(cats_.size())) cat_ = 0;
  if (row_ < 0 || row_ >= int(cats_[cat_].rows.size())) row_ = FirstSelectable(cat_);
  UpdatePause();
}

void SettingsMenu::Close() {
  open_ = false;
  confirm_ = Confirm::kNone;
  UpdatePause();
}

void SettingsMenu::SetFocusPaused(bool paused) {
  focus_paused_ = paused && REXCVAR_GET(cath_pause_on_focus_loss);
  UpdatePause();
}

void SettingsMenu::UpdatePause() {
  bool want = (open_ && REXCVAR_GET(cath_pause_in_menu)) || focus_paused_;
  if (want == paused_ || !runtime_) return;
  auto* audio = static_cast<rex::audio::AudioSystem*>(runtime_->audio_system());
  auto* ks = runtime_->kernel_state();
  if (want && !paused_) {
    // Freeze the game at its next frame boundary, and silence it.
    rex::system::SetGuestPaused(true);
    if (audio) audio->Pause();
    paused_ = true;
  } else if (!want && paused_) {
    if (audio) audio->Resume();
    rex::system::SetGuestPaused(false);
    paused_ = false;
  }
  (void)ks;
}

// --------------------------------------------------------------- settings --
void SettingsMenu::BuildCategories() {
  auto onoff = [](const char* cvar, bool on_first = true) {
    std::vector<Option> o = {{"On", {{cvar, "true"}}}, {"Off", {{cvar, "false"}}}};
    if (!on_first) std::swap(o[0], o[1]);
    return o;
  };
  cats_.clear();

  Category video{"Video", "Display, resolution and image quality settings.", {}};
  video.rows.push_back({RowKind::kHeader, "Display", "", {}, false, nullptr, ""});
  video.rows.push_back({RowKind::kChoice, "Display Mode",
                        "Play in a window or fill the whole screen.",
                        {{"Windowed", {{"fullscreen", "false"}}},
                         {"Fullscreen", {{"fullscreen", "true"}}}},
                        true, nullptr, ""});
  video.rows.push_back(
      {RowKind::kChoice, "Window Size", "Size of the game window when not in fullscreen.",
       {{"1280 x 720", {{"window_width", "1280"}, {"window_height", "720"}}},
        {"1600 x 900", {{"window_width", "1600"}, {"window_height", "900"}}},
        {"1920 x 1080", {{"window_width", "1920"}, {"window_height", "1080"}}},
        {"2560 x 1440", {{"window_width", "2560"}, {"window_height", "1440"}}}},
       true, nullptr, ""});
  video.rows.push_back({RowKind::kChoice, "V-Sync",
                        "Syncs presentation to your display to prevent tearing.", onoff("vsync"),
                        false, nullptr, ""});
  video.rows.push_back({RowKind::kChoice, "Keep 16:9",
                        "Adds black bars instead of stretching the picture on screens that "
                        "are not 16:9.",
                        onoff("present_letterbox"), false, nullptr, ""});
  video.rows.push_back({RowKind::kHeader, "Graphics", "", {}, false, nullptr, ""});
  video.rows.push_back(
      {RowKind::kChoice, "Render Resolution",
       "Resolution the 3D scenes are drawn at. 720p is the original Xbox 360 resolution. "
       "Higher values look sharper but need a much stronger GPU: on a base M4, 1440p runs "
       "around 18-20 FPS.",
       {{"720p (1x)", {{"draw_resolution_scale_x", "1"}, {"draw_resolution_scale_y", "1"}}},
        {"1440p (2x)", {{"draw_resolution_scale_x", "2"}, {"draw_resolution_scale_y", "2"}}},
        {"2160p (3x)", {{"draw_resolution_scale_x", "3"}, {"draw_resolution_scale_y", "3"}}}},
       true, nullptr, ""});
  video.rows.push_back(
      {RowKind::kChoice, "Upscaling Filter",
       "How the picture is scaled up to your screen. Bilinear is smooth, CAS adds "
       "sharpening, FSR is AMD FidelityFX upscaling.",
       {{"Bilinear", {{"present_effect", "bilinear"}}},
        {"CAS (Sharpen)", {{"present_effect", "cas"}}},
        {"FSR", {{"present_effect", "fsr"}}}},
       true, nullptr, ""});
  cats_.push_back(std::move(video));

  Category audio{"Audio", "Sound settings.", {}};
  audio.rows.push_back({RowKind::kHeader, "Sound", "", {}, false, nullptr, ""});
  audio.rows.push_back({RowKind::kChoice, "Game Audio", "Mute or unmute all game sound.",
                        {{"On", {{"audio_mute", "false"}}}, {"Muted", {{"audio_mute", "true"}}}},
                        false, nullptr, ""});
  audio.rows.push_back(
      {RowKind::kChoice, "Audio Buffer",
       "How much sound is prepared ahead of time. Bigger buffers stop crackles and "
       "clicks when the game hitches, at the cost of a tiny delay.",
       {{"Normal (43 ms)", {{"audio_maxqframes", "8"}}},
        {"Smooth (85 ms)", {{"audio_maxqframes", "16"}}},
        {"Extra Smooth (170 ms)", {{"audio_maxqframes", "32"}}}},
       true, nullptr, ""});
  audio.rows.push_back({RowKind::kChoice, "Remove Crackles",
                        "Smooths sudden jumps between pieces of decoded audio and fills tiny "
                        "dropouts, which are heard as clicks, crackles or quick pauses. Does "
                        "not change normal sound.",
                        {{"On", {{"audio_declick", "true"}, {"audio_fill_dropouts", "true"}}},
                         {"Off", {{"audio_declick", "false"}, {"audio_fill_dropouts", "false"}}}},
                        false, nullptr, ""});
  audio.rows.push_back({RowKind::kChoice, "Smooth Audio Gaps",
                        "If sound arrives late, fade it out and back in instead of cutting "
                        "to silence. Removes the clicking sound.",
                        onoff("audio_smooth_gaps"), false, nullptr, ""});
  cats_.push_back(std::move(audio));

  Category controls{"Controls", "Keyboard, mouse and controller settings.", {}};
  controls.rows.push_back({RowKind::kHeader, "Keyboard & Mouse", "", {}, false, nullptr, ""});
  controls.rows.push_back({RowKind::kChoice, "Keyboard Controls",
                           "Lets you play with the keyboard. A controller always works.",
                           onoff("mnk_mode"), false, nullptr, ""});
  controls.rows.push_back({RowKind::kChoice, "Mouse Camera",
                           "Move the camera with the mouse (right stick).",
                           onoff("mnk_mouse", false), false, nullptr, ""});
  controls.rows.push_back(
      {RowKind::kChoice, "Mouse Sensitivity", "How fast the mouse turns the camera.",
       {{"0.25", {{"mnk_sensitivity", "0.25"}}},
        {"0.5", {{"mnk_sensitivity", "0.5"}}},
        {"0.75", {{"mnk_sensitivity", "0.75"}}},
        {"1.0", {{"mnk_sensitivity", "1.0"}}},
        {"1.5", {{"mnk_sensitivity", "1.5"}}},
        {"2.0", {{"mnk_sensitivity", "2.0"}}},
        {"3.0", {{"mnk_sensitivity", "3.0"}}}},
       false, nullptr, ""});
  controls.rows.push_back({RowKind::kHeader, "Keyboard Layout", "", {}, false, nullptr, ""});
  auto bind = [&](const char* label, const char* cvar) {
    std::string v = rex::cvar::GetFlagByName(cvar);
    for (auto& c : v)
      if (c == ',') c = '/';
    controls.rows.push_back({RowKind::kInfo, label, "", {}, false, nullptr, v});
  };
  bind("Move", "keybind_lstick_up");
  controls.rows.back().info = "W / A / S / D";
  bind("Confirm (A)", "keybind_a");
  bind("Back (B)", "keybind_b");
  bind("X", "keybind_x");
  bind("Y", "keybind_y");
  bind("Start / Pause", "keybind_start");
  bind("Select (Back)", "keybind_back");
  bind("Camera", "keybind_rstick_up");
  controls.rows.back().info = "Arrow keys";
  cats_.push_back(std::move(controls));

  Category qol{"Gameplay", "Quality-of-life options.", {}};
  qol.rows.push_back({RowKind::kHeader, "Quality of Life", "", {}, false, nullptr, ""});
  qol.rows.push_back({RowKind::kChoice, "Pause In This Menu",
                      "Freezes the game and its sound while this menu is open.",
                      onoff("cath_pause_in_menu"), false, nullptr, ""});
  qol.rows.push_back({RowKind::kChoice, "Pause When Unfocused",
                      "Pauses the game when you click away to another app.",
                      onoff("cath_pause_on_focus_loss", false), false, nullptr, ""});
  qol.rows.push_back({RowKind::kChoice, "Skip System Popups",
                      "Automatically answers Xbox system popups, like 'Cannot connect to "
                      "Xbox LIVE' at startup.",
                      onoff("headless", false), true, nullptr, ""});
  cats_.push_back(std::move(qol));

  Category sys{"System", "Reset settings or quit.", {}};
  sys.rows.push_back({RowKind::kHeader, "System", "", {}, false, nullptr, ""});
  sys.rows.push_back({RowKind::kAction, "Restart Game",
                      "Closes and reopens Catherine, applying any settings that need a "
                      "restart. Progress since your last save will be lost.",
                      {},
                      false,
                      [this]() {
                        confirm_ = Confirm::kRestart;
                        confirm_choice_ = 1;
                      },
                      ""});
  sys.rows.push_back({RowKind::kAction, "Reset All Settings",
                      "Puts every setting in this menu back to its default.",
                      {},
                      false,
                      [this]() {
                        confirm_ = Confirm::kResetAll;
                        confirm_choice_ = 1;
                      },
                      ""});
  sys.rows.push_back({RowKind::kAction, "Close Game",
                      "Quits Catherine. Progress since your last save will be lost.",
                      {},
                      false,
                      [this]() {
                        confirm_ = Confirm::kCloseGame;
                        confirm_choice_ = 1;
                      },
                      ""});
  cats_.push_back(std::move(sys));
}

int SettingsMenu::CurrentOption(const Row& row) const {
  for (size_t i = 0; i < row.options.size(); ++i) {
    bool all = true;
    for (auto& [name, value] : row.options[i].sets) {
      if (!ValuesMatch(rex::cvar::GetFlagByName(name), value)) {
        all = false;
        break;
      }
    }
    if (all) return int(i);
  }
  return -1;
}

void SettingsMenu::ApplyOption(const Row& row, int index) {
  if (index < 0 || index >= int(row.options.size())) return;
  if (row.restart && index != CurrentOption(row)) restart_pending_ = true;
  for (auto& [name, value] : row.options[index].sets) rex::cvar::SetFlagByName(name, value);
  Save();
  UpdatePause();
}

void SettingsMenu::ResetRow(const Row& row) {
  if (row.kind != RowKind::kChoice) return;
  for (auto& o : row.options)
    for (auto& [name, value] : o.sets) {
      (void)value;
      rex::cvar::ResetToDefault(name);
    }
  Save();
  UpdatePause();
}

void SettingsMenu::Save() {
  if (!config_path_.empty()) rex::cvar::SaveConfig(config_path_);
}

// ------------------------------------------------------------------ input --
bool SettingsMenu::PollPad(uint16_t* buttons, int16_t* lx, int16_t* ly) {
  if (!runtime_) return false;
  auto* input = static_cast<rex::input::InputSystem*>(runtime_->input_system());
  if (!input) return false;
  rex::input::X_INPUT_STATE st{};
  t_menu_polling = true;
  auto r = input->GetState(0, &st);
  t_menu_polling = false;
  if (r != 0) return false;
  *buttons = st.gamepad.buttons;
  *lx = st.gamepad.thumb_lx;
  *ly = st.gamepad.thumb_ly;
  return true;
}

SettingsMenu::Nav SettingsMenu::ReadNav(ImGuiIO& io, float dt) {
  Nav n;
  // Keyboard (only while the menu is open; ImGui receives every key).
  auto key = [&](ImGuiKey k, bool repeat = true) { return ImGui::IsKeyPressed(k, repeat); };
  if (key(ImGuiKey_UpArrow) || key(ImGuiKey_W)) n.up = true;
  if (key(ImGuiKey_DownArrow) || key(ImGuiKey_S)) n.down = true;
  if (key(ImGuiKey_LeftArrow) || key(ImGuiKey_A)) n.left = true;
  if (key(ImGuiKey_RightArrow) || key(ImGuiKey_D)) n.right = true;
  if (key(ImGuiKey_Enter, false) || key(ImGuiKey_Space, false) || key(ImGuiKey_KeypadEnter, false))
    n.accept = true;
  if (key(ImGuiKey_Backspace, false)) n.back = true;
  if (key(ImGuiKey_Q, false)) n.lb = true;
  if (key(ImGuiKey_E, false)) n.rb = true;
  if (key(ImGuiKey_R, false)) n.reset = true;
  if (key(ImGuiKey_T, false)) n.restart = true;
  n.any_key = n.up || n.down || n.left || n.right || n.accept || n.back || n.lb || n.rb || n.reset;

  // Controller.
  uint16_t b = 0;
  int16_t lx = 0, ly = 0;
  if (PollPad(&b, &lx, &ly)) {
    using namespace rex::input;
    auto pressed = [&](uint16_t m) { return (b & m) && !(prev_buttons_ & m); };
    n.accept |= pressed(X_INPUT_GAMEPAD_A);
    n.back |= pressed(X_INPUT_GAMEPAD_B);
    n.lb |= pressed(X_INPUT_GAMEPAD_LEFT_SHOULDER);
    n.rb |= pressed(X_INPUT_GAMEPAD_RIGHT_SHOULDER);
    n.reset |= pressed(X_INPUT_GAMEPAD_Y);
    n.restart |= pressed(X_INPUT_GAMEPAD_X);
    int dir = 0;
    const int16_t dz = 16000;
    if ((b & X_INPUT_GAMEPAD_DPAD_UP) || ly > dz) dir = 1;
    else if ((b & X_INPUT_GAMEPAD_DPAD_DOWN) || ly < -dz) dir = 2;
    else if ((b & X_INPUT_GAMEPAD_DPAD_LEFT) || lx < -dz) dir = 3;
    else if ((b & X_INPUT_GAMEPAD_DPAD_RIGHT) || lx > dz) dir = 4;
    bool fire = false;
    if (dir != held_dir_) {
      held_dir_ = dir;
      repeat_timer_ = 0.38f;
      fire = dir != 0;
    } else if (dir != 0) {
      repeat_timer_ -= dt;
      if (repeat_timer_ <= 0.0f) {
        repeat_timer_ = 0.09f;
        fire = true;
      }
    }
    if (fire) {
      n.up |= dir == 1;
      n.down |= dir == 2;
      n.left |= dir == 3;
      n.right |= dir == 4;
    }
    n.any_pad = fire || n.accept || n.back || n.lb || n.rb || n.reset;
    prev_buttons_ = b;
  }
  (void)io;
  return n;
}

int SettingsMenu::FirstSelectable(int cat) const {
  auto& rows = cats_[cat].rows;
  for (size_t i = 0; i < rows.size(); ++i)
    if (rows[i].kind == RowKind::kChoice || rows[i].kind == RowKind::kAction) return int(i);
  return 0;
}

void SettingsMenu::MoveRow(int dir) {
  auto& rows = cats_[cat_].rows;
  int i = row_;
  for (int step = 0; step < int(rows.size()); ++step) {
    i += dir;
    if (i < 0 || i >= int(rows.size())) return;  // stop at the ends
    if (rows[i].kind == RowKind::kChoice || rows[i].kind == RowKind::kAction) {
      row_ = i;
      return;
    }
  }
}

void SettingsMenu::HandleNav(const Nav& n) {
  if (n.any_pad) last_input_pad_ = true;
  else if (n.any_key) last_input_pad_ = false;

  if (confirm_ != Confirm::kNone) {
    if (n.left || n.up) confirm_choice_ = 0;
    if (n.right || n.down) confirm_choice_ = 1;
    if (n.back) confirm_ = Confirm::kNone;
    if (n.accept) {
      Confirm c = confirm_;
      confirm_ = Confirm::kNone;
      if (confirm_choice_ == 0) {
        if (c == Confirm::kCloseGame && callbacks_.close_game) {
          Close();
          callbacks_.close_game();
        } else if (c == Confirm::kRestart && callbacks_.restart_game) {
          Close();
          callbacks_.restart_game();
        } else if (c == Confirm::kResetAll) {
          for (auto& cat : cats_)
            for (auto& r : cat.rows) ResetRow(r);
        }
      }
    }
    return;
  }

  if (n.back) {
    Close();
    return;
  }
  if (n.restart && restart_pending_) {
    confirm_ = Confirm::kRestart;
    confirm_choice_ = 1;
    return;
  }
  int ncat = int(cats_.size());
  if (n.lb) { cat_ = (cat_ + ncat - 1) % ncat; row_ = FirstSelectable(cat_); scroll_ = 0; }
  if (n.rb) { cat_ = (cat_ + 1) % ncat; row_ = FirstSelectable(cat_); scroll_ = 0; }
  if (n.up) MoveRow(-1);
  if (n.down) MoveRow(1);
  auto& row = cats_[cat_].rows[row_];
  if (row.kind == RowKind::kChoice) {
    int cur = CurrentOption(row);
    if (n.left && cur > 0) ApplyOption(row, cur - 1);
    if (n.left && cur < 0) ApplyOption(row, 0);
    if (n.right && cur < int(row.options.size()) - 1) ApplyOption(row, cur + 1);
    if (n.accept) ApplyOption(row, cur < 0 ? 0 : (cur + 1) % int(row.options.size()));
    if (n.reset) ResetRow(row);
  } else if (row.kind == RowKind::kAction && n.accept && row.action) {
    row.action();
  }
}

// ------------------------------------------------------------------- draw --
void SettingsMenu::OnDraw(ImGuiIO& io) {
  float dt = io.DeltaTime > 0 ? io.DeltaTime : 1.0f / 60.0f;

  static int draw_count = 0;
  if (draw_count++ == 0) REXLOG_ERROR("MENU: first OnDraw");
  if (!open_) {
    // Watch for Back+Start on the controller even while closed.
    uint16_t b = 0;
    int16_t lx, ly;
    bool ok = PollPad(&b, &lx, &ly);
    static uint16_t last_logged = 0xffff;
    if (b != last_logged) { REXLOG_ERROR("MENU: pad ok={} buttons={:04x}", ok, b); last_logged = b; }
    if (ok) {
      using namespace rex::input;
      bool combo = (b & X_INPUT_GAMEPAD_BACK) && (b & X_INPUT_GAMEPAD_START);
      if (combo && !combo_latched_) {
        combo_latched_ = true;
        prev_buttons_ = b;  // don't treat the held buttons as menu presses
        Open();
      } else if (!combo) {
        combo_latched_ = false;
      }
    }
  } else {
    Nav n = ReadNav(io, dt);
    if (combo_latched_) {
      // Ignore input until the opening combo is released.
      using namespace rex::input;
      if (!(prev_buttons_ & (X_INPUT_GAMEPAD_BACK | X_INPUT_GAMEPAD_START))) combo_latched_ = false;
      n = Nav{};
    }
    HandleNav(n);
  }

  float target = open_ ? 1.0f : 0.0f;
  float speed = dt / 0.18f;
  anim_ = anim_ < target ? std::min(target, anim_ + speed) : std::max(target, anim_ - speed);
  if (anim_ <= 0.0f) return;
  Draw(io);
}

void SettingsMenu::Draw(ImGuiIO& io) {
  ImVec2 disp = io.DisplaySize;
  float s = std::min(disp.x / 1280.0f, disp.y / 720.0f);
  ImVec2 org((disp.x - 1280.0f * s) * 0.5f, (disp.y - 720.0f * s) * 0.5f);
  auto P = [&](float x, float y) { return ImVec2(org.x + x * s, org.y + y * s); };
  float e = 1.0f - (1.0f - anim_) * (1.0f - anim_);  // ease-out
  float al = e;
  float slide = (1.0f - e) * -50.0f;

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(disp);
  ImGui::Begin("##catherine_settings", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  bool mouse_ok = open_ && confirm_ == Confirm::kNone;
  bool clicked = mouse_ok && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

  // Backdrop: dim the game, plus a crimson torn band behind the title.
  dl->AddRectFilled(ImVec2(0, 0), disp, Col(kBlack, 0.80f * al));
  TornRect(dl, P(0 + slide, 52), P(470 + slide, 128), Col(kCrimson, 0.55f * al), 0,
           kTornRight | kTornBottom | kTornTop, 7, 7.0f * s);

  // Title sign, tilted like the hanging signs in the main menu.
  {
    int v0 = dl->VtxBuffer.Size;
    Text(dl, g_font_marker, 54.0f * s, P(108 + slide, 52), Col(kWhite, al), "Settings");
    TornRect(dl, P(104 + slide, 116), P(318 + slide, 123), Col(kPink, al), 0,
             kTornTop | kTornBottom, 3, 3.0f * s);
    RotateSince(dl, v0, -3.0f, P(210, 90));
  }
  BarbedWire(dl, org.x + 96 * s, org.x + 1184 * s, org.y + 146 * s, s, Col(0x9a9aa0, 0.85f * al));

  // ---------------------------------------------------------- tab column --
  float tx0 = 104 + slide, tx1 = 318 + slide;
  float ty = 166;
  for (int i = 0; i < int(cats_.size()); ++i) {
    bool sel = i == cat_;
    bool is_last = i == int(cats_.size()) - 1;
    if (is_last) ty += 18;  // separate "System"
    ImVec2 a = P(tx0, ty), b = P(tx1, ty + 42);
    if (mouse_ok && ImGui::IsMouseHoveringRect(a, b) && clicked) {
      cat_ = i;
      row_ = FirstSelectable(i);
      scroll_ = 0;
    }
    if (sel) {
      TornRect(dl, a, b, Col(kCrimson, al), 9 * s, kTornRight, 11 + i, 5.0f * s);
      dl->AddRectFilled(a, ImVec2(a.x + 5 * s, b.y), Col(kPink, al), 3 * s);
    } else {
      dl->AddRectFilled(a, b, Col(kDarkGrey, 0.92f * al), 9 * s);
    }
    const char* name = cats_[i].name.c_str();
    float fs = 21.0f * s;
    ImVec2 ts = TextSize(g_font_body, fs, name);
    Text(dl, g_font_body, fs, ImVec2(a.x + 18 * s, a.y + (b.y - a.y - ts.y) * 0.5f),
         Col(sel ? kWhite : kPaper, (sel ? 1.0f : 0.8f) * al), name);
    ty += 50;
  }
  // "Close Game" shortcut, like the Skate 3 layout.
  {
    ty += 18;
    ImVec2 a = P(tx0, ty), b = P(tx1, ty + 42);
    bool hover = mouse_ok && ImGui::IsMouseHoveringRect(a, b);
    dl->AddRectFilled(a, b, Col(hover ? 0x3a1020 : kDarkGrey, 0.92f * al), 9 * s);
    float fs = 21.0f * s;
    ImVec2 ts = TextSize(g_font_body, fs, "Close Game");
    Text(dl, g_font_body, fs, ImVec2(a.x + 18 * s, a.y + (b.y - a.y - ts.y) * 0.5f),
         Col(kPink, al), "Close Game");
    if (hover && clicked) {
      confirm_ = Confirm::kCloseGame;
      confirm_choice_ = 1;
    }
  }

  // ---------------------------------------------------------- rows column --
  float rx0 = 340 + slide * 0.6f, rx1 = 930 + slide * 0.6f;
  float top = 166, bottom = 628, rh = 42, gap = 7;
  auto& rows = cats_[cat_].rows;
  // Keep the selection visible.
  float sel_y = 0;
  for (int i = 0; i < row_; ++i) sel_y += (rows[i].kind == RowKind::kHeader ? 36 : rh) + gap;
  if (sel_y - scroll_ < 0) scroll_ = sel_y;
  if (sel_y + rh - scroll_ > bottom - top) scroll_ = sel_y + rh - (bottom - top);
  dl->PushClipRect(P(rx0 - 12, top - 10), P(rx1 + 14, bottom + 6), true);
  float y = top - scroll_;
  for (int i = 0; i < int(rows.size()); ++i) {
    const Row& r = rows[i];
    if (r.kind == RowKind::kHeader) {
      ImVec2 a = P(rx0, y + 4), b = P(rx1, y + 32);
      TornRect(dl, a, b, Col(kCrimson, al), 0, kTornTop | kTornBottom, 100 + i + cat_ * 17,
               3.5f * s);
      float fs = 21.0f * s;
      ImVec2 ts = TextSize(g_font_marker, fs, r.label.c_str());
      Text(dl, g_font_marker, fs, ImVec2(a.x + 16 * s, a.y + (b.y - a.y - ts.y) * 0.5f),
           Col(kPaper, al), r.label.c_str());
      y += 36 + gap;
      continue;
    }
    bool sel = i == row_;
    ImVec2 a = P(rx0, y), b = P(rx1, y + rh);
    bool hover = mouse_ok && ImGui::IsMouseHoveringRect(a, b);
    bool selectable = r.kind != RowKind::kInfo;
    if (hover && clicked && selectable) row_ = i;
    if (sel) {
      // Pink torn tag on the left + soft pink body + pink outline.
      TornRect(dl, ImVec2(a.x - 9 * s, a.y), ImVec2(a.x + 6 * s, b.y), Col(kPink, al), 0,
               kTornLeft, 300 + i, 5.0f * s);
      dl->AddRectFilled(a, b, Col(kBlush, al), 10 * s);
      dl->AddRect(a, b, Col(kPink, al), 10 * s, 0, 2.5f * s);
    } else {
      dl->AddRectFilled(a, b, Col(kPaper, (r.kind == RowKind::kInfo ? 0.82f : 0.96f) * al),
                        10 * s);
    }
    float fs = 20.0f * s;
    ImVec2 ls = TextSize(g_font_body, fs, r.label.c_str());
    Text(dl, g_font_body, fs, ImVec2(a.x + 18 * s, a.y + (b.y - a.y - ls.y) * 0.5f),
         Col(r.kind == RowKind::kAction && r.label == "Close Game" ? kCrimson : kBlack, al),
         r.label.c_str());

    float cy = (a.y + b.y) * 0.5f;
    if (r.kind == RowKind::kChoice) {
      int cur = CurrentOption(r);
      std::string val = cur >= 0 ? r.options[cur].label : std::string("Custom");
      float lxc = org.x + (rx0 + 330) * s, rxc = org.x + (rx1 - 22) * s, ar = 11 * s;
      bool can_l = cur != 0, can_r = cur < int(r.options.size()) - 1;
      ArrowButton(dl, ImVec2(lxc, cy), ar, true, can_l, al);
      ArrowButton(dl, ImVec2(rxc, cy), ar, false, can_r, al);
      ImVec2 vs = TextSize(g_font_body, fs, val.c_str());
      Text(dl, g_font_body, fs, ImVec2((lxc + rxc) * 0.5f - vs.x * 0.5f, cy - vs.y * 0.5f),
           Col(kBlack, al), val.c_str());
      if (hover && clicked) {
        if (ImGui::IsMouseHoveringRect(ImVec2(lxc - ar * 1.6f, cy - ar * 1.6f),
                                       ImVec2(lxc + ar * 1.6f, cy + ar * 1.6f)) && can_l)
          ApplyOption(r, cur < 0 ? 0 : cur - 1);
        if (ImGui::IsMouseHoveringRect(ImVec2(rxc - ar * 1.6f, cy - ar * 1.6f),
                                       ImVec2(rxc + ar * 1.6f, cy + ar * 1.6f)) && can_r)
          ApplyOption(r, cur + 1);
      }
    } else if (r.kind == RowKind::kInfo) {
      ImVec2 vs = TextSize(g_font_body, fs, r.info.c_str());
      Text(dl, g_font_body, fs, ImVec2(b.x - 22 * s - vs.x, cy - vs.y * 0.5f),
           Col(0x444448, al), r.info.c_str());
    } else if (r.kind == RowKind::kAction) {
      ArrowButton(dl, ImVec2(b.x - 22 * s, cy), 11 * s, false, true, al);
      if (hover && clicked && r.action) r.action();
    }
    y += rh + gap;
  }
  dl->PopClipRect();
  if (mouse_ok && ImGui::IsMouseHoveringRect(P(rx0, top), P(rx1, bottom))) {
    float wheel = io.MouseWheel;
    if (wheel > 0) MoveRow(-1);
    if (wheel < 0) MoveRow(1);
  }

  // ----------------------------------------------------- description panel --
  {
    float dx0 = 950 - slide * 0.4f, dx1 = 1180 - slide * 0.4f;
    ImVec2 ha = P(dx0, 166), hb = P(dx1, 206);
    ImVec2 ba = P(dx0, 206), bb = P(dx1, 470);
    dl->AddRectFilled(ba, bb, Col(kPanel, 0.95f * al), 10 * s,
                      ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight);
    int v0 = dl->VtxBuffer.Size;
    TornRect(dl, ha, hb, Col(kPink, al), 10 * s, kTornBottom, 55, 4.0f * s);
    Text(dl, g_font_marker, 22 * s, ImVec2(ha.x + 14 * s, ha.y + 6 * s), Col(kWhite, al),
         "Description");
    RotateSince(dl, v0, 1.2f, ImVec2((ha.x + hb.x) * 0.5f, (ha.y + hb.y) * 0.5f));
    const Row& r = rows[row_];
    std::string desc = r.desc.empty() ? cats_[cat_].desc : r.desc;
    float wrap = (dx1 - dx0 - 28) * s;
    Text(dl, g_font_body, 18 * s, P(dx0 + 14, 220), Col(kBlush, al), desc.c_str(), wrap);
    if (r.restart) {
      Text(dl, g_font_hand, 19 * s, P(dx0 + 14, 420), Col(kPink, al),
           "Applies after restarting the game.", wrap);
    }
  }
  // Pending restart notice.
  if (restart_pending_) {
    ImVec2 a = P(950 - slide * 0.4f, 486), b = P(1180 - slide * 0.4f, 556);
    bool hover = mouse_ok && ImGui::IsMouseHoveringRect(a, b);
    TornRect(dl, a, b, Col(hover ? kPink : kCrimson, al), 8 * s, kTornLeft, 77, 4.0f * s);
    Text(dl, g_font_hand, 18 * s, ImVec2(a.x + 12 * s, a.y + 7 * s), Col(kPaper, al),
         "Restart to apply changes");
    KeyCap(dl, ImVec2(a.x + 12 * s, a.y + 36 * s), s, last_input_pad_ ? "X" : "T", "Restart Now", al);
    if (hover && clicked) {
      confirm_ = Confirm::kRestart;
      confirm_choice_ = 1;
    }
  }

  // ------------------------------------------------------------ hint bar --
  {
    float hx = org.x + 104 * s, hy = org.y + 664 * s;
    if (last_input_pad_) {
      hx = KeyCap(dl, ImVec2(hx, hy), s, "A", "Select", al);
      hx = KeyCap(dl, ImVec2(hx, hy), s, "B", "Back", al);
      hx = KeyCap(dl, ImVec2(hx, hy), s, "Y", "Reset to Default", al);
      hx = KeyCap(dl, ImVec2(hx, hy), s, "LB / RB", "Category", al);
    } else {
      hx = KeyCap(dl, ImVec2(hx, hy), s, "Enter", "Select", al);
      hx = KeyCap(dl, ImVec2(hx, hy), s, "Esc", "Back", al);
      hx = KeyCap(dl, ImVec2(hx, hy), s, "R", "Reset to Default", al);
      hx = KeyCap(dl, ImVec2(hx, hy), s, "Q / E", "Category", al);
    }
  }

  // --------------------------------------------------------- confirm box --
  if (confirm_ != Confirm::kNone) {
    dl->AddRectFilled(ImVec2(0, 0), disp, Col(kBlack, 0.55f * al));
    ImVec2 a = P(440, 270), b = P(840, 440);
    TornRect(dl, a, b, Col(kPaper, al), 12 * s, kTornTop | kTornBottom, 91, 5.0f * s);
    dl->AddRectFilled(a, ImVec2(b.x, a.y + 46 * s), Col(kCrimson, al), 12 * s,
                      ImDrawFlags_RoundCornersTop);
    const char* title = confirm_ == Confirm::kCloseGame ? "Close the game?"
                        : confirm_ == Confirm::kRestart ? "Restart the game?"
                                                        : "Reset all settings?";
    const char* body = confirm_ == Confirm::kResetAll
                           ? "Every option in this menu goes back to its default."
                           : "Progress since your last save will be lost.";
    Text(dl, g_font_marker, 24 * s, ImVec2(a.x + 18 * s, a.y + 8 * s), Col(kWhite, al), title);
    Text(dl, g_font_body, 18 * s, ImVec2(a.x + 18 * s, a.y + 62 * s), Col(kBlack, al), body,
         (b.x - a.x) - 36 * s);
    const char* labels[2] = {"Yes", "No"};
    for (int i = 0; i < 2; ++i) {
      ImVec2 ba(a.x + (60 + i * 170) * s, b.y - 56 * s), bb(ba.x + 110 * s, ba.y + 38 * s);
      bool sel = confirm_choice_ == i;
      bool hover = ImGui::IsMouseHoveringRect(ba, bb);
      if (hover) confirm_choice_ = i;
      dl->AddRectFilled(ba, bb, Col(sel ? kPink : 0xd8d8dc, al), 10 * s);
      ImVec2 ts = TextSize(g_font_body, 20 * s, labels[i]);
      Text(dl, g_font_body, 20 * s,
           ImVec2((ba.x + bb.x - ts.x) * 0.5f, (ba.y + bb.y - ts.y) * 0.5f),
           Col(sel ? kWhite : kBlack, al), labels[i]);
      if (hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        Nav n;
        n.accept = true;
        HandleNav(n);
      }
    }
  }
  ImGui::End();
}

}  // namespace catherine
