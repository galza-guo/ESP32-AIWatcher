Factory EYA EA4313-S3 lvgl_demo dump (2026-09-21).

boot-pt.bin  flash 0x0 length 0x10000 (bootloader + partition table)
app.bin      flash 0x10000 (factory partition, 0x2ee000 max)

Restore:
  esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x0 boot-pt.bin 0x10000 app.bin

RGB pinout from lv_port_disp_init @ 0x42026f48:
  PCLK=6 HSYNC=4 VSYNC=2 DE=5 DISP=-1
  data 14 21 47 48 45 46 9 10 11 12 13 7 15 16 8 3
  BL=GPIO1  pclk=15MHz
  800x480  hsync 16/32/10  vsync 3/12/15  pclk_active_neg

Touch correction verified during hardware debugging:
  I2C SDA=42 SCL=41. GPIO39/40 are initialization control pins, not I2C.
  Factory initialization @ 0x4200a4fc sets both high, then both low,
  raises GPIO40, and keeps GPIO39 low. See ../README.md for timings.
  Verified GT911 address after initialization: 0x5D; config version 0x82.
