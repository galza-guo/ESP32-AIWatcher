# Hardware checkpoint — 2026-09-21

## Current experiment: double-buffered native RGB output

The `fix/rgb-bounce-buffer` branch adds the `s3-eya-rgb-bounce` environment.
It was built and flashed successfully with Arduino 3.3.12 / ESP-IDF 5.5.5.
Startup logs confirm two full framebuffers, 32,000 bytes of internal DMA bounce
buffers, successful GT911 initialization, and the internal rendering tile.
Early System updates took 74–80 ms including frame-boundary waits, with no
timeout in the observed startup logs. Pixel-equivalence and retired-buffer
synchronization checks passed, as did the four host tests.

The user clarified that the issue resembles horizontal tearing in the lower
part of the display, recurring during updates. Physical confirmation of this
new driver's effect is pending. See [BOUNCE.md](BOUNCE.md) for its buffer
ownership protocol and rollback requirements. The default environment still
builds the prior driver described below.

## Previous baseline

The active ESP32-S3 firmware is `src/main.cpp`, with layout in `src/ui.hpp`
and board wiring/timing in `src/panel.hpp`. Build the `s3-eya-rgb` environment
from `platformio.ini`. The older `desk_monitor/desk_monitor.ino` is not the
firmware currently running on the board.

## Confirmed behavior

- Display and GT911 touch work; System / Usage tabs respond to taps.
- The minimal light UI and host serial collector are running on hardware.
- System updates use an internal SRAM card tile and changed-row spans.
- Page switches and Usage updates use internal SRAM strips.
- **Unresolved:** System still occasionally jitters; switching tabs also jitters,
  as confirmed by the user after the latest strip-rendering change.
- The display remains at its working 15 MHz pixel clock. A previous experiment
  combining 12 MHz with scanline presentation produced alternating full-screen
  solid colors and was rolled back. The exact cause was not isolated.
- The driver has one scanout framebuffer; page updates are not VSYNC swaps.

## Validation and reproduction

Last firmware build: PlatformIO espressif32 7.1.3, Arduino 2.0.17,
LovyanGFX 1.2.29, ArduinoJson 7.4.3. Dependencies currently use version ranges.
The `s3-eya-rgb` build and on-device flash hash verification passed. The shared
native renderer's assertions cover navigation, incremental card updates and
strip-rendered page transitions, including empty data and shorter values.
These checks do not establish physical scanout stability.

See `README.md` for build/preview instructions and UART logging. Use the host
collector's existing serial connection for logs; stop `desk-monitor.service`
before flashing and restart it afterward.

This Git checkpoint includes the earlier uncommitted host/firmware setup and
the subsequent touch, UI and rendering work. Factory binary dumps and build
outputs remain local and are excluded from Git.
