# Hardware and Drivers

The firmware targets the Seeed Studio reTerminal Sticky and follows the hardware reference implementation for bus ownership, controller reset sequences and board power.

<p align="center">
  <img src="../images/hardware/sticky-button-layout.png" alt="Sticky button and SD card layout" width="760">
</p>

## Input and board controls

| Function | GPIO |
| --- | ---: |
| AI / launcher button | 4 |
| Side button 1 | 5 |
| Side button 2 | 6 |
| Power hold | 45 |
| Power lock | 46 |
| Passive buzzer | 48 |

The three user buttons are shallow side buttons on the same physical edge. The two non-AI keys form the deliberate sleep chord.

## Shared sensor I2C bus

The board sensor bus uses GPIO 0 for SCL and GPIO 1 for SDA.

| Device | Address | Use |
| --- | ---: | --- |
| LSM6DS3TR-C | `0x6A` | Acceleration, orientation and shake sessions |
| PCF8563 | `0x51` | Calendar time and scheduled alarms |
| BQ27220 | `0x55` | State-of-charge percentage |

## Touch bus

The GT911 uses a separate I2C controller:

| Signal | GPIO |
| --- | ---: |
| SCL | 2 |
| SDA | 3 |
| Enable | 42 |
| Interrupt | 21 |
| Reset | 41 |

The reset sequence drives the interrupt level associated with address `0x14`, verifies controller ID `911`, reads the native 480 × 800 resolution and maps it into the current 800 × 480 physical framebuffer.

Touch polling uses a small queue. Applications consume discrete touch events, which lets an input received during a slow ePaper refresh execute after the refresh completes.

## ePaper and MicroSD SPI

The SSD1677 panel and MicroSD slot share SPI2 data and clock lines.

| Signal | GPIO |
| --- | ---: |
| MOSI | 14 |
| MISO | 12 |
| Clock | 13 |
| ePaper CS / DC / Reset / Busy / Enable | 15 / 16 / 17 / 18 / 47 |
| MicroSD CS / Enable / Detect | 8 / 10 / 11 |

Boot first places the MicroSD control lines into a defined idle state. The ePaper driver can then own SPI2 without another device driving the shared bus.

## Charger and external power

GPIO 39 controls the active-low BQ25616 charging path. GPIO 9 reports external power. The battery overlay combines this input with the BQ27220 percentage to draw the charging bolt and fill level.

## Driver source

Board coordination lives in `src/board/`, user-facing device drivers in `src/devices/`, input in `src/input/`, sensors in `src/sensors/`, and the display owner in `src/display/`. The vendored ePaper and GT911 components remain isolated under `components/`.
