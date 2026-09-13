# Current baseline

This document records the implementation and verification baseline for the
current six-app image. Historical refactor analysis is kept under
`docs/refactor/` and is not a description of the current product.

## Firmware and build

- Repository commit: `2556df8`
- Firmware version: `2.0.0`
- Platform: PlatformIO, `espressif32@6.11.0`, ESP-IDF 5.4.1, ESP32-S3
- Board: `sticky_esp32s3`, Seeed Studio reTerminal Sticky
- Apps: Baby Week, Reminders, Checkups, Weight, Kicks and Settings
- Default app: Baby Week
- Host suite: 20/20 passed
- Release, debug and power-test profiles: build gates defined in
  `platformio.ini`; release build passed for this scope

PlatformIO rejects paths containing spaces. Reproducible builds therefore use
an exact mirror under `/private/tmp` or a no-space checkout. The custom
`partitions.csv` allocates 8 MiB to the factory application and 24 KiB to NVS.

The latest release build reported approximately 485,656 bytes of flash and
24,380 bytes of internal RAM, within the current partition and memory budget.
Two 96,000-byte display framebuffers remain allocated in PSRAM at runtime.

## Preserved hardware baseline

The Sticky Core integration retains power latching, SSD1677 e-paper, GT911
touch, top and side buttons, PCF8563 RTC, BQ27220 battery gauge, IMU, buzzer,
deep sleep, timer/button wake and shared bus ownership. The current product
adds application services without moving hardware ownership into an app.

## Functional baseline

- Pregnancy profile accepts confirmed due date or LMP, with redundant NVS
  storage and legacy due-date migration.
- Reminder and checkup services feed the central scheduler and due buzzer.
- Weight and kick services persist independent bounded records and expose local
  derived summaries.
- Launcher and all six apps use fixed landscape 800×480 layouts with English /
  Simplified Chinese copy where the translation table is provided.
- Full/partial e-paper refresh, idle sleep, timer wake and button wake remain
  coordinator-owned.

## Verification status

Host and firmware build gates are green. The current six-app image still
requires a fresh physical-device pass for touch coordinates, every new Weight
and Kicks interaction, e-paper ghosting, sleep/wake and fault recovery. Use
[hardware-regression-checklist.md](hardware-regression-checklist.md) and record
the actual board port and serial result; do not reuse the historical three-app
test record.
