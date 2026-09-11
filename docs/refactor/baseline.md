# Sticky Core Framework refactor baseline

> Historical note: this document records the pre-refactor system and the
> original removal plan. The final product decision retained Pregnancy as the
> third application alongside Home and Settings. See `migration-report.md` for
> the implemented result.

Date: 2026-09-11

## REFACTOR ANALYSIS

### Baseline build

The repository is built with PlatformIO and ESP-IDF. PlatformIO rejects the
workspace path because it contains spaces, so all firmware gates use an exact
source mirror under `/private/tmp`.

```bash
rsync -a --exclude .git --exclude .pio ./ /private/tmp/sticky_core_build/
cd /private/tmp/sticky_core_build
pio run -e sticky-release -e sticky-debug -e sticky-power-test
```

Toolchain and board:

- Platform: `espressif32@6.11.0`
- Framework: ESP-IDF 5.4.1
- Board: `sticky_esp32s3`
- Target: ESP32-S3, 32 MB flash, 8 MB PSRAM
- C++ standard: C++17
- Partition: 8 MB factory app plus 23.94 MB SPIFFS assets partition

All three clean builds passed.

| Environment | Linked flash | RAM | firmware.bin |
| --- | ---: | ---: | ---: |
| sticky-release | 2,189,948 B (26.1% of app partition) | 17,652 B (5.4%) | 2,190,608 B |
| sticky-debug | 2,193,712 B (26.2%) | 17,652 B (5.4%) | 2,194,368 B |
| sticky-power-test | 2,193,920 B (26.2%) | 17,652 B (5.4%) | 2,194,576 B |

`tools/run_host_tests.sh` passed 34/34 tests.

### Current architecture

#### Boot

`src/main.cpp` initializes board power, charger, shared SPI, sensor I2C, RTC,
battery, display, touch, buzzer, IMU, NVS, language storage, Bunny onboarding,
and finally the application coordinator. Required board/display/input failures
halt safely; RTC and battery failures use degraded fallbacks.

#### Applications

The launcher currently exposes five business applications:

1. Desktop Pet (also the implicit Home and default app)
2. Pomodoro
3. Status Board
4. Book of Answers
5. Pregnancy

Bunny onboarding is a sixth product flow executed before the app coordinator.
Each business app owns a FreeRTOS task and uses `StickyAppLifecycle` for a
cooperative pause/resume boundary.

#### App lifecycle and routing

`src/app/sticky_app.cpp` acts as coordinator, manager, launcher controller,
input dispatcher, sleep coordinator, and registry. It contains a switch for
every app's start/pause/resume/power hooks and one started flag per app.

The launcher owns touch/button/IMU while open. The active app is cooperatively
paused before launcher rendering, which gives the launcher temporary display
ownership. A selected app is started once and resumed on later activation.

`sticky_app_router.*` is pure and host-tested, but its orientation and shake
routes return concrete business IDs (Pomodoro, Status Board, Book of Answers).

#### Input flow

```text
GT911 task / button component / IMU task
                 |
                 v
       sticky_app coordinator
          |             |
 launcher open      launcher closed
          |             |
 launcher routes    active app task consumes touch
```

The ownership concept is sound, but it is implicit rather than represented by
a generic input-owner abstraction.

#### Display flow

Apps draw through the common 2-bit `Canvas`. `sticky_display` owns both PSRAM
framebuffers, physical rotation, the global battery overlay, and SSD1677
commits. The coordinator pauses the active app before launcher rendering.

Refresh capabilities already present:

- 4-gray full refresh
- monochrome full refresh
- partial refresh
- fast first frame during app transitions
- periodic full cleanup after fast transitions
- final monochrome full refresh before sleep
- retained e-paper frame during deep sleep

#### Sleep and wake

`sticky_app.cpp` asks each concrete app whether sleep is allowed, which timeout
to use, and how to prepare. Desktop Pet and Pregnancy can return a business
event epoch. The coordinator subtracts a 15-second lead and asks
`board_power_enter_deep_sleep()` to enable ESP32 timer wake. The AI key is the
button wake source. An RTC-retained struct restores the concrete app ID and
canvas rotation.

This is the largest system-to-business coupling. The current PCF8563 driver
supports validated read/write and build-time seeding, but it does not expose
the chip alarm/interrupt registers. Scheduled wake currently uses the ESP32
deep-sleep timer, not a PCF8563 interrupt.

#### Persistence

- Desktop Pet: NVS dual-slot records with sequence, schema, payload size and
  checksum; includes legacy migration.
- Pregnancy: one versioned/checksummed NVS record.
- UI language: separate NVS namespace/key.
- Onboarding completion: separate Bunny-specific NVS state.
- Last active app: only RTC memory, not durable NVS settings.

The dual-slot/checksum ideas are worth preserving, but their implementation is
named for and typed around Pet state.

#### Device and board layer

The following implementations are business-independent and must be kept:

- SSD1677 display and framebuffer/refresh controller
- GT911 touch driver, recovery policy and event queues
- PCF8563 RTC read/write/validation
- BQ27220 battery gauge
- physical buttons and AI-key wake
- LSM6DS3 IMU and generic shake/orientation detection
- buzzer/LEDC transport (business sound patterns require cleanup)
- board power latch and deep sleep sequence
- shared sensor I2C and display/SD SPI preparation
- charger and external-power detection
- Canvas, bitmap assets, fonts and bilingual UI infrastructure

Wi-Fi is linked transitively by ESP-IDF but no mature application-facing Wi-Fi
service exists in this source tree. SD is only placed in a safe idle state; no
filesystem service exists. Neither will be expanded during this refactor.

### Dependency graph and coupling points

```text
main.cpp
  -> Bunny onboarding
  -> sticky_app coordinator

sticky_app coordinator
  -> Desktop Pet / Pomodoro / Status / Answers / Pregnancy
  -> per-app lifecycle and power policy
  -> business-specific orientation and shake routes
  -> Pet as default app and Home target
  -> Pet/Pregnancy next-event epochs
  -> display, touch, buttons, IMU, buzzer, board power

Desktop Pet
  -> RTC time and autonomous-event scheduler
  -> dual-slot NVS save implementation
  -> pet-specific buzzer patterns and assets

Display
  -> common battery overlay only

Board/devices/input/sensors
  -> no application includes
```

Concrete Pet/System coupling found:

- global current app defaults to `DesktopPet`
- cold boot fallback is `DesktopPet`
- invalid router result defaults to `DesktopPet`
- double-click explicitly calls `return_to_desktop_pet()`
- Desktop Pet owns a special in-app `return_home()` path
- onboarding can be reopened only through Desktop Pet
- scheduled timer wake background window is restricted to Desktop Pet
- sleep timeout and next wake are queried through Pet-specific functions
- button and buzzer readiness logs still describe Pet behavior
- reliable record header/checksum utilities use `pet_` names and Pet types

### Removal map

#### KEEP

- board power, charger, pins, sensor I2C and shared SPI
- display driver, Canvas and refresh coordination
- touch, buttons, RTC, battery, IMU and generic shake detector
- buzzer transport and generic alarm/sleep feedback
- app lifecycle pause/resume mechanism
- launcher ownership and touch selection behavior
- language selection and CJK font infrastructure
- host-test runner and generic driver/policy tests

#### GENERALIZE

- `sticky_app.cpp` -> registry-driven app coordinator/manager
- `StickyAppId` -> Home, Settings and invalid-safe values
- concrete launcher tables -> App Registry descriptors
- double-click Pet Home -> generic Home
- RTC-retained business app context -> validated App Manager restore
- app power hooks -> generic lifecycle/power descriptor
- Pet next event -> generic Scheduler
- Pet dual-slot/checksum storage -> Persistent State / Settings Store
- business buzzer enum/logs -> generic feedback capabilities
- orientation/shake selection -> optional generic launcher events, not fixed app IDs
- language persistence -> Device Settings

#### REMOVE

- Desktop Pet model, tasks, state, growth, dialogue, outing, policies and assets
- third-party virtual-pet notices after no code depends on them
- Book of Answers app, database, assets and shake business flow
- Status Board state, UI, pet animation and assets
- Pomodoro app, timer UI and assets
- Pregnancy app, due-date storage, UI and assets (reserved as a future app)
- Bunny onboarding and its assets
- all business-only tests and asset generators
- business-only logging flags and documentation

#### UNKNOWN / verify during phases

- whether every current buzzer pattern has a generic future use
- whether orientation-based direct launch is worth keeping once only two apps
  remain
- whether the PCF8563 interrupt is physically routed on this board; current
  code and pin map do not expose it
- whether the large SPIFFS partition should be resized; leave partition layout
  stable unless a concrete product requirement needs the space

### Actual phase plan

1. Add a minimal Generic Home app; make boot, fallback and double-click target
   Home while all business apps still build.
2. Add App Registry/App Manager descriptors and move concrete dispatch out of
   the coordinator. Add generic persistent device settings and scheduler.
3. Remove Desktop Pet after all system dependencies point to Home/registry.
4. Remove Book of Answers, Status Board, Pomodoro, Pregnancy and Bunny
   onboarding one at a time, running build and host-test gates after each.
5. Add the minimal Settings app and system test actions.
6. Finish generic power/scheduler/storage naming and safe fallbacks.
7. Remove dead assets, generators, tests, flags, translations and dependencies.
8. Run clean builds, host regression, flash the attached device, and perform
   observable boot/display/touch/launcher/RTC/battery/IMU smoke tests. Record
   manual-only sleep/wake checks in the hardware checklist.
9. Rewrite README and produce architecture, migration, hardware and app-author
   documentation using the final real APIs.

Every phase uses the same build gate. A failed gate is fixed before the next
phase begins.
