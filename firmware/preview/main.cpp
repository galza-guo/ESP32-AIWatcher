#include "../src/ui.hpp"
#include "../src/dirty_span.hpp"
#include "../src/frame_damage.hpp"
#include <cassert>
#include <cstring>
#include <vector>

using namespace desk_ui;

static void save(lgfx::LGFX_Sprite &canvas, const char *path) {
  std::vector<lgfx::bgr888_t> pixels(WIDTH * HEIGHT);
  canvas.readRectRGB(0, 0, WIDTH, HEIGHT, pixels.data());
  FILE *file = fopen(path, "wb");
  assert(file);
  fprintf(file, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
  for (auto pixel : pixels) {
    uint8_t bytes[] = {pixel.R8(), pixel.G8(), pixel.B8()};
    fwrite(bytes, 1, 3, file);
  }
  fclose(file);
}

int main() {
  lgfx::LGFX_Sprite canvas;
  canvas.setColorDepth(16);
  assert(canvas.createSprite(WIDTH, HEIGHT));
  SysState sys;
  sys.cpu = 23; sys.mem = 46; sys.temp = 52; sys.fan = 2780;
  sys.net_down = 8340000; sys.net_up = 726000;
  sys.net_rx = 2345000000ULL; sys.net_tx = 425600000ULL; sys.net_totals = true;
  sys.count = 800;
  for (int i = 0; i < sys.count; ++i) {
    sys.hist_cpu[i] = 22 + 9 * sinf(i * .06f) + 4 * sinf(i * .19f);
    sys.hist_mem[i] = 44 + 3 * sinf(i * .012f);
    sys.hist_temp[i] = 50 + 5 * sinf(i * .014f);
    sys.hist_net_down[i] = 4e6 + 3e6 * sinf(i * .09f);
    sys.hist_net_up[i] = 6e5 + 5e5 * sinf(i * .12f);
    sys.hist_fan[i] = 2700 + 250 * sinf(i * .019f);
  }
  Provider providers[3];
  const char *names[] = {"GPT", "Grok", "MiniMax"};
  const char *tiers[] = {"Pro", "SuperGrok", "Coding Plan"};
  for (int i = 0; i < 3; ++i) {
    strcpy(providers[i].name, names[i]); strcpy(providers[i].tier, tiers[i]);
    providers[i].tokens = 1234000 / (i + 1);
    providers[i].usd = 3.27f / (i + 1);
    providers[i].limit = .28f * (i + 1);
    providers[i].week_n = 7;
    for (int j = 0; j < 7; ++j) providers[i].week[j] = 200 + 80 * sinf(j + i);
  }
  render(canvas, PAGE_SYS, sys, providers, 3); save(canvas, "system.ppm");
  render(canvas, PAGE_AI, sys, providers, 3); save(canvas, "usage.ppm");
  SysState empty;
  render(canvas, PAGE_SYS, empty, providers, 0); save(canvas, "empty-system.ppm");
  render(canvas, PAGE_AI, empty, providers, 0); save(canvas, "empty-usage.ppm");
  Page page;
  assert(tab_at(60, 132, &page) && page == PAGE_SYS);
  assert(tab_at(60, 196, &page) && page == PAGE_AI);
  assert(!tab_at(700, 132, &page)); // Chart taps must not act as mirrored tabs.
  assert(!tab_at(60, 165, &page)); // Gap between tabs.
  assert(!tab_at(60, 300, &page));

  // Incremental SRAM tiles must produce exactly the same image as a full
  // render, including shorter values, missing readings and graph movement.
  lgfx::LGFX_Sprite tile, expected;
  tile.setColorDepth(16); expected.setColorDepth(16);
  std::vector<uint16_t> card_storage(CONTENT_W * RENDER_TILE_H);
  tile.setBuffer(card_storage.data(), CONTENT_W, SYSTEM_CARD_H, 16);
  assert(expected.createSprite(WIDTH, HEIGHT));
  render(canvas, PAGE_SYS, sys, providers, 3);
  for (int frame = 0; frame < 6; ++frame) {
    sys.cpu = frame ? frame * 9 : NAN;
    sys.fan = frame ? 999 / frame : NAN;
    sys.net_down = frame ? 9e8 / frame : NAN;
    sys.net_up = frame ? frame * 11 : NAN;
    sys.net_rx += 1234567890ULL;
    for (int i = 0; i < HISTORY; ++i) sys.hist_cpu[i] = 30 + 20 * sinf(i * .06f + frame);
    for (int i=0; i<HISTORY; ++i) {
      sys.hist_net_down[i] = i % 17 ? 5e6 * (1+sinf(i*.09f+frame)) : NAN;
      sys.hist_net_up[i] = i % 23 ? 2e6 * (1+sinf(i*.12f+frame)) : NAN;
    }
    for (int card = 0; card < 4; ++card) {
      tile.fillScreen(PAPER);
      system_card(tile, sys, card, 0, 0);
      for (int row = 0; row < SYSTEM_CARD_H; ++row) {
        auto before = static_cast<uint16_t *>(canvas.getBuffer())
                    + (CONTENT_Y + card * SYSTEM_CARD_STEP + row) * WIDTH + CONTENT_X;
        auto after = static_cast<uint16_t *>(tile.getBuffer()) + row * CONTENT_W;
        int begin, end;
        if (changed_span(before, after, CONTENT_W, begin, end))
          memcpy(before + begin, after + begin, (end - begin) * 2);
      }
    }
    tile.setBuffer(card_storage.data(), CONTENT_W, NETWORK_H, 16);
    tile.fillScreen(PAPER);
    network_card(tile, sys, 0, 0);
    for (int row=0; row<NETWORK_H; ++row) {
      auto before = static_cast<uint16_t *>(canvas.getBuffer()) + (NETWORK_Y+row)*WIDTH + CONTENT_X;
      auto after = static_cast<uint16_t *>(tile.getBuffer()) + row*CONTENT_W;
      int begin, end;
      if (changed_span(before, after, CONTENT_W, begin, end))
        memcpy(before+begin, after+begin, (end-begin)*2);
    }
    tile.setBuffer(card_storage.data(), CONTENT_W, SYSTEM_CARD_H, 16);
    render(expected, PAGE_SYS, sys, providers, 3);
    assert(!memcmp(canvas.getBuffer(), expected.getBuffer(), WIDTH * HEIGHT * 2));
  }
  uint16_t row[] = {1, 2, 3}, different[] = {1, 4, 3};
  int begin, end;
  assert(!changed_span(row, row, 3, begin, end));
  assert(changed_span(row, different, 3, begin, end) && begin == 1 && end == 2);
  // Exercise clipped strip rendering and actual sprite blits across both
  // page transitions, missing data, and changing provider counts.
  std::vector<uint16_t> storage(CONTENT_W * RENDER_TILE_H);
  tile.setBuffer(storage.data(), WIDTH, 48, 16);
  std::vector<uint16_t> front(WIDTH * HEIGHT);
  memcpy(front.data(), canvas.getBuffer(), WIDTH * HEIGHT * 2);
  FrameDamage<WIDTH, HEIGHT> damage;
  for (int frame = 0; frame < 8; ++frame) {
    Page next = frame % 2 ? PAGE_AI : PAGE_SYS;
    const auto &state = frame < 4 ? sys : empty;
    int count = frame % 4;
    render(expected, next, state, providers, count);
    for (int y = 0; y < HEIGHT; y += 48) {
      render(tile, next, state, providers, count, -y);
      for (int row = 0; row < 48; ++row) {
        auto before = static_cast<uint16_t *>(canvas.getBuffer()) + (y + row) * WIDTH;
        auto after = static_cast<uint16_t *>(tile.getBuffer()) + row * WIDTH;
        int begin, end;
        if (changed_span(before, after, WIDTH, begin, end)) {
          damage.mark(begin, y + row, end - begin, 1);
          canvas.setClipRect(begin, y + row, end - begin, 1);
          tile.pushSprite(&canvas, 0, y);
        }
      }
      canvas.clearClipRect();
    }
    assert(!memcmp(canvas.getBuffer(), expected.getBuffer(), WIDTH * HEIGHT * 2));
    // After presentation, the exact same damage tracker used on the device
    // must bring the retired frame up to date, including multiple spans/row.
    damage.synchronize(static_cast<uint16_t *>(canvas.getBuffer()), front.data());
    assert(!memcmp(front.data(), expected.getBuffer(), WIDTH * HEIGHT * 2));
    assert(!damage.changed());
  }
  tile.setBuffer(storage.data(), CONTENT_W, SYSTEM_CARD_H, 16);
  assert(tile.width() == CONTENT_W && tile.height() == SYSTEM_CARD_H);
  char formatted[32];
  compact_rate(999999, formatted, sizeof formatted); assert(!strcmp(formatted,"1.0M"));
  compact_rate(NAN, formatted, sizeof formatted); assert(!strcmp(formatted,"--"));
  // Even the widest compact pair must remain left of the history chart.
  canvas.setFont(&fonts::FreeSans12pt7b);
  for (double value : {0., 99.9, 999., 99900., 999000., 99900000., 999000000.}) {
    compact_rate(value, formatted, sizeof formatted);
    assert(2 * (14 + canvas.textWidth(formatted)) + 20 <= 192);
  }
  traffic(NAN, true, formatted, sizeof formatted); assert(!strcmp(formatted,"--"));
  traffic(0, true, formatted, sizeof formatted); assert(!strcmp(formatted,"0 B/s"));
  traffic(999999, true, formatted, sizeof formatted); assert(!strcmp(formatted,"1.0 MB/s"));
  traffic(5e9, false, formatted, sizeof formatted); assert(!strcmp(formatted,"5.0 GB"));
  assert(tab_at(35, 440, &page) && page == PAGE_SETTINGS);
  assert(!tab_at(35, 404, &page));
  Settings prefs;
  SettingsGesture gesture;
  assert(!gesture.update(prefs, 230, 140, true)); // Read-only status card.
  assert(gesture.update(prefs, SLIDER_LEFT, SLIDER_Y, true));
  assert(prefs.brightness == 10 && brightness_duty(prefs) > 0);
  assert(gesture.update(prefs, 799, SLIDER_Y, false) && prefs.brightness == 100);
  gesture.release();
  assert(!gesture.update(prefs, 680, 320, false)); // No captured drag.
  assert(!prefs.dark);
  assert(gesture.update(prefs, 680, 320, true) && prefs.dark);
  assert(gesture.update(prefs, 600, 410, true) && prefs.sleep == 1);
  assert(encode_settings(decode_settings(encode_settings(prefs))) == encode_settings(prefs));
  for (uint32_t invalid : {0u, 0xffffffffu, 0xA1010000u, 0xA1010065u, 0xA1010664u})
    assert(encode_settings(decode_settings(invalid)) == encode_settings(Settings{}));
  IdleDisplay idle;
  idle.reset(0xfffffff0u);
  assert(!idle.tick(0xfffffff0u + 299999u, prefs));
  assert(idle.tick(0xfffffff0u + 300000u, prefs));
  assert(idle.contact(300010) && !idle.asleep());
  assert(idle.contact(300050)); // Entire wake gesture is consumed.
  idle.release();
  assert(!idle.contact(300100));
  assert(idle.tick(600100, prefs));
  assert(idle.button(600200));
  assert(!idle.button(600300));
  prefs.sleep = 0;
  assert(!idle.tick(3600000, prefs));

  // Settings, dark-theme transitions and status updates must also be identical
  // whether drawn at once or in clipped strips.
  tile.setBuffer(storage.data(), WIDTH, 48, 16);
  for (bool dark : {false, true}) {
    prefs.dark = dark;
    for (Page next : {PAGE_SYS, PAGE_AI, PAGE_SETTINGS}) {
      for (ConnectionStatus status : {ConnectionStatus{}, ConnectionStatus{true, 0}, ConnectionStatus{true, 86400}}) {
        render(expected, next, sys, providers, 3, 0, prefs, status);
        for (int y=0; y<HEIGHT; y+=48) {
          render(tile, next, sys, providers, 3, -y, prefs, status);
          tile.pushSprite(&canvas, 0, y);
        }
        assert(!memcmp(canvas.getBuffer(), expected.getBuffer(), WIDTH*HEIGHT*2));
      }
      if (next == PAGE_SETTINGS) {
        render(canvas, next, sys, providers, 3, 0, prefs, {true,0});
        save(canvas, dark ? "settings-dark.ppm" : "settings-light.ppm");
      } else if (dark) save(canvas, next == PAGE_SYS ? "system-dark.ppm" : "usage-dark.ppm");
    }
  }
  puts("Rendered previews; navigation and incremental pixel-equivalence checks passed.");
}
