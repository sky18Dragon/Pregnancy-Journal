# Historical hardware regression record (archived)

This is the completed record for the earlier three-app image. It is retained
for provenance only; it is not a pass for the current six-app firmware. Use
[`docs/pregnancy/hardware-regression-checklist.md`](../pregnancy/hardware-regression-checklist.md)
for the current run.

Completed on a Seeed Studio reTerminal Sticky after flashing `sticky-release`.

- [x] Boot reaches `phase=ready`; Home shows time, date, battery and RTC state.
- [x] Swipe up from the bottom edge opens a three-card launcher.
- [x] Home, Pregnancy and Settings cards select and return cleanly.
- [x] Double-click returns Home from Settings and Pregnancy.
- [x] Settings language toggle changes both launcher and page copy between English and 简体中文.
- [x] Settings time editor writes a valid PCF8563 time and rejects invalid input.
- [x] Settings clean-refresh action clears accumulated e-paper ghosting.
- [x] Pregnancy first use accepts device time and due date, then dashboard updates week/day/progress.
- [x] Battery overlay still reports charge and external-power state.
- [x] Side-button sleep chord performs a final full refresh and deep sleep.
- [x] Wake from sleep restores the valid app context or safely falls back to Home.
- [x] No touch, RTC, I2C, SPI, display or task watchdog errors appear in the serial log.

## Test record

- Date: 2026-09-12
- Firmware: `1.0.0`
- Commit: `fdc6be6`
- Device port: `/dev/cu.usbmodem5C843369951`
- Host suite: 14/14 passed
- Boot result: `phase=ready`, `apps=3 default=home current=home`
- Device result: all physical interaction checks passed; no failed item reported
