# Pregnancy Journal（孕期手帐）

Pregnancy Journal is an offline pregnancy organizer for the Seeed Studio
reTerminal Sticky. The current firmware combines a Baby Week dashboard,
reminders, checkups, weight records and kick-count sessions on the existing
Sticky Core hardware framework. It is informational record-keeping software,
not a diagnostic, treatment or emergency-alert device.

## Current product

- Target: ESP32-S3 reTerminal Sticky with an 800×480 monochrome e-paper panel.
- Firmware version: `2.0.0`.
- Six registered apps: Baby Week, Reminders, Checkups, Weight, Kicks and
  Settings.
- English and Simplified Chinese UI, stored locally in NVS.
- Pregnancy, reminder, checkup, weight and kick data stay on the device. There
  is no account, network dependency, telemetry, cloud log, OTA or AI service.

For the complete behavior and limits of the current image, see
[Current functionality reference](docs/pregnancy/current-functionality.md).

## Architecture

```text
Apps -> domain/content services -> Sticky Core runtime -> device HAL
```

`app_registry.cpp` is the composition root. `StickyAppManager` owns app
lifecycle and display/input ownership; the framework coordinator owns the
launcher, app switching, scheduling and deep sleep. Fixed-capacity services and
checksummed NVS records keep memory and persistence behavior predictable. See
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

The target is `sticky_esp32s3` with the custom `partitions.csv` layout. The
current release gate is 20/20 host tests plus a successful release build; a
physical-device pass is tracked separately.

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
- Double-click the top button returns to Baby Week.
- Tap a launcher card to open Baby Week, Checkups, Reminders, Weight or Kicks;
  Settings is the header action.
- The launcher language control toggles English and 简体中文.

## Apps at a glance

| App | What it does | Main limits |
| --- | --- | --- |
| Baby Week | Week/day, trimester, due-date countdown, 40-week progress, Baby and Mom offline content | Informational content is clamped to weeks 1–40 |
| Reminders | Today/Upcoming list, six types, complete/delete, global enable switch | 12 records; four visible rows; recurrence is daily/weekly in the domain model |
| Checkups | Add, complete and delete dated appointments | 8 records; four visible rows; new item defaults to +7 days at 09:00 |
| Weight | Daily kg/lb record, BMI, baseline gain and seven-point trend | 32 records; one record per day; height 100–220 cm |
| Kicks | Morning/afternoon/evening sessions, sampled total and 12-hour estimate | 42 sessions; five-second debounce; active session ends at 60 minutes |
| Settings | Language, RTC time, pregnancy setup, clean refresh and return home | Time editor keeps the device awake |

All health-related values are prompts for personal records and care-team
discussion, never medical recommendations.

## Sleep and wake

Stable pages enter deep sleep after 60 seconds of inactivity when external power
is absent. Settings uses a three-minute timeout; editors and an active kick
session keep the device awake. A two-second chord on both side buttons requests
sleep. The coordinator selects the closest of the daily 03:00 refresh, an app
request, the next enabled reminder and the next incomplete checkup, then arms a
timer up to 15 seconds early. A due reminder/checkup starts the buzzer on wake;
any touch or button stops it.

## Storage and privacy

Device settings use two alternating versioned/checksummed NVS slots. Pregnancy
profile uses a primary and backup record with migration from the legacy due-date
format. Reminders, checkups, weight records and kick sessions use bounded,
versioned records in independent NVS namespaces; corrupt slots are skipped while
valid siblings remain available. See [storage](docs/pregnancy/storage.md) and
[data model](docs/pregnancy/data-model.md).

## Documentation map

- [Current functionality reference](docs/pregnancy/current-functionality.md)
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
