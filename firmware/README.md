# ESP32 desk monitor

The UI runs on the panel, not on Omarchy. The ThinkPad only collects CPU /
memory / temperature / fan and the agent usage table, then writes one JSON
line at a time to the CH340 serial port.

## Panel

Default firmware is the **EYA EA4313-S3** 4.3" 800×480 RGB panel with GT911
touch (factory `lvgl_demo`). USB is CH340 on UART0.

| Signal | GPIO |
|--------|------|
| PCLK   | 6 |
| HSYNC  | 4 |
| VSYNC  | 2 |
| DE     | 5 |
| B0–B4  | 14, 21, 47, 48, 45 |
| G0–G5  | 46, 9, 10, 11, 12, 13 |
| R0–R4  | 7, 15, 16, 8, 3 |
| Backlight | 1 |
| GT911 SDA/SCL | 42 / 41 |
| GT911 reset / address-select control | 40 / 39 |

Touch initialization follows the factory GPIO39/40 sequence: both high for
60 ms, both low for 20 ms, GPIO40 high for 20 ms, then GPIO39 held low.
Allow another 300 ms for the controller to initialize before reading it over
hardware I²C. The verified address is `0x5D`, configuration version `0x82`,
resolution 800×480, with five touch points. Reading product ID `911` alone is
insufficient: without the initialization sequence the controller responded but
returned a zero configuration and no coordinates.

The left sidebar has two rounded buttons: System and Usage.
BOOT (GPIO 0) also switches pages.

## Rendering

The UI uses a 768 KB RGB565 canvas in PSRAM as its previous-image cache.
Page switches and Usage updates render 800×48 strips in internal SRAM, compare
each strip with that cache, then submit only changed row spans. The same buffer
is reused for System's live updates, which render one
596×76 card in a 90,592-byte **internal SRAM** tile, compare it against the
last canvas, and copy only changed spans of each row. Unchanged card backgrounds,
labels, navigation and headers never get copied during those updates. This
reduces contention with the display's PSRAM scanout. Early board samples took
19–20 ms per update and copied 5–7 KB after the initial data arrived, compared
with the previous 443 KB content-area copy. The pixel-equivalence preview test
checks that incremental updates match a full redraw, including shorter numbers
and missing readings. If the internal tile cannot be allocated, the firmware
logs `system_tile_internal=0` and falls back to the full content-area path.

Transfers pause for 30 µs after every eight tile rows to limit sustained memory
traffic. Desktop pixel comparisons cover strip boundaries, page transitions,
empty data and provider counts. Physical jitter verification remains necessary.

The attempted 12 MHz pixel clock and staged scanline presentation were rolled
back after the physical display showed alternating solid colors. The display
retains its working 15 MHz configuration.

Each tile is complete before submission, avoiding intermediate clear-and-redraw
steps within it. A page transition is progressive, not atomic. RGB scanout still
uses the driver's single framebuffer; this is not a hardware VSYNC swap.

`src/ui.hpp` contains the shared layout and drawing code. The desktop preview
uses the same LovyanGFX renderer and fonts as the firmware, with sample data:

```bash
cmake -S preview -B /tmp/desk-monitor-ui-build -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/desk-monitor-ui-build -j4
cd /tmp/desk-monitor-ui-build
./preview
```

This requires SDL2 development headers and the PlatformIO dependencies to
have been downloaded. It renders System, Usage and empty-state PPM images
and checks navigation hit areas. `page=... render_us=... present_us=...`
messages in the service journal measure composition and framebuffer-copy
time on the actual board.

Factory flash is saved under `firmware/factory/` so the LVGL demo can be
restored.

## Flash

```bash
cd firmware
pio run -e s3-eya-rgb -t upload
```

Host: `systemctl --user restart desk-monitor.service`

The host collector records firmware startup and touch coordinates in its journal:

```bash
journalctl --user -u desk-monitor.service -f
```

Use this log while the collector is running, rather than opening another serial
monitor on the CH340 port. An `ok:1` startup message confirms communication with
the controller, but successful touch input must be verified separately by
checking `touch x=... y=...` messages and page changes on the panel.
