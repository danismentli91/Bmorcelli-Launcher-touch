# Waveshare ESP32-S3-Touch-LCD-1.83

## Controls

- Display: fixed portrait, 240 x 284, rotation 0. Old NVS/SD landscape settings are ignored.
- Touch: single tap selects the highlighted item **after 300 ms**; double tap goes back without first selecting. Swipe up/left advances; down/right goes to the previous item. Hold for 700 ms and release goes back.
- BOOT: one short click advances after 300 ms; two clicks go to the previous item. Hold 550-1199 ms and release to select; hold at least 1200 ms and release to go back.
- PWR: AXP2101 short press selects; its long-press event goes back. Continuing to hold PWR retains the PMIC's hardware power-off behavior.
- The first touch/BOOT gesture on a dimmed/off screen only wakes it.
- This is gesture navigation: a tap selects the highlighted item, not an arbitrary row under the finger. Coordinate deltas drive swipes; there is no second coordinate-click path to leak into a submenu.

## Hardware sources

Verified against Waveshare's [official schematic, page 1](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.83/ESP32-S3-Touch-LCD-1.83-schematic.pdf):

| Function | Connection |
| --- | --- |
| LCD | DC 4, CS 5, SCK 6, MOSI 7, RESET 38, BL 40 |
| I2C | SDA 15, SCL 14 |
| CST816 family | address 0x15, RESET 39, INT 13 |
| BOOT | GPIO0, active low |
| PWR | AXP2101 PWRON, address 0x34; not a GPIO button |
| microSD SPI | MOSI 1, SCK 2, MISO 3, CS 42; powered by VCC3V3 |

The [official Arduino example](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.83/blob/main/examples/arduino/01_HelloWorld/01_HelloWorld.ino) uses ST7789 rotation 0 and zero offsets for this panel. The bundled [DriveBus driver](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.83/blob/main/examples/arduino/libraries/Arduino_DriveBus/src/touch_chip/Arduino_CST816x.cpp) documents reset delay, FingerNum/XY registers and IRQ mode bits. We select periodic plus change reporting (0xFA = 0x60), read 0x02..0x06 together, and do not mix hardware GestureID with software tap timing. Failed I2C reads and invalid frames are not releases. Auto-sleep disabling is avoided because the bundled SensorLib driver notes all-FF responses on some CST816T revisions.

The [official AXP2101 register definitions](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.83/blob/main/examples/esp-idf/01_AXP2101/main/axp2101_registers.h) define INTEN2 0x41 and INTSTS2 0x49, short press bit 3 and long press bit 2. Poll and acknowledge only those bits. Do not change regulator voltages, battery charging or hardware shutdown timing.

## Build and test

`waveshare-183.yml` runs the C++ gesture regression test, builds this target, verifies the exact component bytes at 0x0000 / 0x8000 / 0x10000, and uploads only one merged `.bin` plus its SHA-256 checksum.

Flash `Launcher-waveshare-esp32-s3-touch-lcd-183.bin` at **0x0000**. It includes the bootloader, partition table and Launcher; it is not an app-only OTA image. Existing data in that address range is overwritten.

On the actual board, check portrait orientation, ten separate taps, repeated double taps (no select before back), swipes, BOOT single/double/hold, PWR short press, and entering Settings without opening Brightness. Test a known-good FAT32 microSD card, opening a directory and reading a firmware file. Build and simulated input tests do not establish electrical or physical touch reliability; these checks still require the user's card.
