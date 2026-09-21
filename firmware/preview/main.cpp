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
  sys.count = 800;
  for (int i = 0; i < sys.count; ++i) {
    sys.hist_cpu[i] = 22 + 9 * sinf(i * .06f) + 4 * sinf(i * .19f);
    sys.hist_mem[i] = 44 + 3 * sinf(i * .012f);
    sys.hist_temp[i] = 50 + 5 * sinf(i * .014f);
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
  assert(tile.createSprite(CONTENT_W, SYSTEM_CARD_H));
  assert(expected.createSprite(WIDTH, HEIGHT));
  render(canvas, PAGE_SYS, sys, providers, 3);
  for (int frame = 0; frame < 6; ++frame) {
    sys.cpu = frame ? frame * 9 : NAN;
    sys.fan = frame ? 999 / frame : NAN;
    for (int i = 0; i < HISTORY; ++i) sys.hist_cpu[i] = 30 + 20 * sinf(i * .06f + frame);
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
    render(expected, PAGE_SYS, sys, providers, 3);
    assert(!memcmp(canvas.getBuffer(), expected.getBuffer(), WIDTH * HEIGHT * 2));
  }
  uint16_t row[] = {1, 2, 3}, different[] = {1, 4, 3};
  int begin, end;
  assert(!changed_span(row, row, 3, begin, end));
  assert(changed_span(row, different, 3, begin, end) && begin == 1 && end == 2);
  // Exercise clipped strip rendering and actual sprite blits across both
  // page transitions, missing data, and changing provider counts.
  std::vector<uint16_t> storage(CONTENT_W * SYSTEM_CARD_H);
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
  puts("Rendered previews; navigation and incremental pixel-equivalence checks passed.");
}
