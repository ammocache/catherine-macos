// Catherine setup screen. See setup_screen.h.

#include "setup_screen.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <rex/logging.h>

#include <cstdlib>

#include "ui_style.h"

namespace catherine {
using namespace catherine::ui;

SetupScreen::SetupScreen(rex::ui::ImGuiDrawer* drawer, std::filesystem::path initial,
                         GameCheck initial_state,
                         std::function<void(std::filesystem::path)> on_ready,
                         std::function<void()> on_quit)
    : ImGuiDialog(drawer),
      folder_(std::move(initial)),
      state_(initial_state),
      on_ready_(std::move(on_ready)),
      on_quit_(std::move(on_quit)) {
  if (state_ == GameCheck::kWrongVersion) found_hash_ = FileSha256(folder_ / "default.xex");
}

void SetupScreen::Evaluate(const std::filesystem::path& dir) {
  folder_ = dir;
  state_ = CheckGameFolder(dir, &found_hash_);
  REXLOG_ERROR("SETUP: folder '{}' -> {}", dir.string(),
               state_ == GameCheck::kOk ? "ok" : state_ == GameCheck::kMissing ? "missing" : "wrong version");
  ready_timer_ = 0.0f;
  choice_ = 0;
}

void SetupScreen::OpenPicker() {
  if (picker_open_) return;
  picker_open_ = true;
  int count = 0;
  SDL_Window** wins = SDL_GetWindows(&count);
  SDL_Window* win = (wins && count > 0) ? wins[0] : nullptr;
  std::string start = folder_.empty() ? std::string() : folder_.string();
  SDL_ShowOpenFolderDialog(
      [](void* userdata, const char* const* filelist, int) {
        auto* self = static_cast<SetupScreen*>(userdata);
        std::lock_guard<std::mutex> lock(self->pick_mutex_);
        self->picked_ = (filelist && filelist[0]) ? filelist[0] : "";
        self->picked_ready_ = true;
      },
      this, win, start.empty() ? nullptr : start.c_str(), false);
  if (wins) SDL_free(wins);
}

void SetupScreen::OnDraw(ImGuiIO& io) {
  if (done_) return;
  static bool logged = false;
  if (!logged) { logged = true; REXLOG_ERROR("SETUP: screen shown"); }
  float dt = io.DeltaTime > 0 ? io.DeltaTime : 1.0f / 60.0f;
  anim_ = std::min(1.0f, anim_ + dt / 0.25f);

  // Testing: CATH_SETUP_AUTOPICK=<folder> acts as if that folder was chosen.
  static bool autopicked = false;
  if (!autopicked && anim_ >= 1.0f && state_ != GameCheck::kOk) {
    autopicked = true;
    if (const char* p = std::getenv("CATH_SETUP_AUTOPICK")) {
      std::lock_guard<std::mutex> lock(pick_mutex_);
      picked_ = p;
      picked_ready_ = true;
    }
  }
  // Folder picked?
  {
    std::lock_guard<std::mutex> lock(pick_mutex_);
    if (picked_ready_) {
      picked_ready_ = false;
      picker_open_ = false;
      if (!picked_.empty()) Evaluate(picked_);
    }
  }

  // Input: keyboard and mouse (the controller isn't running yet at this point).
  bool accept = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) choice_ = 0;
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) choice_ = 1;
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !picker_open_ && state_ != GameCheck::kOk) {
    done_ = true;
    if (on_quit_) on_quit_();
    return;
  }

  if (state_ == GameCheck::kOk) {
    ready_timer_ += dt;
    if (ready_timer_ > 1.2f || accept) {
      done_ = true;
      if (on_ready_) on_ready_(folder_);
      return;
    }
  }

  ImVec2 disp = io.DisplaySize;
  float s = std::min(disp.x / 1280.0f, disp.y / 720.0f);
  ImVec2 org((disp.x - 1280.0f * s) * 0.5f, (disp.y - 720.0f * s) * 0.5f);
  auto P = [&](float x, float y) { return ImVec2(org.x + x * s, org.y + y * s); };
  float e = 1.0f - (1.0f - anim_) * (1.0f - anim_);
  float al = e, slide = (1.0f - e) * -40.0f;

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(disp);
  ImGui::Begin("##catherine_setup", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

  dl->AddRectFilled(ImVec2(0, 0), disp, Col(kBlack, 1.0f));
  TornRect(dl, P(0 + slide, 52), P(560 + slide, 128), Col(kCrimson, 0.6f * al), 0,
           kTornRight | kTornBottom | kTornTop, 7, 7.0f * s);
  {
    int v0 = dl->VtxBuffer.Size;
    Text(dl, g_font_marker, 50.0f * s, P(108 + slide, 56), Col(kWhite, al), "Welcome, Stray Sheep");
    TornRect(dl, P(104 + slide, 116), P(470 + slide, 123), Col(kPink, al), 0, kTornTop | kTornBottom,
             3, 3.0f * s);
    RotateSince(dl, v0, -2.5f, P(280, 90));
  }
  BarbedWire(dl, org.x + 96 * s, org.x + 1184 * s, org.y + 146 * s, s, Col(0x9a9aa0, 0.85f * al));

  // The paper card.
  ImVec2 ca = P(240, 190), cb = P(1040, 560);
  TornRect(dl, ca, cb, Col(kPaper, al), 14 * s, kTornTop | kTornBottom, 21, 6.0f * s);
  ImVec2 ha = ca, hb = ImVec2(cb.x, ca.y + 54 * s);
  dl->AddRectFilled(ha, hb, Col(state_ == GameCheck::kOk ? kPink : kCrimson, al), 14 * s,
                    ImDrawFlags_RoundCornersTop);

  const char* title = "";
  std::string body;
  const char* primary = nullptr;
  switch (state_) {
    case GameCheck::kMissing:
      title = folder_.empty() ? "Find your copy of Catherine" : "Game files not found";
      body = std::string(folder_.empty() ? "" : "Couldn't find default.xex in:\n" + folder_.string() + "\n\n") +
             "Choose the folder that contains your own extracted " + kSupportedGameName +
             " game files - the one with default.xex in it.\n\nYour files stay where they are; "
             "Catherine just remembers the location.";
      primary = "Choose Game Folder";
      break;
    case GameCheck::kWrongVersion:
      title = "Different version of the game";
      body = std::string("The default.xex in this folder isn't the one this build was made for.\n\n"
                         "Needed: ") + kSupportedGameName + "\n" + "Folder: " + folder_.string() +
             "\nFingerprint: " + (found_hash_.empty() ? std::string("unreadable") : found_hash_.substr(0, 16)) +
             "...\n\nPlease choose a folder with the retail USA release.";
      primary = "Choose Another Folder";
      break;
    case GameCheck::kOk:
      title = "Game files verified";
      body = std::string(kSupportedGameName) + " found in:\n" + folder_.string() +
             "\n\nStarting the game...";
      break;
  }
  Text(dl, g_font_marker, 26 * s, ImVec2(ha.x + 22 * s, ha.y + 11 * s), Col(kWhite, al), title);
  float wrap = (cb.x - ca.x) - 60 * s;
  Text(dl, g_font_body, 19 * s, ImVec2(ca.x + 30 * s, ca.y + 78 * s), Col(kBlack, al), body.c_str(), wrap);

  if (state_ == GameCheck::kOk) {
    // A filling pink bar while we start.
    ImVec2 ba(ca.x + 30 * s, cb.y - 52 * s), bb(cb.x - 30 * s, cb.y - 40 * s);
    dl->AddRectFilled(ba, bb, Col(0xe2e2e6, al), 6 * s);
    float t = std::min(1.0f, ready_timer_ / 1.2f);
    dl->AddRectFilled(ba, ImVec2(ba.x + (bb.x - ba.x) * t, bb.y), Col(kPink, al), 6 * s);
  } else if (primary) {
    const char* labels[2] = {primary, "Quit"};
    float bx = ca.x + 30 * s;
    for (int i = 0; i < 2; ++i) {
      ImVec2 ts = TextSize(g_font_body, 20 * s, labels[i]);
      ImVec2 ba(bx, cb.y - 70 * s), bb(bx + ts.x + 48 * s, cb.y - 26 * s);
      bool hover = ImGui::IsMouseHoveringRect(ba, bb);
      if (hover) choice_ = i;
      bool sel = choice_ == i;
      if (sel) {
        TornRect(dl, ImVec2(ba.x - 8 * s, ba.y), ImVec2(ba.x + 4 * s, bb.y), Col(kPink, al), 0, kTornLeft,
                 400 + i, 4.0f * s);
      }
      dl->AddRectFilled(ba, bb, Col(sel ? kPink : 0xd8d8dc, al), 10 * s);
      Text(dl, g_font_body, 20 * s, ImVec2(ba.x + 24 * s, (ba.y + bb.y - ts.y) * 0.5f),
           Col(sel ? kWhite : kBlack, al), labels[i]);
      if (hover && clicked) accept = true;
      bx = bb.x + 22 * s;
    }
    if (picker_open_) {
      Text(dl, g_font_hand, 19 * s, ImVec2(bx + 10 * s, cb.y - 60 * s), Col(kCrimson, al),
           "Waiting for the folder window...");
    }
    if (accept && !picker_open_) {
      if (choice_ == 0) {
        OpenPicker();
      } else if (on_quit_) {
        done_ = true;
        on_quit_();
      }
    }
  }

  float hx = org.x + 104 * s, hy = org.y + 664 * s;
  hx = KeyCap(dl, ImVec2(hx, hy), s, "Enter", "Select", al);
  hx = KeyCap(dl, ImVec2(hx, hy), s, "Arrows", "Move", al);
  KeyCap(dl, ImVec2(hx, hy), s, "Esc", "Quit", al);
  ImGui::End();
}

}  // namespace catherine
