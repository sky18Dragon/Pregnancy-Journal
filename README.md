# reTerminal Sticky Bunny

<p align="center">
  <strong>A living virtual pet and an interactive ePaper app collection for reTerminal Sticky.</strong>
</p>

<p align="center">
  <a href="README_CN.md">简体中文</a> ·
  <a href="docs/wiki/Getting-Started.md">Get started</a> ·
  <a href="docs/wiki/Home.md">Documentation</a> ·
  <a href="CHANGELOG.md">Changelog</a>
</p>

<p align="center">
  <img alt="ESP32-S3" src="https://img.shields.io/badge/MCU-ESP32--S3-000000?style=flat-square">
  <img alt="ESP-IDF 5.4.1" src="https://img.shields.io/badge/ESP--IDF-5.4.1-000000?style=flat-square">
  <img alt="PlatformIO" src="https://img.shields.io/badge/build-PlatformIO-000000?style=flat-square">
  <img alt="Firmware 0.1.0" src="https://img.shields.io/badge/firmware-0.1.0-000000?style=flat-square">
  <img alt="MIT License" src="https://img.shields.io/badge/license-MIT-000000?style=flat-square">
  <a href="https://github.com/limengdu/reTerminal_Sticky_Bunny/actions/workflows/build.yml"><img alt="Build and test" src="https://github.com/limengdu/reTerminal_Sticky_Bunny/actions/workflows/build.yml/badge.svg"></a>
</p>

<p align="center">
  <img src="docs/images/desktop-pet/home.png" alt="Sticky Bunny virtual pet home" width="245">
  &nbsp;&nbsp;
  <img src="docs/images/launcher/launcher-portrait.png" alt="Sticky Bunny portrait app launcher" width="245">
</p>

reTerminal Sticky Bunny turns the Seeed Studio **reTerminal Sticky** into a small, persistent world: raise a rabbit that remembers your care, start a Pomodoro session, show your availability, or ask the Book of Answers. Touch, buttons, swipe gestures, device rotation, continuous shaking, RTC scheduling, and low-power ePaper behavior are designed as one coherent firmware experience.

This repository contains the complete PlatformIO/ESP-IDF firmware, original monochrome artwork, host-side behavior and rendering tests, release packaging, and developer documentation.

## Why this project is different

- **A pet with continuity, not a static mascot.** It hatches, grows through five life stages, develops one of three personalities, remembers care, sleeps, speaks, plays, and occasionally goes outside.
- **Four polished on-device experiences.** The desktop pet, Pomodoro timer, status board, and Book of Answers share one launcher and one visual language.
- **The device itself is the controller.** Open the launcher by button or swipe, select by touch, rotate into portrait or landscape apps, and shake to enter the Book of Answers.
- **Designed for ePaper.** Stable screens use full or quality refreshes; time-sensitive views use bounded partial updates; input remains responsive while a refresh is in flight.
- **Built around real hardware behavior.** The firmware manages the shared display/SD SPI bus, GT911 touch controller, LSM6DS3TR-C IMU, PCF8563 RTC, BQ27220 fuel gauge, buzzer, side buttons, battery operation, and deep sleep.

## Application gallery

| Virtual pet | Pomodoro timer |
| --- | --- |
| <img src="docs/images/desktop-pet/home.png" alt="Rabbit pet home showing growth, love, fullness and energy" width="330"> | <img src="docs/images/pomodoro/setup.png" alt="Sticky Pomodoro Timer setup screen" width="330"> |
| Hatch, name, feed, pet, talk to and play with a rabbit whose stage, personality and dialogue change over time. | Pick 15, 25 or 60 minutes, enter a custom duration, pause or end a session, and receive a gentle repeating alarm. |

| Status board | Book of Answers |
| --- | --- |
| <img src="docs/images/status-board/menu.png" alt="Sticky Status Board menu" width="390"> | <img src="docs/images/book-of-answers/home.png" alt="Book of Answers home screen" width="245"> |
| Display `BUSY`, `MEETING`, `ON CALL`, `OPEN TO TALK`, `REST`, or a custom on-device message in landscape. | Hold a question in mind and shake for three seconds to reveal a message, `YES`, `NO`, or `UNCLEAR`. |

## The virtual pet

The pet is the home screen and the emotional center of the firmware. Its state is advanced by real RTC time and stored in two checksummed NVS slots so a damaged save can fall back to the previous valid record.

<p align="center">
  <img src="docs/images/desktop-pet/growth-lineage.png" alt="Rabbit growth lineage from egg to adult personalities" width="640">
</p>

### Life and personality

1. **Egg** — three deliberate taps hatch the pet.
2. **Hatchling** — care establishes the first bond.
3. **Child** — feeding, affection and play begin shaping personality evidence.
4. **Youth** — the rabbit becomes `FOODIE`, `AFFECTIONATE`, or `ACTIVE`.
5. **Adult** — each branch receives its own proportions, actions, dialogue and keepsake.

Growth, love, fullness, energy, daily care streaks, mood, recent actions, recent dialogue, personality evidence and scheduled outings persist across restarts. Detailed rules live in the [pet system guide](docs/wiki/Pet-Growth-System.md).

## Launcher and physical interaction

<p align="center">
  <img src="docs/images/launcher/launcher-landscape.png" alt="Landscape launcher" width="700">
</p>

- Tap the **AI key** or swipe up from the lower screen to open the launcher.
- Tap an app card to launch it.
- Swipe down from the upper half to close the launcher.
- Double-tap the AI key from any page to return to the pet.
- Rotate from landscape to portrait while the launcher is open to enter Pomodoro.
- Rotate from portrait to landscape while the launcher is open to enter the Status Board.
- Shake while the launcher is open to enter the Book of Answers.
- Hold both non-AI side keys to enter deep sleep.

Orientation is accepted only after motion settles and the final placement is stable. Shake routing has priority once a qualified shake session starts. See [Launcher and Gestures](docs/wiki/App-Launcher-and-Gestures.md) for thresholds and state transitions.

## ePaper, power and time

The display keeps its last image without power, so the firmware treats every refresh as a limited resource. Static pages receive a clean baseline; countdown digits and animation regions use smaller refresh policies; periodic full refreshes restore contrast.

The pet schedules its next meaningful event before sleep. The PCF8563 RTC can wake the ESP32-S3 shortly before an outing or another autonomous event instead of waking at a fixed interval. Battery percentage comes from the BQ27220 fuel gauge, and the UI keeps charging and sleep indicators visible without covering app content.

Read [Power and RTC](docs/wiki/Power-and-RTC.md) and [Hardware and Drivers](docs/wiki/Hardware-and-Drivers.md) for the complete lifecycle.

## Hardware target

| Component | Firmware use |
| --- | --- |
| ESP32-S3R8 | Application, graphics, input routing and low-power control |
| 800 × 480 monochrome ePaper | Portrait and landscape application UI |
| GT911 | Capacitive touch and swipe input |
| LSM6DS3TR-C | Stable orientation, motion sessions and continuous shake detection |
| PCF8563 | Calendar time and scheduled wake-up |
| BQ27220 | Battery state of charge |
| Buzzer | Context-aware pet sounds and Pomodoro alarm |
| MicroSD slot | Shared SPI hardware path prepared safely at boot |

The implementation follows the board initialization and driver behavior demonstrated by Seeed Studio's Sticky hardware examples.

## Quick start

### Requirements

- reTerminal Sticky
- USB-C data cable
- PlatformIO Core 6.1 or PlatformIO IDE
- Python 3
- macOS, Linux, or Windows

The project pins `espressif32@6.11.0` and builds against ESP-IDF 5.4.1.

### Clone and build

```bash
git clone https://github.com/limengdu/reTerminal_Sticky_Bunny.git
cd reTerminal_Sticky_Bunny
pio run
```

`sticky-release` is the default environment. A successful build creates:

```text
.pio/build/sticky-release/firmware.bin
```

### Upload the release firmware

```bash
pio run -e sticky-release -t upload
```

For development logs:

```bash
pio run -e sticky-debug -t upload
pio device monitor -e sticky-debug
```

The monitor runs at `115200` baud. See [Getting Started](docs/wiki/Getting-Started.md) for download-mode recovery, complete-image flashing, NVS reset and first-boot expectations.

## Build profiles

| Environment | Purpose | Runtime rules | Logging |
| --- | --- | --- | --- |
| `sticky-release` | Daily use and releases | Production time, limits and outing schedule | Essential warnings and lifecycle events |
| `sticky-debug` | Hardware and interaction diagnosis | Production gameplay rules | Detailed app, input and storage logs |
| `sticky-power-test` | Accelerated sleep/wake validation | Shortened power timing only | Power-focused diagnostics |

Build every profile before a release:

```bash
pio run -e sticky-release -e sticky-debug -e sticky-power-test
```

## Repository map

```text
src/
├── app/                 # App manager, launcher, routing and lifecycle
├── apps/                # Pet, Pomodoro, Status Board, Answers and onboarding
├── board/               # Power, charger, shared buses and pin configuration
├── devices/             # Battery, RTC and buzzer drivers
├── display/             # ePaper ownership and refresh operations
├── input/               # Buttons and GT911 touch queue
├── sensors/             # IMU orientation and shake sessions
└── ui/                  # Canvas, font, overlays and generated pixel assets

assets/                  # Original art, firmware-ready images and QA renders
docs/wiki/               # User and developer documentation
test/                    # Native state, policy and rendering tests
tools/                   # Deterministic asset and database generators
third_party/             # License notices for adapted open-source ideas
```

The [firmware architecture guide](docs/wiki/Firmware-Architecture.md) follows execution from `app_main()` through hardware initialization, onboarding, app ownership, display refresh and deep sleep.

## Tests and visual QA

The native tests exercise state machines and render pages into PPM files without requiring the device. Coverage includes:

- pet progression, offline time, dialogue, animation scheduling and dual-slot saves;
- Pomodoro input, countdown policy, page layout and alarm states;
- status selection, custom text and status-specific animation;
- answer selection, three-second shake qualification and result layouts;
- launcher touch zones, swipe gestures, orientation routing and power policy;
- onboarding navigation and all six final pages.

```bash
./tools/run_host_tests.sh
python3 tools/check_markdown_links.py
```

The complete command matrix and expected preview files are documented in [Testing and Debugging](docs/wiki/Testing-and-Debugging.md). Hardware release checks are in [Flashing and Releases](docs/wiki/Flashing-and-Releases.md).

## Documentation

| Guide | What it explains |
| --- | --- |
| [Documentation home](docs/wiki/Home.md) | Find the right user or developer guide |
| [Getting Started](docs/wiki/Getting-Started.md) | Build, upload and complete first boot |
| [Desktop Pet](docs/wiki/Desktop-Pet.md) | Daily interactions and visible behavior |
| [Pet Growth System](docs/wiki/Pet-Growth-System.md) | Stages, personality, values and persistence |
| [Pomodoro Timer](docs/wiki/Pomodoro-Timer.md) | Presets, custom time, countdown and alarm |
| [Status Board](docs/wiki/Status-Board.md) | Preset and custom landscape status pages |
| [Book of Answers](docs/wiki/Book-of-Answers.md) | Message and crystal-ball answer modes |
| [Launcher and Gestures](docs/wiki/App-Launcher-and-Gestures.md) | Touch, button, rotation and shake routing |
| [Power and RTC](docs/wiki/Power-and-RTC.md) | Sleep policy, scheduled events and battery UI |
| [Firmware Architecture](docs/wiki/Firmware-Architecture.md) | Modules, ownership and execution flow |
| [Asset Pipeline](docs/wiki/Asset-Pipeline.md) | Artwork sources and deterministic conversion |
| [Testing and Debugging](docs/wiki/Testing-and-Debugging.md) | Native tests, logs and visual QA |
| [Troubleshooting](docs/wiki/Troubleshooting.md) | Common build, flash, display, touch and RTC checks |

## Design history

The firmware grew through physical-device testing and many ePaper-specific UI iterations. The Wiki keeps selected concept sheets beside final code-rendered screens so future contributors can understand the visual system without mistaking experiments for shipped behavior. Visit [Design and Asset Gallery](docs/wiki/Design-and-Asset-Gallery.md).

## Contributing

Issues and pull requests are welcome. Start with [CONTRIBUTING.md](CONTRIBUTING.md), keep hardware behavior traceable to the Sticky reference implementation, add a native regression test for behavior changes, and include a device validation plan when hardware is required.

## Credits and licenses

Sticky Bunny is released under the [MIT License](LICENSE).

The pet architecture adapts small MIT-licensed ideas from TamaPoke, esp32-artoria-tamagotchi, openclaw-tamagotchi and ESP32-TamaPetchi. The original notices and exact audited commits are recorded in [third_party/virtual_pet/NOTICE.md](third_party/virtual_pet/NOTICE.md). Rabbit artwork in this repository is original to this project.

The Book of Answers message database is derived from `DBinK/The-Book-of-Answers-Interpreter` under the Apache License 2.0; its complete notice is stored with the source database.

reTerminal Sticky is a product of Seeed Studio. This is a community firmware project and is not presented as the device's factory firmware.
