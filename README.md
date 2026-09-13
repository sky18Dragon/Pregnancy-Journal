# Pregnancy Journal（孕期手帐）

Pregnancy Journal is an offline pregnancy organizer for the Seeed Studio
reTerminal Sticky. It combines a calm e-paper dashboard, pregnancy week
information, reminders, appointments, weight trends and kick-count sessions
while keeping the
hardware-tested Sticky Core framework intact. It is an informational organizer,
not a diagnostic or treatment device.

## Features

- Due-date or last-menstrual-period setup with persisted pregnancy profile.
- Baby Week is the default home, with week/day, due-date countdown and progress.
- Overview, Baby and Mom pages are backed by bundled offline week content.
- Today/upcoming reminders with simple creation, completion, deletion and
  daily/weekly recurrence support in the domain layer.
- Checkup organizer for appointments and completion status.
- Daily kg/lb weight records with BMI, baseline gain and weekly trend checks.
- Morning/afternoon/evening kick-count sessions with 12-hour estimates and
  change detection.
- A six-app, high-contrast launcher designed for the 800x480 e-paper panel.
- English and Simplified Chinese UI.
- Typed reminder/checkup/daily-refresh scheduler, deep sleep and button/timer
  wake.

## Architecture

```text
Pregnancy apps -> domain/content services -> Sticky Core -> device HAL
```

The registry composes six isolated apps: Baby Week, Reminder, Checkup, Weight,
Kicks and Settings. `StickyAppManager` enforces lifecycle and input/
display ownership. The framework coordinator owns Launcher, scheduling and
deep sleep. See [architecture.md](docs/pregnancy/architecture.md).

## Build

PlatformIO with ESP-IDF 5.4.1 is required. PlatformIO rejects paths containing
spaces, so run from a no-space checkout or exact mirror:

```bash
pio run -e sticky-release
pio run -e sticky-debug
pio run -e sticky-power-test
tools/run_host_tests.sh
```

The target is `sticky_esp32s3` with the custom `partitions.csv` layout.

## Flash

```bash
pio run -e sticky-release -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --filter time --rts 0 --dtr 0
```

Replace the port with the device shown by `pio device list`. Run the physical
[hardware regression checklist](docs/pregnancy/hardware-regression-checklist.md)
after flashing.

## First setup

On first use, set the RTC if requested, then choose either a clinician-confirmed
estimated due date or the first day of the last menstrual period. The profile is
saved only on the device and can be changed later from Settings → Pregnancy
Settings.

## Launcher and navigation

- Swipe up from the bottom or single-click the top button: open Launcher.
- Swipe down or single-click again: close Launcher.
- Double-click the top button: return to Baby Week.
- Tap a Launcher card to open Baby Week, Reminder, Checkup, Weight or Kicks;
  Settings has a dedicated control in the Launcher header.

## Apps

Pregnancy displays Overview, Baby and Mom tabs for the current calculated week.
Its bundled content is broad, non-diagnostic guidance and remains available
without a network.

Reminder lists today and upcoming items. Add a simple typed reminder, mark it
complete or delete it. The earliest future item participates in system wake
scheduling.

Checkup stores appointment dates and completion status. Dates should always be
confirmed with the user's care team.

## Sleep and wake

Stable pages enter deep sleep after 60 seconds of inactivity when external power
is absent; date/input editors keep the device awake. The top button remains a wake
source. Before sleep, one coordinator chooses the earliest reminder, checkup,
app request or daily 03:00 refresh and arms one timer wake. A due reminder or
checkup starts the buzzer after timer wake; any touch/button interaction stops it.

## Storage and privacy

Pregnancy profile, reminder and checkup records use versioned checksummed NVS
storage. Corrupt records are skipped or recovered from a valid fallback. There
is no account, telemetry, cloud log, network dependency or AI service. See
[storage.md](docs/pregnancy/storage.md).

## Testing

`tools/run_host_tests.sh` compiles business logic and renderer/interaction tests
with C++17, warnings as errors. The firmware gate builds release, debug and
power-test profiles. Hardware results must be recorded separately; a successful
host/build gate does not claim a physical-device pass.

## Documentation

- [Baseline and phase plan](docs/pregnancy/baseline.md)
- [Data model](docs/pregnancy/data-model.md)
- [Pregnancy calculation](docs/pregnancy/pregnancy-calculation.md)
- [Reminder scheduler](docs/pregnancy/reminder-scheduler.md)
- [E-ink refresh strategy](docs/pregnancy/eink-refresh-strategy.md)
- [Future roadmap](docs/pregnancy/future-roadmap.md)

The project retains Seeed Studio board integration and upstream attribution.
See [LICENSE](LICENSE) and the notices under `third_party/fonts/NotoSansCJK`.
