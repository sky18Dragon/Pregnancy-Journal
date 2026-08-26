# Testing and Debugging

Sticky Bunny uses two complementary test levels: native tests on the development computer and physical-device validation on reTerminal Sticky.

## Native test suite

Run the maintained test runner from the repository root:

```bash
./tools/run_host_tests.sh
```

The script compiles each first-party test with the system C++ compiler, runs it, and stops at the first failure. A successful run ends with `PASS 29/29`.

The suite covers:

- app routing, display orientation, swipe gestures and power policy;
- IMU continuous-shake policy;
- desktop-pet core, daily rules, outings, sounds, power and dual-slot storage;
- Pomodoro countdown, custom input and all principal pages;
- Status Board states, pages and animation poses;
- Book of Answers states and rendered results;
- onboarding state, navigation and all six pages;
- pixel asset and battery protocol helpers.

## Render previews

Rendering tests write PPM files under `/tmp`. Useful outputs include:

```text
/tmp/desktop_pet_home.ppm
/tmp/pomodoro_setup_25_minutes.ppm
/tmp/status_board_menu.ppm
/tmp/book_home.ppm
/tmp/sticky_launcher_portrait.ppm
/tmp/sticky_launcher_landscape.ppm
/tmp/onboarding_1.ppm ... /tmp/onboarding_6.ppm
```

These previews use the firmware's real canvas, font and asset arrays. They are suitable for coordinate, spacing, touch-map and regression review before a device upload.

## Firmware builds

```bash
pio run -e sticky-release -e sticky-debug -e sticky-power-test
```

The release profile proves optimized code size. Debug proves the detailed log configuration. Power-test proves the accelerated sleep policy still compiles with the same product code.

## Log categories

`platformio.ini` defines the overall compile level and independent high-frequency switches. Normal debug builds keep animation frames, raw IMU samples, timer ticks and display timing quiet until their individual macros are enabled.

Important stable tags include:

| Tag | Area |
| --- | --- |
| `sticky_boot` | Startup phase, memory and reset/wake reason |
| `sticky_touch` / `GT911` | Controller initialization and accepted touches |
| `sticky_imu` | Placement, motion and shake sessions |
| `sticky_app` | Launcher and app lifecycle |
| `desktop_pet_app` | Pet actions, progression, outings and readiness |
| `pet_storage` | Slot selection, sequence and checksum result |
| `pomodoro_app` | Timer state transitions |
| `status_board_app` | Status input and rendering transitions |
| `book_of_answers_app` | Shake qualification, animation and answer selection |

## Hardware validation

Physical validation focuses on what a host test cannot prove: ePaper ghosting, refresh latency, touch feel, IMU thresholds in hand, buzzer tone, battery operation, RTC alarm wake and behavior after unplugging USB.

Use a continuous action sequence and keep the serial log for the same sequence. This preserves the relationship between physical movement, final placement, screen refresh and event timestamps.
