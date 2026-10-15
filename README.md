# Pregnancy Journal（孕期手帐）

Pregnancy Journal is an offline pregnancy organizer for the Seeed Studio
reTerminal Sticky. The current firmware combines a Baby Week dashboard,
weight records, kick-count sessions and Settings on the existing
Sticky Core hardware framework. It is informational record-keeping software,
not a diagnostic, treatment or emergency-alert device.

## Delivery plan

The 2026-10-10 scope decision targets a first delivery with four apps: Weight,
Kicks, Settings and Baby Week. Reminders, Checkups and business alerts, Habits,
Notes, recording/classification, provisioning and speech-to-text move to the
next delivery. The MicroSD data requirement remains in force, including a new
Kicks repository and validated legacy migration. See the
[current specification](docs/PROJECT-SPEC.md), [active tasks](TODO.md) and
[V2 extension specification](docs/V2-PROJECT-SPEC.md).

The four-app implementation is now available as an unaccepted firmware candidate.
Automatic validation and the remaining physical gate are recorded in
[V1 validation](docs/V1-VALIDATION.md).

## Current product

- Target: ESP32-S3 reTerminal Sticky with an 800×480 monochrome e-paper panel.
- Firmware version: `2.0.0`.
- Four registered apps: Baby Week, Weight, Kicks and Settings.
- English and Simplified Chinese UI; device preferences are stored in the
  selected MicroSD dataset.
- Pregnancy, weight, Kicks and preferences use the MicroSD repository.
  Legacy records have explicit verified migration; reminder/checkup copies are
  preserved for V2. There is no active cloud upload or transcription path.

For the complete behavior and limits of the current image, see
[Current functionality reference](docs/pregnancy/current-functionality.md).

## Architecture

```text
Apps -> domain/content services -> Sticky Core runtime -> device HAL
```

`app_registry.cpp` is the composition root. `StickyAppManager` owns app
lifecycle and display/input ownership; the framework coordinator owns the
launcher, app switching, scheduling and deep sleep. Fixed-capacity services and
the journaled MicroSD repository keep memory and persistence predictable. See
[architecture](docs/pregnancy/architecture.md).

## Build

PlatformIO with ESP-IDF 5.4.1 is required. The workspace path contains spaces,
which PlatformIO rejects, so build from a no-space checkout or an exact mirror:

```bash
pio run -e sticky-release
pio run -e sticky-debug
pio run -e sticky-power-test
tools/run_host_tests.sh
```

The target is `sticky_esp32s3` with the custom `partitions.csv` layout, which
includes a 128 KiB `coredump` partition (read a crash back with
`idf.py coredump-info`). `sticky-release` additionally applies
`sdkconfig.release.defaults` (-O2, silent assertions, INFO log ceiling). The
current release gate is 76/76 host tests plus successful release, debug and
power-test builds; a physical-device pass is tracked separately.

## Flash

```bash
pio device list
pio run -e sticky-release -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --filter time --rts 0 --dtr 0
```

Replace the port with the device reported by `pio device list`. If automatic
bootloader entry fails, hold **BOOT**, tap **RESET**, release **BOOT**, and run
the upload command again. After flashing, run the
[hardware regression checklist](docs/pregnancy/hardware-regression-checklist.md).

## First setup

On first launch, Baby Week asks for device time (`YYYYMMDDHHMM`) and then a
clinician-confirmed estimated due date or the first day of the last menstrual
period (`YYYYMMDD`). The profile is saved only on the device and can be changed
later from Settings → Pregnancy Settings.

## Navigation

- Top-button single click toggles Launcher.
- Swipe up from the bottom edge opens Launcher; swipe down from the lower 30%
  closes it.
- Swipe left or right along the bottom edge (a long, flat swipe that starts in
  the bottom band, Launcher closed) switches straight to the next or previous
  app in Launcher order, wrapping around, without opening Launcher.
- Double-click the top button returns to Baby Week.
- Tap a launcher entry to open Baby Week, Weight, Kicks or Settings.
- Settings changes English / 简体中文.

## Apps at a glance

| App | What it does | Main limits |
| --- | --- | --- |
| Baby Week | Week/day, trimester, due-date countdown, 40-week progress, Baby and Mom offline content | Informational content is clamped to weeks 1–40 |
| Weight | Daily kg/lb record, baseline change, 7/30 calendar-day trend, history editing/deletion | 366 records; one per date; replacement/deletion confirmation |
| Kicks | 1-hour count, Count to 10 and Feel check-in; each record compared with the median of the user's own previous seven days (direction and size of the difference only) | 400 records; undo plus 400 ms accidental-double-tap guard; active session ends at 60 minutes |
| Settings | Language, RTC time, pregnancy setup, clean refresh and MicroSD status/confirmation | Time editor keeps the device awake; card initialization requires two explicit actions |

All health-related values are prompts for personal records and care-team
discussion, never medical recommendations.

## Sleep and wake

Stable pages enter deep sleep after 60 seconds of inactivity when external power
is absent. Settings uses a three-minute timeout; editors and an active kick
session keep the device awake. A two-second chord on both side buttons requests
sleep. The coordinator schedules daily 03:00 maintenance and an active-app
request, with the timer up to 15 seconds early. V1 has no business-alert timer
wake or buzzer path.

## Storage and privacy

All four apps use a dataset-bound, journaled MicroSD repository with no Flash
fallback. Settings offers verified legacy import (including an independent
Kicks path), safe eject, device reset and card-application-data deletion.
Reminder/checkup NVS copies remain intact. The recovery index holds 1,024 keys
including tombstones; journal/object garbage collection is not implemented.
See [storage](docs/pregnancy/storage.md), [data model](docs/pregnancy/data-model.md)
and [V1 validation](docs/V1-VALIDATION.md).

## Documentation map

- [First-delivery specification (four offline apps)](docs/PROJECT-SPEC.md)
- [V2 extension specification (reminders and speech-to-text)](docs/V2-PROJECT-SPEC.md)
- [Development design and task plan](TODO.md)
- [Contributor and agent guide](AGENTS.md)
- [E-paper product design system](DESIGN.md)
- [Architecture entry point](docs/ARCHITECTURE.md)
- [Component guidelines](docs/COMPONENT-GUIDELINES.md)
- [Page and navigation structure](docs/PAGE-STRUCTURE.md)
- [Development workflow](docs/DEVELOPMENT.md)
- [reTerminal Sticky hardware reference](docs/HARDWARE-REFERENCE.md)
- [Current functionality reference](docs/pregnancy/current-functionality.md)
- [UI prototype implementation mapping](docs/pregnancy/ui-prototype-mapping.md)
- [Architecture](docs/pregnancy/architecture.md)
- [Data model](docs/pregnancy/data-model.md)
- [Pregnancy calculation](docs/pregnancy/pregnancy-calculation.md)
- [Reminder and scheduler](docs/pregnancy/reminder-scheduler.md)
- [E-paper refresh strategy](docs/pregnancy/eink-refresh-strategy.md)
- [Storage](docs/pregnancy/storage.md)
- [Hardware regression checklist](docs/pregnancy/hardware-regression-checklist.md)
- [Future roadmap](docs/pregnancy/future-roadmap.md)

See [CONTRIBUTING.md](CONTRIBUTING.md) for development conventions and
[docs/refactor/adding-an-app.md](docs/refactor/adding-an-app.md) for extending
the app registry. See [LICENSE](LICENSE) and the notices under
`third_party/fonts/NotoSansCJK` for upstream attribution.
