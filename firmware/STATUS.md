# Stable hardware validation — v1.0.0

On 2026-09-21, the user confirmed that the previously observed periodic tearing
in the lower System page and during System / Usage transitions was completely
resolved. The double-buffered native RGB driver is the stable default.

## Verified configuration

- EYA EA4313-S3: ESP32-S3, 8 MB PSRAM, 16 MB flash, 800×480 RGB, GT911 touch.
- Environment: `s3-eya-rgb-bounce`, pioarduino 55.03.312.
- Arduino 3.3.12 / ESP-IDF 5.5.5; LovyanGFX 1.2.29; ArduinoJson 7.4.3.
- Two PSRAM framebuffers and two internal 10-line DMA buffers (32,000 bytes).
- Internal rendering tile: 90,592 bytes; nominal pixel clock: 15 MHz.
- Display, GT911 touch, live System updates and tab transitions work on hardware.
- First sampled System updates took 74–80 ms, including frame-boundary waits.

## Validation

Firmware build and flash hash verification passed. Startup logs confirmed all
buffers and touch initialization, with no observed frame-boundary timeout.
Four Python host tests passed. Native preview assertions passed for navigation,
incremental card rendering, strip-based page transitions, empty data, shorter
values, and synchronizing the retired framebuffer after presentation.

The active firmware is `src/main.cpp`; `src/ui.hpp` holds the shared layout and
`src/panel_bounce.hpp` the stable native driver. The older Arduino sketch under
`desk_monitor/` is not the active application. See [BOUNCE.md](BOUNCE.md) for
buffer ownership and [README.md](README.md) for wiring and logging.

## Historical baseline and rollback

The legacy `s3-eya-rgb` environment uses Arduino 2.0.17 and LovyanGFX's
single-framebuffer RGB bus. Touch worked, but System updates and tab transitions
still tore after incremental rendering reduced memory traffic. This environment
is retained for rollback, not as the default build.

A separate experiment combining a 12 MHz clock with staged scanline copies
produced alternating solid colors and was rolled back; its exact cause was
not isolated. The stable version retains the working nominal 15 MHz timing.

Before changing SDK generations, preserve both the current application image
and the first 64 KiB of flash. Factory dumps and local rollback binaries are
excluded from Git and are not included in public releases.
