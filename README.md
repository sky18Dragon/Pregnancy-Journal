# Pregnancy Journal（孕期手帐）

**English** | [简体中文](docs/README_CN.md) | [日本語](docs/README_JA.md)

Pregnancy Journal is an offline pregnancy organizer for the Seeed Studio
reTerminal Sticky, a small magnetic e-paper device. The firmware combines a
Baby Week dashboard, weight records, kick-count sessions and Settings on the
Sticky Core hardware framework. It is informational record-keeping software,
not a diagnostic, treatment or emergency-alert device: every health-related
value is a prompt for personal records and care-team discussion, never a medical
recommendation.

## Status

- Target: ESP32-S3 reTerminal Sticky with a 3.97-inch 800×480 monochrome
  e-paper panel. Firmware version `2.0.0`.
- Four registered apps: **Baby Week**, **Weight**, **Kicks** and **Settings**.
  There are no reminders, checkups, business alerts, habits, notes, recording,
  provisioning or speech-to-text.
- English and Simplified Chinese UI. All user data lives on the MicroSD card;
  there is no account, telemetry, cloud upload, OTA or AI service.
- This is an unaccepted firmware candidate. The automatic gate is the host
  suite (`tools/run_host_tests.sh`, 77/77 steps on 2026-10-19) plus successful
  `sticky-release`, `sticky-debug` and `sticky-power-test` builds. Physical
  acceptance is tracked separately in [V1 validation](docs/V1-VALIDATION.md) and
  the [hardware regression checklist](docs/pregnancy/hardware-regression-checklist.md);
  nothing here claims a device pass.

For the complete behavior and limits of the current image, see the
[current functionality reference](docs/pregnancy/current-functionality.md).

## Apps

| App | What it does | Main limits |
| --- | --- | --- |
| Baby Week | Dashboard with an engraving-style plate for each of weeks 6–40, a week grid and the current week; Baby and Mom pages with week/day, trimester, days until the due date and offline guidance | Content is clamped to weeks 1–40 |
| Weight | kg/lb records at 0.1 precision, baseline and height, 7- or 30-day chart, history editing and deletion | 366 records, one per date; replacing or deleting needs confirmation |
| Kicks | Three record types: 1-hour count, Count to 10 and Feel check-in; the chosen type is remembered; each record is compared with the median of the user's own previous seven days (direction and size of the difference only) | 400 records; undo; 400 ms double-tap guard; a session ends at 60 minutes |
| Settings | Language, device time, pregnancy settings, double-click shortcut, e-paper clean refresh and the MicroSD page (status, dataset creation, safe eject, legacy import, card data deletion) | The time editor keeps the device awake; creating a dataset needs two explicit actions |

On the dashboard, tapping the illustration opens the Baby page and tapping the
week number opens the Mom page. The dashboard no longer shows a progress bar or a
countdown; those figures are on the Baby and Mom pages.

## First setup

On first launch, Baby Week asks for the device time (`YYYYMMDDHHMM`) and then a
clinician-confirmed estimated due date or the first day of the last menstrual
period (`YYYYMMDD`). The profile is saved only on the MicroSD card and can be
changed later from Settings → Pregnancy Settings. Without a card the app shows
why it cannot load instead of falling back to Flash.

## Controls

| Input | Result |
| --- | --- |
| Top button, single click | Any page → Baby Week home → Launcher → close (back to where you were); on the Kicks counter page it counts one movement |
| Top button, double click | Runs the shortcut chosen in Settings → Shortcut: record today's weight (default), Baby Week home, start Kicks, or off |
| Side buttons, single press | Previous / next app in Launcher order; pages that hold unsaved input or a running count ignore them |
| Both side buttons, 2 s | Request deep sleep |
| Swipe up from the bottom edge | Open Launcher |
| Swipe down from the lower 30% | Close Launcher |
| Long flat swipe left / right along the bottom edge (Launcher closed) | Previous / next app, wrapping, without opening Launcher |
| Tap a Launcher entry | Open Baby Week, Weight, Kicks or Settings |

An idle Launcher closes after 30 seconds. The layout follows the grip
orientation reported by the IMU: Launcher is two columns in landscape and one in
portrait, and every app has dedicated landscape and portrait pages. Header
buttons mean **APPS** (leave the app for Launcher), **BACK** (one level up),
**CANCEL** (abandon without saving) and **HOME** (Baby Week); see
[page and navigation structure](docs/PAGE-STRUCTURE.md).

## Sleep and wake

Stable pages enter deep sleep after 60 seconds of inactivity when external power
is absent. Settings uses three minutes; editors and an active kick session keep
the device awake. The top button wakes the device. Before sleeping the
coordinator schedules a daily 03:00 maintenance wake and any request from the
active app, with the timer armed up to 15 seconds early. This version has no
business-alert timer wake and no alarm sound.

## E-paper refresh

Product pages use black and white only and are sent to the panel as 1-bit
frames (about 1.9 s for a full refresh, about 0.9 s for a partial one). Page
changes use a full refresh, with these exceptions:

- Opening Launcher, returning to the app and switching apps draw their first frame
  with the fast (partial) waveform; every sixth such transition is a full refresh.
- Switching between the Baby Week home and the Baby page is a partial refresh; one
  full refresh repaints the page after 5 seconds without activity.
- The display layer forces a full refresh after 10 consecutive partial refreshes.
- Entering Weight or Kicks draws the content page directly when the card answers
  within 1.5 s (a loading page appears only for a slower read), and entering Baby
  Week with a profile it already holds draws the home page at once and re-checks
  the card quietly.

Rotation changes and the sleep frame always use a full refresh. The rules are in
[docs/DESIGN.md](docs/DESIGN.md) and the
[refresh strategy](docs/pregnancy/eink-refresh-strategy.md).

## Storage and privacy

All four apps use a dataset-bound, journaled MicroSD repository with no Flash
fallback. Settings offers verified legacy import (Kicks has an independent
path), safe eject and card-application-data deletion. The recovery index holds 1,024 keys
including deleted keys; journal and object garbage collection is not implemented,
and a full index rejects new keys instead of dropping records. See
[storage](docs/pregnancy/storage.md), [data model](docs/pregnancy/data-model.md)
and [backup and recovery](docs/S1-BACKUP-RECOVERY.md).

## Repository layout

| Path | Contents |
| --- | --- |
| `src/app/` | App registry, coordinator (Launcher, app switching, key routing, sleep) |
| `src/apps/` | Baby Week (`pregnancy`), `weight`, `kick`, `settings` |
| `src/pregnancy/` | Domain services, models and offline content |
| `src/ui/` | Canvas, fonts, tokens, shared pages, generated assets |
| `src/storage/` | MicroSD service, journal, index and repository code |
| `src/display/`, `src/board/`, `src/devices/`, `src/input/`, `src/sensors/` | Display owner and hardware access |
| `components/` | GT911 touch and SSD1677 e-paper drivers |
| `test/`, `tools/` | Host tests and tooling (below) |
| `docs/` | Specifications, design notes, validation records and evidence |

The architecture is `Apps → domain/content services → Sticky Core runtime →
device HAL`. `app_registry.cpp` is the composition root; `StickyAppManager` owns
app lifecycle and display/input ownership. See
[architecture](docs/ARCHITECTURE.md) and
[adding an app](docs/refactor/adding-an-app.md).

## Build and test

PlatformIO with ESP-IDF 5.4.1 is required. The workspace path contains spaces,
which PlatformIO rejects, so build from a no-space checkout or an exact mirror
(see the [development workflow](docs/DEVELOPMENT.md)):

```bash
pio run -e sticky-release
pio run -e sticky-debug
pio run -e sticky-power-test
tools/run_host_tests.sh
python3 tools/check_markdown_links.py
```

The target is `sticky_esp32s3` with the custom `partitions.csv` layout, which
includes a 128 KiB `coredump` partition (read a crash back with
`idf.py coredump-info`). `sticky-release` additionally applies
`sdkconfig.release.defaults` (-O2, silent assertions, INFO log ceiling). Other
environments in `platformio.ini` are isolated hardware tests (glyph coverage,
storage power-loss and fault injection); they are not product builds.

Useful tools: `tools/ui_preview/render_all.sh` renders every page as a 1-bit PNG
with hit-area overlays and lint output; `tools/check_ui_tokens.py` enforces the
type-size and margin tokens; `tools/validate_sd_backup.py` checks a copied
`PREGJNL` dataset.

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

## Documentation map

**Product and status**
- [Current functionality reference](docs/pregnancy/current-functionality.md)
- [First-delivery specification (four offline apps)](docs/PROJECT-SPEC.md)
- [Active tasks](docs/TODO.md) and [changelog](docs/CHANGELOG.md)
- Other languages: [简体中文](docs/README_CN.md), [日本語](docs/README_JA.md)
- [V1 validation](docs/V1-VALIDATION.md), [hardware results](docs/V1-HARDWARE-RESULTS.md),
  [manual test checklist](docs/V1-MANUAL-TEST-CHECKLIST.md) and
  [hardware regression checklist](docs/pregnancy/hardware-regression-checklist.md)

**Architecture and development**
- [Architecture entry point](docs/ARCHITECTURE.md) and [pregnancy architecture](docs/pregnancy/architecture.md)
- [Development workflow](docs/DEVELOPMENT.md), [component guidelines](docs/COMPONENT-GUIDELINES.md)
  and [contributing](docs/CONTRIBUTING.md)
- [Contributor and agent guide](docs/AGENTS.md); the e-paper UI rules that every
  UI change must follow are in [docs/CLAUDE.md](docs/CLAUDE.md)
- [Security policy](docs/SECURITY.md) and [code of conduct](docs/CODE_OF_CONDUCT.md)
- [reTerminal Sticky hardware reference](docs/HARDWARE-REFERENCE.md)

**UI and interaction**
- [E-paper product design system](docs/DESIGN.md)
- [Page and navigation structure](docs/PAGE-STRUCTURE.md)
- [Key and shortcut design](docs/pregnancy/key-shortcuts-design.md)
- [UI prototype implementation mapping](docs/pregnancy/ui-prototype-mapping.md)
- [E-paper refresh strategy](docs/pregnancy/eink-refresh-strategy.md) and [UI audit](docs/ui-audit.md)

**Data and storage**
- [Storage](docs/pregnancy/storage.md) and [data model](docs/pregnancy/data-model.md)
- [Pregnancy calculation](docs/pregnancy/pregnancy-calculation.md)
- [Journal format](docs/S1-JOURNAL-FORMAT.md), [index snapshot](docs/S1-INDEX-SNAPSHOT.md)
  and [backup and recovery](docs/S1-BACKUP-RECOVERY.md)

See [LICENSE](LICENSE) and the notices under `third_party/fonts/NotoSansCJK` for
upstream attribution. The licence of the week illustrations is recorded in
[assets/pregnancy/week-plates/README.md](assets/pregnancy/week-plates/README.md).

## Acknowledgements

This project started from [limengdu/reTerminal_Sticky_Bunny](https://github.com/limengdu/reTerminal_Sticky_Bunny)
(MIT) and still uses its hardware foundation: the SSD1677/UC8179 e-paper and GT911
touch components, board power and shared-SPI setup, the display owner, the
IMU, RTC, battery and buzzer drivers, button/touch input and the Canvas drawing
layer, and the startup sequence. The app framework, the four apps, the MicroSD
journaled storage and the tests were written for this project on top of that
base.
