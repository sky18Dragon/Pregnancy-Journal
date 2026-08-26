# Getting Started

This guide takes a new reTerminal Sticky from source code to its first running Sticky Bunny screen.

## What you need

- reTerminal Sticky with an ESP32-S3R8 and 800 × 480 monochrome ePaper display.
- A USB-C cable that supports data.
- PlatformIO Core 6.1 or the PlatformIO IDE extension.
- Python 3 and Git.

The project pins `espressif32@6.11.0` and uses ESP-IDF 5.4.1. PlatformIO installs the matching framework and compiler packages automatically.

## Clone the repository

```bash
git clone https://github.com/limengdu/reTerminal_Sticky_Bunny.git
cd reTerminal_Sticky_Bunny
```

## Build the release firmware

```bash
pio run
```

`sticky-release` is the default environment. A successful build ends with `SUCCESS` and creates:

```text
.pio/build/sticky-release/firmware.bin
```

## Connect and upload

Connect Sticky over USB-C, then run:

```bash
pio run -e sticky-release -t upload
```

PlatformIO normally detects a port such as `/dev/cu.wchusbserial...` on macOS or `COM...` on Windows. A successful upload ends with `SUCCESS` and the device restarts.

## First boot

On a fresh installation the expected sequence is:

1. Board power, charger, buses, RTC, battery gauge and display initialize.
2. The ePaper panel performs a physical white full refresh.
3. Touch and buzzer services start.
4. The six-page tutorial opens once.
5. Completing or skipping the tutorial records the choice in NVS.
6. A new save opens on the pet egg page.

<p align="center">
  <img src="../images/onboarding/tutorial-overview.png" alt="Six-page first boot tutorial" width="760">
</p>

Use `BACK`, `NEXT` and `SKIP` at the bottom of the tutorial. The final page uses `START`. The pet home later provides a small book icon that can reopen the tutorial.

## Development build and logs

```bash
pio run -e sticky-debug -t upload
pio device monitor -e sticky-debug
```

The serial monitor uses `115200` baud. Successful startup includes `phase=ready result=ok`. Debug builds expose detailed app, input, storage and power events while keeping high-frequency frame logs behind separate compile-time switches.

## Clear user data

To repeat first boot and pet creation, erase the device and upload again:

```bash
pio run -e sticky-debug -t erase
pio run -e sticky-debug -t upload
```

If automatic reset cannot enter download mode, hold the device's download/reset combination for the board, reconnect USB, and repeat the command. Keep the cable short and disconnect noisy USB hubs during recovery.

## Next steps

- Learn the [Desktop Pet](Desktop-Pet.md).
- Learn the [App Launcher and Gestures](App-Launcher-and-Gestures.md).
- Use [Troubleshooting](Troubleshooting.md) when the screen or touch controller does not start.
