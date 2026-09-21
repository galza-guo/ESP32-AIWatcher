#pragma once
#define LGFX_USE_V1
#ifdef DESK_RGB_BOUNCE
#include "panel_bounce.hpp"
#else
#include <driver/i2c.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

// EYA EA4313-S3 4.3" 800×480 RGB + GT911.
// Pinout lifted from the factory lvgl_demo (lv_port_disp_init @ 0x42026f48).
class DeskPanel : public lgfx::LGFX_Device {
  lgfx::Bus_RGB _bus;
  lgfx::Panel_RGB _panel;
  lgfx::Light_PWM _light;
  lgfx::Touch_GT911 _touch;
 public:
  DeskPanel() {
    {
      auto cfg = _panel.config();
      cfg.memory_width = 800;
      cfg.memory_height = 480;
      cfg.panel_width = 800;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel.config(cfg);
    }
    {
      auto cfg = _panel.config_detail();
      cfg.use_psram = 1;
      _panel.config_detail(cfg);
    }
    {
      auto cfg = _bus.config();
      cfg.panel = &_panel;
      cfg.pin_d0 = 14;
      cfg.pin_d1 = 21;
      cfg.pin_d2 = 47;
      cfg.pin_d3 = 48;
      cfg.pin_d4 = 45;
      cfg.pin_d5 = 46;
      cfg.pin_d6 = 9;
      cfg.pin_d7 = 10;
      cfg.pin_d8 = 11;
      cfg.pin_d9 = 12;
      cfg.pin_d10 = 13;
      cfg.pin_d11 = 7;
      cfg.pin_d12 = 15;
      cfg.pin_d13 = 16;
      cfg.pin_d14 = 8;
      cfg.pin_d15 = 3;
      cfg.pin_henable = 5;
      cfg.pin_vsync = 2;
      cfg.pin_hsync = 4;
      cfg.pin_pclk = 6;
      cfg.freq_write = 15000000;
      cfg.hsync_polarity = 0;
      cfg.hsync_front_porch = 10;
      cfg.hsync_pulse_width = 16;
      cfg.hsync_back_porch = 32;
      cfg.vsync_polarity = 0;
      cfg.vsync_front_porch = 15;
      cfg.vsync_pulse_width = 3;
      cfg.vsync_back_porch = 12;
      cfg.pclk_active_neg = true;
      cfg.de_idle_high = false;
      cfg.pclk_idle_high = false;
      _bus.config(cfg);
    }
    _panel.setBus(&_bus);
    {
      auto cfg = _light.config();
      cfg.pin_bl = 1;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }

  void bindTouch(int sda, int scl, uint8_t addr) {
    auto cfg = _touch.config();
    cfg.x_min = 0;
    cfg.x_max = 799;
    cfg.y_min = 0;
    cfg.y_max = 479;
    cfg.pin_int = -1;
    cfg.pin_rst = -1;
    cfg.bus_shared = false;
    cfg.offset_rotation = 0;
    cfg.i2c_port = I2C_NUM_0;
    cfg.i2c_addr = addr;
    cfg.pin_sda = sda;
    cfg.pin_scl = scl;
    cfg.freq = 100000;
    _touch.config(cfg);
    _panel.setTouch(&_touch);
  }
};
#endif
