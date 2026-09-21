#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <math.h>
#include <string.h>
#include <esp_heap_caps.h>

#include "panel.hpp"
#include "ui.hpp"
#include "dirty_span.hpp"

using namespace desk_ui;

static DeskPanel lcd;

static lgfx::LGFX_Sprite canvas(&lcd);
static lgfx::LGFX_Sprite system_tile;
static bool tile_ready = false;
static bool canvas_valid = false;
static constexpr int PAGE_STRIP_H = 48;
static_assert(WIDTH * PAGE_STRIP_H <= CONTENT_W * SYSTEM_CARD_H,
              "Page strip must fit the internal card tile");
static constexpr int BTN_PIN = 0;

static SysState sys;
static Provider providers[3];
static int provider_n = 0;
static Page page = PAGE_SYS;
static char line[768];
static size_t filled = 0;
static uint32_t last_touch = 0;
static bool btn_last = true;
static bool touch_down = false;
static bool chrome_dirty = true;
static bool sys_dirty = false;
static bool ai_dirty = false;

static int touch_sda = 42;
static int touch_scl = 41;
static uint8_t touch_addr = 0x14;

static void push(float *hist, float value) {
  if (isnan(value)) return;
  if (sys.count < HISTORY) {
    hist[sys.count] = value;
  } else {
    memmove(hist, hist + 1, (HISTORY - 1) * sizeof(float));
    hist[HISTORY - 1] = value;
  }
}

static void handle_sys(JsonDocument &doc) {
  sys.cpu = doc["cpu"].isNull() ? NAN : doc["cpu"].as<float>();
  sys.mem = doc["mem"].isNull() ? NAN : doc["mem"].as<float>();
  sys.temp = doc["temp"].isNull() ? NAN : doc["temp"].as<float>();
  sys.fan = doc["fan"].isNull() ? NAN : doc["fan"].as<float>();
  push(sys.hist_cpu, sys.cpu);
  push(sys.hist_mem, sys.mem);
  push(sys.hist_temp, sys.temp);
  push(sys.hist_fan, sys.fan);
  if (sys.count < HISTORY) sys.count++;
  sys_dirty = true;
}

static void handle_ai(JsonDocument &doc) {
  JsonArray arr = doc["p"].as<JsonArray>();
  provider_n = 0;
  if (arr.isNull()) return;
  for (JsonObject row : arr) {
    if (provider_n >= 3) break;
    Provider &p = providers[provider_n++];
    strlcpy(p.name, row["n"] | "?", sizeof p.name);
    strlcpy(p.tier, row["g"] | "", sizeof p.tier);
    p.tokens = row["v"] | 0;
    p.usd = row["u"].isNull() ? NAN : row["u"].as<float>();
    p.limit = row["l"] | 0.0f;
    p.week_n = 0;
    JsonArray week = row["w"].as<JsonArray>();
    if (!week.isNull()) {
      for (JsonVariant v : week) {
        if (p.week_n >= 7) break;
        p.week[p.week_n++] = v.as<float>();
      }
    }
  }
  ai_dirty = true;
}

static void take_line(char *raw) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, raw);
  if (err) return;
  const char *kind = doc["t"] | "";
  if (kind[0] == 's') handle_sys(doc);
  else if (kind[0] == 'a') handle_ai(doc);
}

// Compose off-screen, then copy only complete content to the scanout buffer.
// The clipping also keeps the sidebar/header untouched during live updates.
static void present(bool full) {
  if (!full) lcd.setClipRect(CONTENT_X, CONTENT_Y, CONTENT_W, HEIGHT - CONTENT_Y);
  canvas.pushSprite(0, 0);
  lcd.clearClipRect();
}

static uint32_t submit_tile(int x, int y) {
  auto previous = static_cast<uint16_t *>(canvas.getBuffer());
  auto pixels = static_cast<uint16_t *>(system_tile.getBuffer());
  uint32_t copied = 0;
  const int width = system_tile.width();
  for (int row = 0; row < system_tile.height(); ++row) {
    auto old_row = previous + (y + row) * WIDTH + x;
    auto new_row = pixels + row * width;
    int begin, end;
    if (!canvas_valid) { begin = 0; end = width; }
    if (!canvas_valid || changed_span(old_row, new_row, width, begin, end)) {
      const size_t bytes = (end - begin) * sizeof(uint16_t);
      memcpy(old_row + begin, new_row + begin, bytes);
      lcd.setClipRect(x + begin, y + row, end - begin, 1);
      system_tile.pushSprite(&lcd, x, y);
      copied += bytes;
    }
    // Bound each burst of external-memory reads/writes during RGB scanout.
    if ((row & 7) == 7) delayMicroseconds(30);
  }
  lcd.clearClipRect();
  return copied;
}

static uint32_t render_page_tiles() {
  void *pixels = system_tile.getBuffer();
  system_tile.setBuffer(pixels, WIDTH, PAGE_STRIP_H, 16);
  uint32_t copied = 0;
  for (int y = 0; y < HEIGHT; y += PAGE_STRIP_H) {
    desk_ui::render(system_tile, page, sys, providers, provider_n, -y);
    copied += submit_tile(0, y);
  }
  system_tile.setBuffer(pixels, CONTENT_W, SYSTEM_CARD_H, 16);
  canvas_valid = true;
  return copied;
}

static void update_system() {
  if (!tile_ready) {
    system_cards(canvas, sys);
    present(false);
    return;
  }
  uint32_t started = micros();
  uint32_t copied = 0;
  for (int card = 0; card < 4; ++card) {
    const int y = CONTENT_Y + card * SYSTEM_CARD_STEP;
    // Render in internal SRAM, away from the PSRAM used by RGB scanout.
    system_tile.fillScreen(PAPER);
    system_card(system_tile, sys, card, 0, 0);
    copied += submit_tile(CONTENT_X, y);
  }
  lcd.clearClipRect();
  // A bounded diagnostic sample verifies the actual traffic after startup.
  static unsigned samples = 0;
  if (samples < 3) {
    Serial.printf("system update_us=%lu copied_bytes=%lu\n",
                  (unsigned long)(micros() - started), (unsigned long)copied);
    ++samples;
  }
}

static void show_page() {
  uint32_t started = micros();
  uint32_t copied = 0;
  if (tile_ready) copied = render_page_tiles();
  else {
    desk_ui::render(canvas, page, sys, providers, provider_n);
    present(true);
    copied = WIDTH * HEIGHT * 2;
  }
  Serial.printf("page=%s update_us=%lu copied_bytes=%lu\n",
                page == PAGE_SYS ? "System" : "Usage",
                (unsigned long)(micros() - started), (unsigned long)copied);
  chrome_dirty = false;
  sys_dirty = false;
  ai_dirty = false;
}

static bool gt_write(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(touch_addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool gt_read(uint16_t reg, uint8_t *dst, size_t n) {
  Wire.beginTransmission(touch_addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(touch_addr, n, true) != n) return false;
  for (size_t i = 0; i < n; ++i) dst[i] = Wire.read();
  return true;
}

static int gt_finger(int *x, int *y) {
  uint8_t st = 0;
  if (!gt_read(0x814E, &st, 1)) return -1;
  if ((st & 0x80) == 0) return -1;
  uint8_t n = st & 0x0F;
  if (n == 0) {
    gt_write(0x814E, 0);
    return false;
  }
  uint8_t buf[8] = {};
  if (n > 5) { gt_write(0x814E, 0); return -1; }
  if (!gt_read(0x814F, buf, 8)) return -1;
  gt_write(0x814E, 0);
  *x = buf[1] | (buf[2] << 8);
  *y = buf[3] | (buf[4] << 8);
  return (*x < WIDTH && *y < HEIGHT) ? 1 : -1;
}

static bool gt_start() {
  // Factory firmware @ 0x4200a4fc drives GPIO40 as RESET and GPIO39
  // as INT/address select. Keep INT low during reset to select 0x5D.
  constexpr int RESET_PIN = 40;
  constexpr int INT_PIN = 39;
  pinMode(RESET_PIN, OUTPUT);
  pinMode(INT_PIN, OUTPUT);
  digitalWrite(RESET_PIN, HIGH);
  digitalWrite(INT_PIN, HIGH);
  delay(60);
  digitalWrite(INT_PIN, LOW);
  digitalWrite(RESET_PIN, LOW);
  delay(20);
  digitalWrite(RESET_PIN, HIGH);
  delay(20);
  // Match the board's factory initialization, which keeps GPIO39 low.
  digitalWrite(INT_PIN, LOW);
  delay(300);

  Wire.setTimeOut(20);
  if (!Wire.begin(touch_sda, touch_scl, 100000)) return false;
  for (uint8_t addr : { (uint8_t)0x5D, (uint8_t)0x14 }) {
    touch_addr = addr;
    uint8_t info[12] = {};
    if (!gt_read(0x8140, info, sizeof info) || memcmp(info, "911", 3)) continue;
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(255);
  digitalWrite(1, HIGH);
  bool ok = gt_start();
  // The touch controller stays powered across ESP32 resets. Restore normal
  // coordinate reporting if a previous firmware left it in raw-data mode.
  if (ok) {
    ok = gt_write(0x8040, 0);
    delay(20);
    gt_write(0x814E, 0);
  }
  uint8_t id[3] = {};
  gt_read(0x8140, id, 3);
  Serial.printf("{\"hello\":\"desk-monitor\",\"sda\":%d,\"scl\":%d,\"addr\":%u,\"id\":\"%c%c%c\",\"ok\":%d}\n",
                touch_sda, touch_scl, touch_addr,
                id[0] ? id[0] : '?', id[1] ? id[1] : '?', id[2] ? id[2] : '?',
                ok ? 1 : 0);
  uint8_t cfg[7] = {};
  bool cfg_ok = gt_read(0x8047, cfg, sizeof cfg);
  Serial.printf("touch config ok=%u version=%u width=%u height=%u points=%u switch=0x%02x\n",
                cfg_ok, cfg[0], cfg[1] | (cfg[2] << 8), cfg[3] | (cfg[4] << 8), cfg[5], cfg[6]);
  canvas.setColorDepth(16);
  canvas.setPsram(true);
  if (!canvas.createSprite(WIDTH, HEIGHT)) {
    Serial.println("error: offscreen canvas allocation failed");
    lcd.fillScreen(TFT_WHITE);
    lcd.setTextColor(TFT_BLACK);
    lcd.drawString("Display buffer unavailable", 24, 24);
    while (true) delay(1000);
  }
  Serial.printf("canvas_bytes=%u free_psram=%u\n", WIDTH * HEIGHT * 2, ESP.getFreePsram());
  auto tile_pixels = heap_caps_malloc(CONTENT_W * SYSTEM_CARD_H * 2,
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (tile_pixels) {
    system_tile.setBuffer(tile_pixels, CONTENT_W, SYSTEM_CARD_H, 16);
    tile_ready = true;
  }
  Serial.printf("system_tile_internal=%u bytes=%u\n", tile_ready,
                CONTENT_W * SYSTEM_CARD_H * 2);
  show_page();
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      line[filled] = 0;
      if (filled) take_line(line);
      filled = 0;
    } else if (filled + 1 < sizeof(line)) {
      line[filled++] = c;
    } else {
      filled = 0;
    }
  }

  bool btn = digitalRead(BTN_PIN);
  if (btn_last && !btn) {
    page = (page == PAGE_SYS) ? PAGE_AI : PAGE_SYS;
    chrome_dirty = true;
  }
  btn_last = btn;

  if (millis() - last_touch > 40) {
    int raw_x = 0, raw_y = 0;
    int sample = gt_finger(&raw_x, &raw_y);
    last_touch = millis();
    if (sample > 0) {
      int px = raw_x, py = raw_y;
      if (!touch_down) {
        Page next = page;
        Serial.printf("touch x=%d y=%d\n", px, py);
        if (tab_at(px, py, &next) && next != page) {
          page = next;
          chrome_dirty = true;
        }
      }
      touch_down = true;
    } else if (sample == 0) {
      touch_down = false;
    }
  }

  if (chrome_dirty) {
    show_page();
    return;
  }
  if (page == PAGE_SYS && sys_dirty) {
    update_system();
    sys_dirty = false;
  } else if (page == PAGE_AI && ai_dirty) {
    if (tile_ready) render_page_tiles();
    else {
      usage_cards(canvas, providers, provider_n);
      present(false);
    }
    ai_dirty = false;
  }
}
