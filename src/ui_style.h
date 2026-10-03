// Catherine UI style kit: palette, fonts and the torn-paper drawing helpers
// shared by the settings menu and the setup screen.
#pragma once

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace catherine::ui {

// ---------------------------------------------------------------- palette --
constexpr uint32_t kPink = 0xfd016a;      // hot pink: selection, arrows, accents
constexpr uint32_t kCrimson = 0x8d0037;   // deep crimson: headers, active tab
constexpr uint32_t kPaper = 0xf5f9fc;     // off-white: rows
constexpr uint32_t kBlush = 0xfecbef;     // soft pink: descriptions, hints
constexpr uint32_t kBlack = 0x000000;
constexpr uint32_t kWhite = 0xffffff;
constexpr uint32_t kGrey = 0x808080;      // inactive arrows
constexpr uint32_t kDarkGrey = 0x2a2a2f;  // tabs
constexpr uint32_t kPanel = 0x1b1b20;     // description body

inline ImU32 Col(uint32_t rgb, float a = 1.0f) {
  a = std::clamp(a, 0.0f, 1.0f);
  return IM_COL32((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, int(a * 255.0f));
}

// ------------------------------------------------------------------ fonts --
inline ImFont* g_font_marker = nullptr;  // Permanent Marker: titles, headers
inline ImFont* g_font_hand = nullptr;    // Kalam Bold: hints, accents
inline ImFont* g_font_body = nullptr;    // Poppins Medium: rows, descriptions

inline std::filesystem::path ExecutableDir() {
#if defined(__APPLE__)
  char buf[4096];
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) == 0) {
    std::error_code ec;
    auto p = std::filesystem::weakly_canonical(buf, ec);
    return (ec ? std::filesystem::path(buf) : p).parent_path();
  }
#endif
  return std::filesystem::current_path();
}

inline std::filesystem::path FindFontDir() {
  auto exe = ExecutableDir();
  std::vector<std::filesystem::path> candidates = {
      exe / ".." / "Resources" / "fonts",  // inside Catherine.app
      exe / "assets" / "fonts",
#ifdef CATHERINE_ASSET_DIR
      std::filesystem::path(CATHERINE_ASSET_DIR) / "fonts",  // dev build
#endif
  };
  for (auto& c : candidates) {
    std::error_code ec;
    if (std::filesystem::exists(c / "Poppins-Medium.ttf", ec)) return c;
  }
  return {};
}

// ------------------------------------------------------------- draw kit --
// Deterministic pseudo-random numbers so torn edges don't flicker.
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed * 2654435761u + 0x9e3779b9u) {}
  float Next() {  // 0..1
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return float(s & 0xffffff) / float(0xffffff);
  }
};

enum TornSide : int { kTornLeft = 1, kTornRight = 2, kTornTop = 4, kTornBottom = 8 };

// A rounded rectangle with "torn paper" teeth sticking out of the chosen sides.
// Untorn corners are rounded (modern look); torn sides are jagged (Catherine).
inline void TornRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float rounding, int torn,
              uint32_t seed, float amp) {
  ImDrawFlags corners = 0;
  if (!(torn & (kTornLeft | kTornTop))) corners |= ImDrawFlags_RoundCornersTopLeft;
  if (!(torn & (kTornRight | kTornTop))) corners |= ImDrawFlags_RoundCornersTopRight;
  if (!(torn & (kTornLeft | kTornBottom))) corners |= ImDrawFlags_RoundCornersBottomLeft;
  if (!(torn & (kTornRight | kTornBottom))) corners |= ImDrawFlags_RoundCornersBottomRight;
  if (corners == 0) corners = ImDrawFlags_RoundCornersNone;
  dl->AddRectFilled(a, b, col, rounding, corners);
  Rng rng(seed);
  auto teeth = [&](ImVec2 p0, ImVec2 p1, ImVec2 outward) {
    float len = std::sqrt((p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y));
    ImVec2 dir((p1.x - p0.x) / len, (p1.y - p0.y) / len);
    float t = 0.0f;
    while (t < len) {
      float w = amp * (1.2f + rng.Next() * 1.6f);
      float t1 = std::min(len, t + w);
      float h = amp * (0.35f + rng.Next() * 0.75f);
      float mid = t + (t1 - t) * (0.3f + rng.Next() * 0.4f);
      ImVec2 q0(p0.x + dir.x * t, p0.y + dir.y * t);
      ImVec2 q1(p0.x + dir.x * t1, p0.y + dir.y * t1);
      ImVec2 apex(p0.x + dir.x * mid + outward.x * h, p0.y + dir.y * mid + outward.y * h);
      dl->AddTriangleFilled(q0, apex, q1, col);
      t = t1;
    }
  };
  if (torn & kTornTop) teeth(ImVec2(a.x, a.y), ImVec2(b.x, a.y), ImVec2(0, -1));
  if (torn & kTornBottom) teeth(ImVec2(a.x, b.y), ImVec2(b.x, b.y), ImVec2(0, 1));
  if (torn & kTornLeft) teeth(ImVec2(a.x, a.y), ImVec2(a.x, b.y), ImVec2(-1, 0));
  if (torn & kTornRight) teeth(ImVec2(b.x, a.y), ImVec2(b.x, b.y), ImVec2(1, 0));
}

// Barbed wire: a slightly wavy strand with little X knots.
inline void BarbedWire(ImDrawList* dl, float x0, float x1, float y, float s, ImU32 col) {
  const int n = 48;
  ImVec2 pts[n + 1];
  for (int i = 0; i <= n; ++i) {
    float x = x0 + (x1 - x0) * i / n;
    pts[i] = ImVec2(x, y + std::sin(i * 0.9f) * 1.6f * s);
  }
  dl->AddPolyline(pts, n + 1, col, ImDrawFlags_None, 1.6f * s);
  for (float x = x0 + 22 * s; x < x1 - 10 * s; x += 46 * s) {
    float k = 5.0f * s;
    dl->AddLine(ImVec2(x - k, y - k), ImVec2(x + k, y + k), col, 1.8f * s);
    dl->AddLine(ImVec2(x - k, y + k), ImVec2(x + k, y - k), col, 1.8f * s);
  }
}

// Rotates every vertex added since `start` around `center` (for tilted signs).
inline void RotateSince(ImDrawList* dl, int start, float degrees, ImVec2 center) {
  float r = degrees * 3.14159265f / 180.0f, c = std::cos(r), sn = std::sin(r);
  for (int i = start; i < dl->VtxBuffer.Size; ++i) {
    ImVec2& p = dl->VtxBuffer[i].pos;
    float x = p.x - center.x, y = p.y - center.y;
    p = ImVec2(center.x + x * c - y * sn, center.y + x * sn + y * c);
  }
}

inline ImVec2 TextSize(ImFont* f, float size, const char* text) {
  if (!f) return ImVec2(0, 0);
  return f->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
}

inline void Text(ImDrawList* dl, ImFont* f, float size, ImVec2 pos, ImU32 col, const char* text,
          float wrap = 0.0f) {
  dl->AddText(f, size, pos, col, text, nullptr, wrap);
}

// Round arrow button: pink when usable, grey when at the end of the list.
inline void ArrowButton(ImDrawList* dl, ImVec2 c, float r, bool left, bool active, float alpha) {
  dl->AddCircleFilled(c, r, Col(active ? kPink : 0xd2d2d6, alpha), 20);
  float k = r * 0.45f;
  ImU32 tc = Col(kWhite, alpha);
  if (left) {
    dl->AddTriangleFilled(ImVec2(c.x - k, c.y), ImVec2(c.x + k * 0.7f, c.y - k),
                          ImVec2(c.x + k * 0.7f, c.y + k), tc);
  } else {
    dl->AddTriangleFilled(ImVec2(c.x + k, c.y), ImVec2(c.x - k * 0.7f, c.y + k),
                          ImVec2(c.x - k * 0.7f, c.y - k), tc);
  }
}

// Small key cap for the hint bar.
inline float KeyCap(ImDrawList* dl, ImVec2 pos, float s, const char* key, const char* label,
             float alpha) {
  float fs = 15.0f * s;
  ImVec2 ks = TextSize(g_font_body, fs, key);
  float pad = 7.0f * s, h = 24.0f * s;
  ImVec2 a(pos.x, pos.y), b(pos.x + ks.x + pad * 2, pos.y + h);
  dl->AddRectFilled(a, b, Col(kPaper, alpha), 6.0f * s);
  Text(dl, g_font_body, fs, ImVec2(a.x + pad, a.y + (h - ks.y) * 0.5f), Col(kBlack, alpha), key);
  float lx = b.x + 7.0f * s;
  float ls = 17.0f * s;
  ImVec2 lsz = TextSize(g_font_hand, ls, label);
  Text(dl, g_font_hand, ls, ImVec2(lx, a.y + (h - lsz.y) * 0.5f), Col(kBlush, alpha), label);
  return lx + lsz.x + 22.0f * s;
}


// Loads the menu fonts into the ImGui atlas (call from OnConfigureFonts).
inline void LoadFonts(ImFontAtlas* atlas) {
  auto dir = FindFontDir();
  if (dir.empty()) return;
  ImFontConfig cfg;
  cfg.OversampleH = 2;
  cfg.OversampleV = 2;
  g_font_body = atlas->AddFontFromFileTTF((dir / "Poppins-Medium.ttf").string().c_str(), 20.0f, &cfg);
  g_font_marker =
      atlas->AddFontFromFileTTF((dir / "PermanentMarker-Regular.ttf").string().c_str(), 28.0f, &cfg);
  g_font_hand = atlas->AddFontFromFileTTF((dir / "Kalam-Bold.ttf").string().c_str(), 20.0f, &cfg);
}

}  // namespace catherine::ui
