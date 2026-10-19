# Seeed Studio reTerminal Sticky hardware reference

Last verified: 2026-09-27. This local note summarizes official Seeed Studio
material and maps it to this repository. Follow the linked source for revisions,
electrical detail and schematics.

## Product identity

reTerminal Sticky is a compact magnetic e-paper device, distinct from the
larger reTerminal E1001/E1002 series. Official dimensions are 106 × 65.5 ×
7.3 mm, weight 70 g and protection IP40. It has a glass touch front, corner
magnets, USB-C, MicroSD, microphone, buzzer and three user buttons.

## Main hardware

| Function | Official hardware | Repository status |
| --- | --- | --- |
| MCU | ESP32-S3 with 8 MB PSRAM | Targeted by `sticky_esp32s3` |
| Flash | 32 MB QSPI | Board JSON and partition table configured |
| Display | 3.97-inch, 800×480, 235 ppi, 4-level grayscale e-paper | SSD1677 driver; current product pages mainly 1-bit |
| Touch | Capacitive, GT911 over I2C0 | Implemented |
| Motion | LSM6DS3TR-C 6-axis IMU over shared I2C1 | Orientation implemented |
| RTC | PCF8563 over shared I2C1 | Implemented |
| Battery gauge | BQ27220 over shared I2C1 | Implemented |
| Battery | 750 mAh single-cell lithium battery | Read/charge state integrated |
| Environment | SHT40 temperature/humidity sensor | Hardware present; not integrated in this firmware |
| Audio input | PDM microphone | Hardware present; not used by this firmware |
| Audio output | Passive buzzer on PWM | Implemented |
| Storage | MicroSD on shared SPI2 | S1 normal lifecycle physically verified for boot, hot remove/reinsert, sleep unmount and wake remount |
| Wireless | 2.4 GHz Wi-Fi and BLE 5.0 | Hardware present; intentionally outside current offline firmware |

“Hardware present” does not mean the Pregnancy Journal currently initializes or
uses it. The firmware has no Wi-Fi, BLE provisioning, cloud sync, OTA, audio
recording or AI service.

## Pin and bus map

The values below match Seeed's official overview and
`src/board/pin_config.h`.

| Subsystem | Signals |
| --- | --- |
| Power latch | HOLD GPIO45, LOCK GPIO46 |
| AI/top button | GPIO4 |
| Side buttons | GPIO5, GPIO6 |
| Charger/external power | CHG_EN GPIO39, detect GPIO9 |
| Shared sensor I2C1 | SCL GPIO0, SDA GPIO1 |
| PCF8563 / BQ27220 / LSM6DS3 | `0x51` / `0x55` / `0x6A` |
| E-paper SPI2 | MOSI 14, CLK 13, MISO 12, CS 15, DC 16, RST 17, BUSY 18, EN 47 |
| MicroSD on SPI2 | CS 8, EN 10, detect 11; data/clock shared with display |
| GT911 I2C0 | SCL 2, SDA 3, EN 42, INT 21, RST 41 |
| Buzzer | GPIO48 |
| Microphone | PDM CLK 19, DATA 20, power enable 38 (official dashboard demo, rechecked 2026-09-25) |
| USB UART | TX GPIO43, RX GPIO44 |

### Shared-resource rules

- BQ27220, PCF8563, SHT40 and LSM6DS3TR-C reuse one board-owned I2C1 bus.
- GT911 uses a separate I2C0 bus.
- E-paper and MicroSD share SPI2 signal lines and use different chip-selects.
  The board owns bus initialization; device drivers serialize SPI transactions.
  S0 tests concurrent SD writes and panel refreshes without unmounting between
  them. Mount SD before panel traffic; close files before unmounting, and do not
  free the shared bus. Concurrent operation still needs physical validation.
- Explicitly set unused SPI data pins to `-1`; leaving them as `0` can collide
  with the sensor bus on GPIO0.
- Peripheral power is software controlled. Shut down active tasks before
  removing their rail for deep sleep.

## E-paper behavior

The official development guide distinguishes three useful modes:

| Mode | Suitable content | Project API |
| --- | --- | --- |
| Four-level grayscale full refresh | Static imagery using four gray levels | `sticky_display_refresh()` |
| Black/white full refresh | Stable monochrome page, cleanup, sleep frame | `sticky_display_refresh_monochrome()` |
| Black/white partial refresh | Small on-page changes such as counters/status | `sticky_display_refresh_partial()` |

The panel retains its image with display power removed. Partial refresh is not
free of artifacts: schedule periodic full cleanup and never treat the panel as
a high-frame-rate display. The repository's display layer also handles the
installed panel's 180-degree physical rotation and SSD1677 comparison frame.

## Power and development notes

- Official development documentation uses ESP-IDF v5.4; this repository pins
  the PlatformIO ESP-IDF 5.4.1 package.
- Use a USB-C data cable for flashing; a charge-only cable will not expose a
  serial port.
- E-paper content remains visible in deep sleep. The current firmware turns off
  display power after drawing a final frame.
- Official factory firmware capabilities (Seeedash, Wi-Fi, OTA, AI voice) do not
  automatically apply after flashing this custom firmware.
- The official schematic is the electrical source of truth; this note is an
  implementation-oriented index, not a replacement for it.

## Official sources

- [Hardware Overview](https://www.seeedstudio.com/sticky/docs/en/device-guide/hardware-overview/) — dimensions, modules, signals and system architecture.
- [reTerminal Sticky schematic (PDF)](https://files.seeedstudio.com/wiki/reterminal_sticky/res/reTerminal_Sticky_Schematic_diagram_260609.pdf) — circuit-level reference.
- [ESP-IDF Development Basics](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-basics/) — ESP-IDF v5.4 setup and demo hardware inventory.
- [ESP-IDF Pages and Peripherals](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-pages/) — bus ownership, input flow and peripheral integration patterns.
- [ESP-IDF Refresh and Low Power](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-refresh/) — refresh modes, deep sleep and state restore.
- [Seeed_GFX2](https://github.com/Seeed-Studio/Seeed_GFX2) — official cross-platform display stack and Sticky support.
- [Sticky Playground Registry](https://github.com/Seeed-Projects/reterminal-sticky-playground-registry) — community firmware packaging and publishing reference.
