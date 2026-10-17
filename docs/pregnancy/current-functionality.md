# Current functionality reference

This page is the user-facing source of truth for the firmware currently in the
repository. It describes the implementation, not planned product ideas. The
firmware is an offline organizer for the Seeed Studio reTerminal Sticky; it is
not a diagnostic, treatment or emergency-alert device.

## Product shape

- Target: Seeed Studio reTerminal Sticky, ESP32-S3, 800×480 monochrome
  e-paper panel.
- Firmware version: `2.0.0` (from `platformio.ini`).
- Default app: Baby Week (the Pregnancy app).
- Registered apps: Baby Week, Weight, Kicks and Settings.
- Languages: English and Simplified Chinese. The language setting is stored in
  the selected MicroSD dataset and is changed from Settings.
- Data boundary: no account, telemetry, cloud log, OTA or active AI service.
  S2 includes a device-bound network-authorization storage contract but no
  cloud submission path. Product records remain local.

## Launcher and navigation

The launcher follows the settled grip orientation reported by the IMU. It uses
a two-column list in 800×480 landscape and a single-column list in 480×800
portrait. Its four numbered entries are Baby Week, Weight, Kicks and Settings.

The same grip-following policy applies inside every registered app.
Weight, Kicks and Settings keep their existing two-dimensional
landscape controls and switch to dedicated portrait rows, charts, keypads and
bottom actions rather than cropping or rotating a fixed screenshot. Touch
targets reflow with the visual controls.

- Top-button single click toggles the launcher.
- Swipe up beginning in the bottom edge opens it; swipe down beginning in the
  lower 30% closes it.
- Swipes must be predominantly vertical, travel at least 96 px (or 20% of the
  panel height), and finish within 60–1600 ms.
- A launcher with no activity closes after 30 seconds.
- Double-clicking the top button always returns to Baby Week.
- The on-screen Close control returns to the previously active app.

## App behavior

### Baby Week (Pregnancy)

First use is a two-part setup: enter device time, then choose a confirmed
estimated due date or the first day of the last menstrual period. Dates are
entered as `YYYYMMDD`; clock input is `YYYYMMDDHHMM`. The profile is saved in
the selected MicroSD repository and can be edited later.

The home page follows the settled grip orientation and reflows rather than
cropping: landscape uses a two-column dashboard, while portrait uses a vertical
week/progress, illustration and growth-card sequence. Setup and Baby/Mom detail
pages have matching portrait layouts. Weeks 6–40 each have a distinct
engraving-style plate. The 340×340 sources are replaceable under
`assets/pregnancy/week-plates/` and are compiled into 1-bit assets.
The Baby and Mom detail tabs show week/day, trimester, days until the due date,
offline guidance and three facts. Content is clamped to weeks 1–40.

### Weight

Weight uses 0.1 kg precision (30.0–200.0 kg input range), with kg/lb display
and editing. There is one record per date and a 366-record limit. Replacing an
existing date or deleting a selected record requires confirmation. History is
ordered newest first, five rows per page; select a row to edit its number while
preserving its date and note. Full capacity rejects additions without overwriting
history, while existing dates can still be updated.

The chart filters seven or thirty calendar days ending today. Missing days have
no point and are not plotted as zero. The latest value and numerical baseline
change remain visible. Height (100–220 cm) and baseline are editable; the first
saved weight initializes an unset baseline. There is no fixed BMI/gain threshold
or normal/abnormal assessment. Canonical storage remains metric even in lb mode.

### Kicks

The app supports short counting sessions throughout the day and
tags each saved session as morning, afternoon or evening from the RTC (before
12:00, 12:00–17:59, or 18:00 onward). Tapping the central target records one
distinct movement; a 400 ms guard filters accidental double taps, and Undo
removes the most recent tap. Finish saves a non-empty session. An active session
keeps the device awake and is forced to finish after 60 minutes. Capacity is 400
records. New sessions use the shared asynchronous MicroSD repository; the
saved totals change only after durable completion. A failed save retains the
draft for a stable-ID reload/retry, rejecting different-card or conflicting
records. Cancellation discards the active draft. Eight partial updates trigger
a full monochrome cleanup. The captured start date/time remains fixed across
midnight, and summary pages update for the new date.

The first page shows today's total, session count, period totals and latest
session. The second page shows seven calendar days as a bar chart, recorded-day
and session totals, the average per recorded day, the most-recorded time period,
and today's period breakdown. These values describe what the user recorded;
they are not a normal/abnormal score, diagnostic threshold or extrapolated
12-hour estimate. The Kicks pages show recorded data and comparisons only; they
carry no care-seeking instruction.

Start opens a record-type choice:

- **1-hour count** counts distinct movements for up to 60 minutes and converts
  the session to movements per hour (sessions under 15 minutes are not
  converted).
- **Count to 10** stops automatically at the tenth movement and stores the time
  taken. A session that runs 60 minutes without reaching ten is stored as
  "under 10 in 60 min"; one ended earlier without reaching ten is saved but not
  compared.
- **Feel check-in** records no count, only one of: as usual, weaker, fewer or
  pattern changed.

After a durable save, the Result page compares the record with the user's own
baseline: the median of the same record type in the seven calendar days before
it (1-hour counts use the same morning/afternoon/evening period). At least three
baseline records are required; until then the page shows how many exist. The
output states only the direction and size of the difference, for example
"7 per hour fewer than usual (-50%)" or "6:30 longer than usual (+57%)", with
no label, threshold or advice. Compare With Usual (from Insights or Result)
shows today's latest 1-hour count per period, the latest count-to-10 in seven
days and the seven-day feel check-in tally. Feel check-ins are excluded from
movement totals and the seven-day chart.

### Settings

Settings provides language, device-time editing, a Pregnancy Settings summary,
e-paper clean refresh and a MicroSD status summary; its header returns to Baby
Week. Pregnancy Settings shows the dating source, due date, current week and
40-week progress before entering the existing date editor. The MicroSD page
reports card access, capacity, dataset identity/generation and recovery state.
A writable,
uninitialized card exposes a second confirmation page that explains the fixed
`PREGJNL` dataset creation boundary before any request is submitted. Time input
is `YYYYMMDDHHMM`; invalid dates or RTC writes are rejected. The time editor
cannot time out to sleep; Settings' stable pages use a three-minute timeout.
Safe Eject drains accepted storage work and holds the card unmounted until it is
removed. Old Device Data provides verified, idempotent NVS-to-card import and a
separate cleanup confirmation. Choose profile/weight/settings or Kicks import;
Kicks scans all 42 historical slots independently. Both cleanup paths preserve
reminder/checkup NVS copies for V2. A card change invalidates cleanup eligibility
until another verified import. Tapping the dataset opens a confirmation that
deletes only `PREGJNL`; Device Reset opens a different confirmation that only
forgets the local network wrapping key and RAM Wi-Fi configuration.

## Sleep, wake and refresh

Stable pages normally enter deep sleep after 60 seconds without activity when
external power is absent. Settings keeps the device awake for three minutes;
date/time editors and an active kick-count session do not time out. The top
button is a wake source, and a two-second chord on both side buttons requests
sleep.

Before sleep, the coordinator schedules the daily 03:00 maintenance refresh
and any future event requested by the active app. The timer is armed up to 15
seconds early. Reminder/checkup caches, business wake events and timer-triggered
alarms are disabled in V1; their persisted app IDs remain unchanged and fall
back to Baby Week when selected from an older saved state.

Stable page entry uses a full refresh; mutations use partial refresh. After five
fast app transitions the display receives a cleanup full refresh. Settings also
offers an explicit cleanup action.

## Verification status

The four-app implementation is a candidate awaiting physical acceptance. See
[V1 validation](../V1-VALIDATION.md) for automatic results, firmware checksums
and the manual gate. Historical single-card debug results do not certify the
new release. Use [hardware checklist](hardware-regression-checklist.md).

## Known scope boundaries

Reminder/checkup apps, business alerts, habits, notes, recording, provisioning,
cloud submission and speech-to-text are deferred to V2. Their stored records
and cryptographic protections are preserved. Kicks has no per-session history
editor/deletion UI; its 400-record limit requires planning around a backed-up
card dataset. Weight history edits the existing date's value, not its date.
The shared index holds at most 1,024 distinct keys including deleted keys;
there is no tombstone or journal garbage collection in this candidate. Full
capacity rejects new keys and never silently drops records. User data has no
Flash fallback.
