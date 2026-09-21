# ESP32 firmware

The Omarchy window is the working display. This sketch receives the same JSON
the host writes to the CH340 serial port at 115200 baud.

A line looks like:

```json
{"v":1,"t":1710000000000,"sys":{"cpu":18.2,"mem":41.0,"temp":40.1,"fan":0},"ai":{}}
```

Flash `desk_monitor.ino` with the ESP32 Arduino core. The board in hand is an
ESP32-S3 with a QinHeng CH340 (`/dev/ttyUSB0`). Without a panel the sketch
only keeps the link alive. Pins for a later 320×240 ILI9341 are listed in the
sketch.
