# Firmware Architecture

The firmware separates hardware ownership, pure decision logic, application lifecycle and page rendering. This keeps behavior testable on a computer while the ESP-IDF layer remains responsible for real devices.

## Startup flow

`app_main()` performs the following sequence:

1. Initialize the logging policy and print firmware/build identity.
2. Latch board power and enable the charging path.
3. Isolate MicroSD before ePaper takes the shared SPI2 bus.
4. Initialize the shared sensor I2C bus.
5. Attach RTC and battery gauge with non-fatal fallback handling.
6. Initialize the display and obtain its shared `Canvas`.
7. Perform a physical white clear after a cold boot.
8. Initialize touch and buzzer services.
9. Open the one-time onboarding flow or start the app manager.

## Ownership model

Only one application owns the shared display and touch stream at a time. The app manager pauses the active app at a safe loop boundary before another app starts, preserves restorable state, sets the correct display rotation and resumes an existing app when appropriate.

The launcher is not a fifth long-running app. It is an overlay owned by the manager, with its own temporary IMU session and touch map.

## Layers

| Layer | Responsibility |
| --- | --- |
| `src/board` | Electrical startup, shared buses, charger and power latch |
| `src/devices` | RTC, battery and buzzer interfaces |
| `src/input` | Button events and queued GT911 touch points |
| `src/sensors` | Accelerometer sampling, stable orientation and shake sessions |
| `src/display` | ePaper initialization, framebuffer and physical refresh modes |
| `src/ui` | Canvas primitives, text, overlays and compiled artwork |
| `src/app` | Launcher, routing, gestures, orientation and app lifecycle |
| `src/apps` | Product behavior and page-specific input/render loops |

## Pure logic and host tests

Time rules, state transitions, touch rectangles, orientation mapping, countdown policy, gesture qualification, save checksums and most page rendering have no ESP-IDF dependency. Native tests compile them with the host C++ compiler and write PPM previews from the same `Canvas` used by firmware.

## Application structure

Each application normally contains:

- an app entry and FreeRTOS lifecycle owner (`*_app.cpp`);
- state and policy files with deterministic transitions;
- page rendering and hit-test files;
- generated pixel assets in `src/ui/assets`;
- native behavior and visual regression tests in `test/`.

## Display refresh contract

Applications draw into a shared 2-bit grayscale framebuffer. A refresh request transfers the required mode to the physical panel. Slow operations remain outside pure state mutation; input queues and pending actions preserve user intent across refresh completion.

## Persistence

NVS stores durable onboarding and pet records. RTC memory stores short-lived app restoration state across deep sleep. PCF8563 supplies calendar time and wake alarms. These three storage classes are kept separate because they have different lifetime and write-cost characteristics.
