#pragma once
#include <algorithm>
#include <cstdint>

namespace desk_ui {
constexpr const char *FIRMWARE_VERSION = "1.1.0-dev";

struct Settings {
  uint8_t brightness = 100; // Percent; keep the manual minimum visible.
  bool dark = false;
  uint8_t sleep = 0;       // Never, 5 minutes, 15 minutes.
};

inline uint32_t encode_settings(const Settings &s) {
  return 0xA1010000u | s.brightness | (uint32_t(s.dark) << 8) | (uint32_t(s.sleep) << 9);
}
inline Settings decode_settings(uint32_t value) {
  Settings s;
  if ((value & 0xFFFF0000u) != 0xA1010000u) return s;
  unsigned brightness = value & 255, sleep = (value >> 9) & 3;
  if (brightness < 10 || brightness > 100 || sleep > 2) return s;
  s.brightness = brightness; s.dark = (value >> 8) & 1; s.sleep = sleep;
  return s;
}
inline uint8_t brightness_duty(const Settings &s) {
  return (unsigned(s.brightness) * 255 + 50) / 100;
}
inline uint32_t sleep_timeout(const Settings &s) {
  return s.sleep == 1 ? 300000u : s.sleep == 2 ? 900000u : 0;
}

struct Rect {
  int x, y, w, h;
  bool contains(int px, int py) const { return px >= x && px < x+w && py >= y && py < y+h; }
};
constexpr Rect SETTINGS_BUTTON{12, 414, 124, 50};
constexpr Rect BRIGHTNESS_HIT{376, 233, 376, 49};
constexpr int SLIDER_LEFT = 396, SLIDER_RIGHT = 728, SLIDER_Y = 253;
constexpr Rect LIGHT_BUTTON{508, 308, 120, 44};
constexpr Rect DARK_BUTTON{628, 308, 124, 44};
constexpr Rect SLEEP_BUTTONS[] = {{454, 389, 98, 44}, {554, 389, 98, 44}, {654, 389, 98, 44}};

inline uint8_t brightness_at(int x) {
  x = std::max(SLIDER_LEFT, std::min(SLIDER_RIGHT, x));
  return 10 + ((x - SLIDER_LEFT) * 90 + (SLIDER_RIGHT-SLIDER_LEFT)/2) / (SLIDER_RIGHT-SLIDER_LEFT);
}
// A gesture captures its initial control. Dragging over a different row must
// not activate it. Brightness can continue to its endpoints outside the rail.
class SettingsGesture {
  bool slider_ = false;
 public:
  void release() { slider_ = false; }
  bool update(Settings &s, int x, int y, bool first) {
    uint32_t old = encode_settings(s);
    if (first) {
      slider_ = BRIGHTNESS_HIT.contains(x, y);
      if (LIGHT_BUTTON.contains(x,y)) s.dark = false;
      if (DARK_BUTTON.contains(x,y)) s.dark = true;
      for (int i=0; i<3; ++i) if (SLEEP_BUTTONS[i].contains(x,y)) s.sleep = i;
    }
    if (slider_) s.brightness = brightness_at(x);
    return encode_settings(s) != old;
  }
};

class IdleDisplay {
  uint32_t activity_ = 0;
  bool asleep_ = false;
  bool swallowing_ = false;
 public:
  void reset(uint32_t now) { activity_ = now; asleep_ = false; swallowing_ = false; }
  bool asleep() const { return asleep_; }
  bool tick(uint32_t now, const Settings &s) {
    uint32_t timeout = sleep_timeout(s);
    if (!asleep_ && timeout && uint32_t(now-activity_) >= timeout) {
      asleep_ = true; return true;
    }
    return false;
  }
  // Return true for every sample of the wake gesture, until its release.
  bool contact(uint32_t now) {
    activity_ = now;
    if (asleep_) { asleep_ = false; swallowing_ = true; }
    return swallowing_;
  }
  void release() { swallowing_ = false; }
  bool button(uint32_t now) {
    bool was_asleep = asleep_;
    activity_ = now; asleep_ = false;
    return was_asleep;
  }
};

struct ConnectionStatus {
  bool seen = false;
  uint32_t age_seconds = 0;
  bool connected() const { return seen && age_seconds < 3; }
};
} // namespace desk_ui
