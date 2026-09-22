# ESP32-AIWatcher

A small, USB-connected desk display for Linux system performance and AI usage.
An ESP32-S3 renders a minimal touch interface; a Python collector supplies the
readings from your computer over serial.

## What it shows

- **System:** CPU, memory, temperature, fan speed, and upload/download speeds
  with live traces and persistent daily network totals.
- **Usage:** GPT/Codex, Grok, and MiniMax totals, available cost estimates,
  recent activity, and usage limits from local usage files.
- **Settings:** brightness, Light/Dark appearance, idle sleep and read-only
  connection details. Preferences persist on the device.
- A minimal interface with touch tabs and a physical BOOT-button
  shortcut. A companion browser dashboard runs at `http://127.0.0.1:7824/`.

**Latest release: v1.1.0.** Adds [display settings](firmware/SETTINGS.md),
[network monitoring](firmware/NETWORK.md), and stronger dark-mode typography.
The double-framebuffer display driver retains the v1.0.0 tearing fix.
See [hardware validation](firmware/STATUS.md) and
[download the release](https://github.com/galza-guo/ESP32-AIWatcher/releases/tag/v1.1.0).
Upgrade both firmware and the host collector to receive Network data.

## Hardware

Tested on the **EYA EA4313-S3**, with an ESP32-S3, 8 MB PSRAM, 16 MB flash,
800×480 4.3-inch RGB display, GT911 touch, and CH340 USB serial.

The firmware is board-specific. RGB wiring, touch initialization, and display
settings are documented in [firmware/README.md](firmware/README.md). Other
PlatformIO environments are historical configurations and are not validated.

## Run the collector

The host uses Python 3's standard library and Linux `/proc` and hwmon interfaces.
Temperature and fan readings depend on the sensors exposed by your system.

```bash
git clone https://github.com/galza-guo/ESP32-AIWatcher.git
cd ESP32-AIWatcher
python3 host/desk_monitor.py
```

Open `http://127.0.0.1:7824/` for the companion dashboard. The collector also
tries the configured CH340 device, `/dev/ttyUSB0`, and `/dev/ttyACM0`; your user
must have serial-device access. It samples system metrics at 6 Hz and reloads
AI usage every 15 seconds. It does not query provider APIs or require API keys.

AI usage currently comes from an existing Omarchy agent-usage setup:

```text
~/.local/state/omarchy/agents/usage/{codex,grok,minimax}.json
~/.grok/sessions/                         # Grok cost metadata
```

This repository reads those files; it does not generate them. Without them,
usage values are unavailable or default to zero. System monitoring still works.
The seven-day traces reflect the source files' activity counts, not token totals.

The optional `systemd/`, `udev/`, and `bin/` files are examples from the original
Arch/Omarchy installation. Adjust absolute paths and serial-access groups for
your machine before installing them. The `desk-monitor-ui` launcher additionally
requires Omarchy and Hyprland; an ordinary browser works without either.

## Build and flash

Install PlatformIO Core, connect the board, and stop any collector or serial
monitor that owns its port:

```bash
pio run -d firmware -e s3-eya-rgb-bounce
pio run -d firmware -e s3-eya-rgb-bounce -t upload
```

Restart the collector afterward. For an existing user service:

```bash
systemctl --user restart desk-monitor.service
journalctl --user -u desk-monitor.service -f
```

The active firmware is `firmware/src/main.cpp`. The older Arduino sketch under
`firmware/desk_monitor/` is retained as history, not the current application.
Factory flash dumps and generated build outputs are not included in this repo.

## Development

```bash
python3 -m unittest discover -s tests
```

A native preview uses the same LovyanGFX drawing code and fonts as the firmware.
After a firmware build has installed its dependencies, install CMake, a C++
compiler, and SDL2 development headers, then run:

```bash
cmake -S firmware/preview -B /tmp/aiwatcher-preview -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/aiwatcher-preview -j4
cd /tmp/aiwatcher-preview
./preview
```

This writes PPM previews and checks touch hit areas and pixel equivalence of
incremental rendering. Physical display timing still requires hardware testing.

| Path | Purpose |
| --- | --- |
| `firmware/src/main.cpp` | Serial input, touch, and rendering updates |
| `firmware/src/ui.hpp` | Shared minimal interface layout |
| `firmware/src/panel.hpp` | Board wiring and RGB configuration |
| `host/desk_monitor.py` | Linux metrics, local AI usage, HTTP and serial |
| `ui/` | Companion browser dashboard |

Built with [LovyanGFX](https://github.com/lovyan03/LovyanGFX),
[ArduinoJson](https://github.com/bblanchon/ArduinoJson), and
[PlatformIO](https://github.com/platformio/platformio-core).
