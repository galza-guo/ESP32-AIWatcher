# Experimental RGB bounce-buffer driver

Build with `pio run -d firmware -e s3-eya-rgb-bounce` from the repository root.
This environment pins pioarduino 55.03.312 (Arduino 3.3.12 / ESP-IDF 5.5.5),
LovyanGFX 1.2.29, and ArduinoJson 7.4.3. The default `s3-eya-rgb` environment
remains the previous driver and is available for rollback.

## Why a separate driver

Internal drawing tiles reduced application memory traffic but did not eliminate
the user's reported jitter. The old LovyanGFX RGB bus still used DMA directly
from a PSRAM framebuffer. ESP-IDF's RGB driver can instead feed DMA from two
internal SRAM buffers, refilled by the CPU through the cache. Espressif documents
this as a way to reduce susceptibility to PSRAM bandwidth contention:

- [RGB bounce buffers](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html)
- [Display drift FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html)

`src/panel_bounce.hpp` connects the native driver's back framebuffer to a
LovyanGFX sprite, so the existing UI, touch code and changed-region rendering
remain in use. Two full PSRAM framebuffers separate rendering from scanout.
DMA has two 10-line buffers (32,000 bytes total). Cache
invalidation is left disabled to avoid invalidating data written by rendering.

## Signal compatibility

The nominal clock remains 15 MHz, with the same 800×480 porch timing and pins.
Data GPIOs retain the old bus's byte-swapped mapping for LovyanGFX RGB565 bytes.
HSYNC and VSYNC idle low. Pixel data changes on the rising clock edge, matching
the old bus's actual register value: LovyanGFX 1.2.29's `Bus_RGB.cpp` assigns
`lcd_ck_out_edge` from `pclk_idle_high`, ignoring `pclk_active_neg`.

After all changed tiles have been drawn, the back framebuffer is published with
`esp_lcd_panel_draw_bitmap`. The native bounce driver selects the new buffer at
a frame boundary. Rendering waits for two subsequent frame-complete callbacks
before reusing the retired buffer, conservatively covering a callback racing
with publication. Only changed row spans are copied to bring the retired buffer
up to date. A 250 ms timeout stops rendering instead of writing into a potentially
active scanout buffer. Hardware validation is still needed.

## Hardware validation

Before changing SDK generations, save both the current application image and
the first 64 KiB of flash (bootloader and partition metadata). A rollback must
restore both when the new upload has replaced them. Stop the serial collector
for flashing, then restart it for startup logs and touch checks.

Expected startup log: `rgb_driver=idf_bounce` with `bounce_bytes=32000`.
Confirm System refresh and tab transitions on the physical panel; compilation
and desktop renderer tests cannot establish scanout stability.
