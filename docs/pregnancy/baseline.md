# Pregnancy Journal baseline

## Starting point

- Commit: `3a1e048769691f5fa88eb873c3de93798bf54194`
- Platform: PlatformIO, ESP-IDF 5.4.1, ESP32-S3, reTerminal Sticky
- Apps: Home, Settings, Pregnancy
- Host tests: 14/14
- Release build: PASS
- Flash: 427,576 B / 8,388,608 B (5.1%)
- Internal RAM: 16,916 B / 327,680 B (5.2%)
- PSRAM: two 96,000-byte display framebuffers are allocated at runtime

Because PlatformIO rejects the workspace path containing spaces, builds are
performed from an exact, no-space mirror under `/private/tmp`.

## Preserved hardware baseline

The starting firmware had a completed device verification for power latching,
SSD1677 e-paper, GT911 touch, top and side buttons, PCF8563 RTC, BQ27220 battery
gauge, IMU, buzzer, deep sleep, timer/button wake and app switching. Pregnancy
Sticky development preserves those HALs and ownership boundaries.

The new Pregnancy Journal image passed a physical cold-boot smoke test. The
remaining interaction, persistence, sleep/wake and fault cases in
[hardware-regression-checklist.md](hardware-regression-checklist.md) still
require hands-on regression testing; host and build gates do not substitute for
those tests.

## Historical phase plan and gate

1. Pregnancy profile, calculation, storage and tests.
2. First-use LMP/due-date setup.
3. Pregnancy Home dashboard.
4. Overview/Baby/Mom and offline content.
5. Reminder domain, persistence and scheduler.
6. Reminder UI and Home aggregation.
7. Checkup domain, UI and Home aggregation.
8. Advice UI.
9. Text journal and file persistence.
10. Seven-app launcher and Settings integration.
11. Refresh, wake and idle-power review.
12. Clean builds, host tests and hardware checklist.

Each implemented phase passed its host test and release build gate before the
next set of changes was started.

## Current snapshot after product-scope revision

- Firmware version: 2.0.0
- Apps after product-scope revision: 4 (Baby Week, Reminder, Checkup, Settings)
- Default home: Baby Week
- Host tests after product-scope revision: 18/18
- Clean release/debug/power-test builds: PASS
- Release Flash: 469,716 B / 8,388,608 B (5.6%), +42,140 B
- Release internal RAM: 19,980 B / 327,680 B (6.1%), +3,064 B
- Release binary: 470,384 B, +42,144 B
- Runtime display PSRAM allocation: unchanged at 192,000 B
- Previous seven-app image physical cold-boot smoke test: PASS (2026-09-12)
- Revised four-app image physical regression: REQUIRED
