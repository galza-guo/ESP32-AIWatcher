#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_panel_ops.h>
#include "frame_damage.hpp"

// LovyanGFX remains the renderer. ESP-IDF owns the RGB peripheral and feeds
// its DMA from two internal SRAM bounce buffers, not directly from PSRAM.
class DeskPanel : public lgfx::LGFX_Sprite {
  esp_lcd_panel_handle_t handle_ = nullptr;
  void *frames_[2] = {};
  int back_ = 1;
  volatile uint32_t completed_ = 0;
  FrameDamage<800, 480> damage_;
  static bool IRAM_ATTR frame_complete(esp_lcd_panel_handle_t,
      const esp_lcd_rgb_panel_event_data_t *, void *ctx) {
    auto self = static_cast<DeskPanel *>(ctx);
    self->completed_ = self->completed_ + 1;
    return false;
  }
 public:
  bool init() {
    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_DEFAULT;
    cfg.data_width = 16;
    cfg.bits_per_pixel = 16;
    cfg.num_fbs = 2;
    cfg.bounce_buffer_size_px = 800 * 10;
    cfg.dma_burst_size = 64;
    cfg.flags.fb_in_psram = true;
    // Leave cache invalidation disabled: renderer and bounce ISR share data.
    cfg.hsync_gpio_num = 4;
    cfg.vsync_gpio_num = 2;
    cfg.de_gpio_num = 5;
    cfg.pclk_gpio_num = 6;
    cfg.disp_gpio_num = -1;
    const int pins[] = {14, 21, 47, 48, 45, 46, 9, 10,
                        11, 12, 13, 7, 15, 16, 8, 3};
    // Match Bus_RGB's byte-swapped signal mapping for LGFX RGB565 storage.
    for (int i = 0; i < 16; ++i) cfg.data_gpio_nums[i ^ 8] = pins[i];
    auto &timing = cfg.timings;
    timing.pclk_hz = 15000000;
    timing.h_res = 800;
    timing.v_res = 480;
    timing.hsync_pulse_width = 16;
    timing.hsync_back_porch = 32;
    timing.hsync_front_porch = 10;
    timing.vsync_pulse_width = 3;
    timing.vsync_back_porch = 12;
    timing.vsync_front_porch = 15;
    // Preserve the working driver's actual register levels. Bus_RGB uses
    // pclk_idle_high for lcd_ck_out_edge and ignores pclk_active_neg.
    timing.flags.pclk_active_neg = false;
    timing.flags.hsync_idle_low = true;
    timing.flags.vsync_idle_low = true;
    esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &handle_);
    if (err != ESP_OK) {
      Serial.printf("RGB bounce allocation failed: %s\n", esp_err_to_name(err));
      return false;
    }
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(handle_, 2, &frames_[0], &frames_[1]));
    memset(frames_[0], 0, 800 * 480 * 2);
    memset(frames_[1], 0, 800 * 480 * 2);
    setBuffer(frames_[back_], 800, 480, 16);
    esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
    callbacks.on_frame_buf_complete = frame_complete;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(handle_, &callbacks, this));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(handle_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(handle_));
    Serial.printf("rgb_driver=idf_bounce framebuffers=2 pclk=15000000 bounce_bytes=32000 free_internal=%u\n",
                  ESP.getFreeHeap());
    return true;
  }

  void markDirty(int x, int y, int width, int height) {
    damage_.mark(x, y, width, height);
  }

  bool presentFrame() {
    if (!damage_.changed()) return true;
    // With bounce buffers, IDF selects cur_fb_index only at a complete-frame
    // boundary. Never modify a published buffer until it has been retired.
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(handle_, 0, 0, 800, 480, frames_[back_]));
    const uint32_t before = completed_;
    const uint32_t started = millis();
    // Two completions after publication conservatively cover a callback
    // racing with draw_bitmap, including the bounce buffers' prefetch.
    while (uint32_t(completed_ - before) < 2) {
      if (millis() - started > 250) {
        Serial.println("error: RGB frame boundary timeout; rendering stopped");
        return false;
      }
      delay(1);
    }
    auto source = static_cast<uint16_t *>(frames_[back_]);
    back_ ^= 1;
    auto destination = static_cast<uint16_t *>(frames_[back_]);
    // Keep both buffers identical between updates, copying only the damage
    // after the old front buffer is no longer used by scanout.
    damage_.synchronize(source, destination);
    setBuffer(frames_[back_], 800, 480, 16);
    return true;
  }

  void setBrightness(uint8_t brightness) {
    pinMode(1, OUTPUT);
    digitalWrite(1, brightness ? HIGH : LOW);
  }
};
