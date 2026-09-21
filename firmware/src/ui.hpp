#pragma once

#include <LovyanGFX.hpp>
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace desk_ui {

constexpr int WIDTH = 800;
constexpr int HEIGHT = 480;
constexpr int HISTORY = 800;
constexpr int TAB_W = 148;
constexpr int CONTENT_X = 176;
constexpr int CONTENT_Y = 108;
constexpr int CONTENT_W = 596;
constexpr int SYSTEM_CARD_H = 76;
constexpr int SYSTEM_CARD_STEP = 86;

constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
constexpr auto PAPER = rgb(245, 245, 247);
constexpr auto RAIL = rgb(237, 238, 241);
constexpr auto WHITE = rgb(255, 255, 255);
constexpr auto INK = rgb(29, 29, 31);
constexpr auto MUTE = rgb(116, 119, 128);
constexpr auto RULE = rgb(224, 226, 232);
constexpr auto BLUE = rgb(0, 113, 227);
constexpr auto TRACK = rgb(233, 239, 247);

enum Page { PAGE_SYS, PAGE_AI };

struct SysState {
  float cpu = NAN, mem = NAN, temp = NAN, fan = NAN;
  float hist_cpu[HISTORY] = {}, hist_mem[HISTORY] = {};
  float hist_temp[HISTORY] = {}, hist_fan[HISTORY] = {};
  int count = 0;
};

struct Provider {
  char name[16] = {}, tier[24] = {};
  int64_t tokens = 0;
  float usd = NAN, limit = 0;
  float week[7] = {};
  int week_n = 0;
};

inline bool tab_at(int x, int y, Page *out) {
  if (x < 12 || x >= TAB_W - 12) return false;
  if (y >= 108 && y < 158) { *out = PAGE_SYS; return true; }
  if (y >= 172 && y < 222) { *out = PAGE_AI; return true; }
  return false;
}

inline void text(lgfx::LGFX_Sprite &g, const char *s, int x, int y,
                 const lgfx::IFont *font, uint16_t color,
                 lgfx::textdatum_t datum = lgfx::textdatum_t::top_left) {
  g.setFont(font);
  g.setTextSize(1);
  g.setTextDatum(datum);
  g.setTextColor(color);
  g.drawString(s, x, y);
}

inline void tokens(int64_t n, char *out, size_t size) {
  if (n >= 1000000000LL) snprintf(out, size, "%.1fB", n / 1e9);
  else if (n >= 1000000) snprintf(out, size, "%.1fM", n / 1e6);
  else if (n >= 1000) snprintf(out, size, "%.1fK", n / 1e3);
  else snprintf(out, size, "%lld", (long long)n);
}

inline void icon(lgfx::LGFX_Sprite &g, int x, int y, Page page, uint16_t color) {
  if (page == PAGE_SYS) {
    g.drawRoundRect(x, y, 20, 16, 3, color);
    g.drawFastHLine(x + 6, y + 19, 8, color);
    g.drawFastVLine(x + 10, y + 16, 3, color);
    g.drawLine(x + 4, y + 10, x + 8, y + 6, color);
    g.drawLine(x + 8, y + 6, x + 12, y + 10, color);
    g.drawLine(x + 12, y + 10, x + 16, y + 5, color);
  } else {
    g.fillRoundRect(x + 1, y + 11, 4, 9, 2, color);
    g.fillRoundRect(x + 8, y + 6, 4, 14, 2, color);
    g.fillRoundRect(x + 15, y + 1, 4, 19, 2, color);
  }
}

inline void chrome(lgfx::LGFX_Sprite &g, Page page, int dy = 0) {
  g.fillScreen(PAPER);
  g.fillRect(0, dy, TAB_W, HEIGHT, RAIL);
  g.drawFastVLine(TAB_W - 1, dy, HEIGHT, RULE);
  text(g, "Desk", 24, 31 + dy, &fonts::FreeSansBold12pt7b, INK);
  text(g, "MONITOR", 25, 64 + dy, &fonts::DejaVu9, MUTE);
  for (int i = 0; i < 2; ++i) {
    Page item = static_cast<Page>(i);
    int y = 108 + i * 64 + dy;
    bool selected = page == item;
    if (selected) g.fillRoundRect(12, y, TAB_W - 24, 50, 11, WHITE);
    uint16_t color = selected ? BLUE : MUTE;
    icon(g, 24, y + 15, item, color);
    text(g, i ? "Usage" : "System", 55, y + 17, &fonts::DejaVu12, color);
  }
  text(g, page == PAGE_SYS ? "System" : "Usage", CONTENT_X, 28 + dy,
       &fonts::FreeSans18pt7b, INK);
  text(g, page == PAGE_SYS ? "Live performance" : "Daily totals & recent activity",
       CONTENT_X + 1, 73 + dy, &fonts::DejaVu12, MUTE);
}

inline void sparkline(lgfx::LGFX_Sprite &g, int x, int y, int w, int h,
                      const float *samples, int count, float scale, bool fit = false) {
  g.drawFastHLine(x, y + h - 1, w, TRACK);
  if (count <= 0) return;
  int start = fit || count < w ? 0 : count - w;
  int shown = count - start;
  if (fit) {
    for (int i = start; i < count; ++i)
      if (std::isfinite(samples[i]) && samples[i] > scale) scale = samples[i];
  }
  if (scale < 1) scale = 1;
  int prev_x = -1, prev_y = 0;
  for (int i = 0; i < shown; ++i) {
    float value = samples[start + i];
    if (!std::isfinite(value)) { prev_x = -1; continue; }
    float fraction = fmaxf(0, fminf(1, value / scale));
    int px = fit ? x + (shown > 1 ? i * (w - 1) / (shown - 1) : w - 1)
                 : x + w - shown + i;
    int py = y + h - 2 - (int)(fraction * (h - 4));
    if (prev_x >= 0) {
      g.drawLine(prev_x, prev_y, px, py, BLUE);
      g.drawLine(prev_x, prev_y + 1, px, py + 1, BLUE);
    }
    prev_x = px; prev_y = py;
  }
  if (prev_x >= 0) g.fillCircle(prev_x, prev_y, 2, BLUE);
}

inline void system_card(lgfx::LGFX_Sprite &g, const SysState &s, int i, int x, int y) {
  if (y >= g.height() || y + SYSTEM_CARD_H <= 0) return;
  const char *names[] = {"CPU", "Memory", "Temperature", "Fan"};
  const char *units[] = {"%", "%", " C", " rpm"};
  const float values[] = {s.cpu, s.mem, s.temp, s.fan};
  const float *history[] = {s.hist_cpu, s.hist_mem, s.hist_temp, s.hist_fan};
  const float scales[] = {100, 100, 100, 5000};
    g.fillRoundRect(x, y, CONTENT_W, SYSTEM_CARD_H, 13, WHITE);
    text(g, names[i], x + 20, y + 11, &fonts::DejaVu12, MUTE);
    char value[24];
    if (std::isfinite(values[i])) snprintf(value, sizeof value, "%.0f%s", values[i], units[i]);
    else snprintf(value, sizeof value, "--");
    text(g, value, x + 20, y + 34, &fonts::FreeSans18pt7b, INK);
    sparkline(g, x + 212, y + 23, CONTENT_W - 236, 34,
              history[i], s.count, scales[i]);
}

inline void system_cards(lgfx::LGFX_Sprite &g, const SysState &s, int dy = 0) {
  for (int i = 0; i < 4; ++i) {
    system_card(g, s, i, CONTENT_X, CONTENT_Y + i * SYSTEM_CARD_STEP + dy);
  }
}

inline void usage_cards(lgfx::LGFX_Sprite &g, const Provider *providers, int count, int dy = 0) {
  g.fillRect(CONTENT_X, CONTENT_Y + dy, CONTENT_W, HEIGHT - CONTENT_Y, PAPER);
  if (!count) {
    text(g, "Waiting for usage data", CONTENT_X + CONTENT_W / 2, 259 + dy,
         &fonts::DejaVu18, MUTE, lgfx::textdatum_t::middle_center);
    return;
  }
  for (int i = 0; i < count && i < 3; ++i) {
    const Provider &p = providers[i];
    int y = CONTENT_Y + i * 116 + dy;
    if (y >= g.height() || y + 106 <= 0) continue;
    g.fillRoundRect(CONTENT_X, y, CONTENT_W, 106, 13, WHITE);
    text(g, p.name, CONTENT_X + 20, y + 13, &fonts::FreeSans12pt7b, INK);
    text(g, p.tier, CONTENT_X + CONTENT_W - 20, y + 17, &fonts::DejaVu12,
         MUTE, lgfx::textdatum_t::top_right);
    char value[24], cost[32];
    tokens(p.tokens, value, sizeof value);
    text(g, value, CONTENT_X + 20, y + 42, &fonts::FreeSans18pt7b, INK);
    if (std::isfinite(p.usd)) snprintf(cost, sizeof cost, "tokens today  /  $%.2f", p.usd);
    else snprintf(cost, sizeof cost, "tokens today");
    text(g, cost, CONTENT_X + 20, y + 82, &fonts::DejaVu9, MUTE);
    sparkline(g, CONTENT_X + 300, y + 51, CONTENT_W - 324, 30,
              p.week, p.week_n, 1, true);
    int bar_w = CONTENT_W - 324;
    int filled = (int)(fmaxf(0, fminf(1, p.limit)) * bar_w);
    g.fillRoundRect(CONTENT_X + 300, y + 94, bar_w, 3, 1, TRACK);
    if (filled > 0) g.fillRoundRect(CONTENT_X + 300, y + 94, filled, 3, 1, BLUE);
  }
}

inline void render(lgfx::LGFX_Sprite &g, Page page, const SysState &sys,
                   const Provider *providers, int count, int dy = 0) {
  chrome(g, page, dy);
  if (page == PAGE_SYS) system_cards(g, sys, dy);
  else usage_cards(g, providers, count, dy);
}

} // namespace desk_ui
