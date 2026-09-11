# Sticky Core Framework

Sticky Core Framework is the small, reusable firmware foundation for Seeed
Studio reTerminal Sticky. It keeps the board drivers, e-paper refresh policy,
touch/buttons, RTC, battery gauge, IMU, buzzer, shared buses and deep-sleep
behavior while exposing a generic application runtime.

The shipped applications are:

- Home: landscape clock, date, battery and RTC status.
- Settings: bilingual language switch, RTC time editor and display clean-refresh test.
- Pregnancy: the retained pregnancy-week tracker with first-use date/time setup.

The launcher is owned by the framework. Applications receive exclusive display
and touch ownership through lifecycle callbacks; invalid or unavailable app IDs
fall back to Home. Settings are stored as versioned, checksummed records in two
NVS slots so a torn write cannot destroy the previous configuration.

## Build

PlatformIO and ESP-IDF are required. PlatformIO rejects paths containing spaces,
so build from a no-space checkout or mirror:

```bash
pio run -e sticky-release
pio run -e sticky-debug
pio run -e sticky-power-test
tools/run_host_tests.sh
```

The supported board is `sticky_esp32s3`. The release profile is the normal
device firmware; debug enables diagnostic logs and power-test shortens idle
timeouts.

## Validated baseline

Firmware `1.0.0` at commit `fdc6be6` was built, flashed and verified on a
Seeed Studio reTerminal Sticky on 2026-09-12. The focused host suite passed
14/14 tests. The release image uses 427,576 B of flash and 16,916 B of RAM;
the generated firmware binary is 428,240 B. Device logs confirmed successful
RTC, battery gauge, SSD1677 display, GT911 touch, IMU and button startup, and
the three-app runtime reached Home. The full physical interaction checklist
was also completed successfully.

## Flash and monitor

```bash
pio run -e sticky-release -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --filter time --rts 0 --dtr 0
```

After boot, swipe up from the bottom edge or single-click the top button to
open the launcher. Double-click returns Home. Open Settings to switch English
and 简体中文, set the PCF8563 time, or force a full e-paper cleanup refresh.
Pregnancy opens from the launcher; its first-use flow asks for device time and
the estimated due date.

## Adding an app

Implement the lifecycle surface described in
[`docs/refactor/adding-an-app.md`](docs/refactor/adding-an-app.md), add one
descriptor to `src/app/app_registry.cpp`, add its source files to
`src/CMakeLists.txt`, and add a focused host test. The registry is the only
composition point that knows concrete applications; the coordinator and
launcher remain generic.

## Architecture and verification

- [`docs/refactor/baseline.md`](docs/refactor/baseline.md) records the pre-refactor graph and hardware baseline.
- [`docs/refactor/architecture.md`](docs/refactor/architecture.md) describes the final ownership model.
- [`docs/refactor/migration-report.md`](docs/refactor/migration-report.md) records staged changes, tests and firmware sizes.
- [`docs/refactor/hardware-regression-checklist.md`](docs/refactor/hardware-regression-checklist.md) records the completed device verification.

The project retains Seeed Studio board integration and the original upstream
repository attribution. See [`LICENSE`](LICENSE) and the notices under
[`third_party/fonts/NotoSansCJK`](third_party/fonts/NotoSansCJK) for licenses.
