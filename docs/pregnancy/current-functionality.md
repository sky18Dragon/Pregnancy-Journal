# Current functionality reference

This page is the user-facing source of truth for the firmware currently in the
repository. It describes the implementation, not planned product ideas. The
firmware is an offline organizer for the Seeed Studio reTerminal Sticky; it is
not a diagnostic, treatment or emergency-alert device.

## Product shape

- Target: Seeed Studio reTerminal Sticky, ESP32-S3, 800×480 monochrome
  e-paper panel.
- Firmware version: `2.0.1` (from `platformio.ini`).
- Default app: Baby Week (the Pregnancy app).
- Registered apps: Baby Week, Weight, Kicks and Settings.
- Languages: English and Simplified Chinese. The language setting is stored in
  the selected MicroSD dataset and is changed from Settings.
- Data boundary: no account, telemetry, cloud log, OTA, network access or AI
  service. Product records remain local.

## Launcher and navigation

The launcher follows the settled grip orientation reported by the IMU. It uses
a two-column list in 800×480 landscape and a single-column list in 480×800
portrait. Its four numbered entries are Baby Week, Weight, Kicks and Settings.

The same grip-following policy applies inside every registered app.
Weight, Kicks and Settings keep their existing two-dimensional
landscape controls and switch to dedicated portrait rows, charts, keypads and
bottom actions rather than cropping or rotating a fixed screenshot. Touch
targets reflow with the visual controls.

- Top-button single click: any page goes to the Baby Week home, the home opens
  the launcher, and the launcher closes back to the page you were on. On the
  Kicks counter page it counts one movement instead; the setup wizard ignores it.
- Top-button double click runs the shortcut chosen in Settings → Shortcut: record
  today's weight (the default), go to the Baby Week home, start Kicks, or off.
  Pages holding unsaved input or a running count are left alone.
- Side-button single press switches to the previous or next app in launcher
  order, wrapping; presses within 400 ms merge into one switch. Pages with
  unsaved input (editors, confirmations, a running count) and the setup wizard
  ignore it. The direction is mirrored only when the screen is rotated 180°.
- Swipe up beginning in the bottom edge opens the launcher; swipe down beginning
  in the lower 30% closes it. A long flat swipe left or right along the bottom
  edge, with the launcher closed, switches to the next or previous app without
  opening it.
- Swipes must be predominantly vertical, travel at least 96 px (or 20% of the
  panel height), and finish within 60–1600 ms.
- A launcher with no activity closes after 30 seconds.
- The on-screen Close control returns to the previously active app.

## App behavior

### Baby Week (Pregnancy)

First use is a two-part setup: enter device time, then choose a confirmed
estimated due date or the first day of the last menstrual period. Dates are
entered as `YYYYMMDD`; clock input is `YYYYMMDDHHMM`. The profile is saved in
the selected MicroSD repository and can be edited later.

The home page is a plate. It follows the settled grip orientation and reflows
rather than cropping: landscape puts the 340 px illustration on the right with
the week number, `WEEKS` and an 8×5 week grid (filled = past, ring with a dot =
current, empty = ahead) in a left column; portrait is calendar-style, with the
illustration on top, the grid at the lower left and the week number at the lower
right. Tapping the illustration opens the Baby page and tapping the week number
block opens the Mom page; the menu icon opens the launcher. The dashboard has no
progress bar or countdown. Setup and Baby/Mom detail pages have matching
portrait layouts. Weeks 6–40 each have a distinct engraving-style plate. The
340×340 sources are replaceable under `assets/pregnancy/week-plates/` and are
compiled into 1-bit assets.
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
removes the most recent tap, and on the counter page the top button also counts
one movement. Finish saves a non-empty session. An active session
keeps the device awake and is forced to finish after 60 minutes. Capacity is 400
records. New sessions use the shared asynchronous MicroSD repository; the
saved totals change only after durable completion. A failed save retains the
draft for a stable-ID reload/retry, rejecting different-card or conflicting
records. Cancellation discards the active draft. Ten partial updates trigger
a full monochrome cleanup. The captured start date/time remains fixed across
midnight, and summary pages update for the new date.

The first page shows today's total, session count, period totals and latest
session. The second page shows seven calendar days as a bar chart, recorded-day
and session totals, the average per recorded day, the most-recorded time period,
and today's period breakdown. These values describe what the user recorded;
they are not a normal/abnormal score, diagnostic threshold or extrapolated
12-hour estimate. The Kicks pages show recorded data and comparisons only; they
carry no care-seeking instruction.

The first Start opens a record-type choice, and the chosen type is remembered:
later Starts (including the double-click shortcut) go straight to that type, and
the Today page has a Change row to pick another. Choosing opens:

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
the double-click shortcut, e-paper clean refresh and a MicroSD status summary;
its header returns to Baby Week. The Shortcut page offers four targets (today's
weight entry, Baby Week home, start Kicks, off), saves on tap and stays on the
page. Pregnancy Settings shows the dating source, due date, current week and
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
Kicks scans all 42 historical slots independently. A card change invalidates
cleanup eligibility until another verified import. Tapping the dataset opens a
confirmation that deletes only `PREGJNL`.

## Sleep, wake and refresh

Stable pages normally enter deep sleep after 60 seconds without activity when
external power is absent. Settings keeps the device awake for three minutes;
date/time editors and an active kick-count session do not time out. The top
button is a wake source, and a two-second chord on both side buttons requests
sleep.

Before sleep, the coordinator schedules the daily 03:00 maintenance refresh
and any future event requested by the active app. The timer is armed up to 15
seconds early. There are no business wake events or timer-triggered alarms. The
persisted app IDs of the retired Reminder and Checkup apps stay reserved and fall
back to Baby Week when read from an older saved state.

Page changes use a full refresh; mutations inside a page use partial refresh,
and the display layer makes every eleventh consecutive partial refresh a full
one. Opening the launcher, returning to the app and switching apps draw their
first frame with the fast (partial) waveform, and every sixth such transition is
a full refresh. Settings also offers an explicit cleanup action. Trial
(2026-10-18): switching between the pregnancy home and the baby page uses the
partial waveform and, once the user has been idle for 5 s, one full refresh
repaints the page; every other page change keeps the full refresh. Entering the
pregnancy home with a profile the app already holds draws the home page at once
and re-checks the card quietly. Weight and Kicks draw no loading page when the
card answers within 1.5 s. Details: [refresh strategy](eink-refresh-strategy.md).

## Verification status

The four-app implementation is a candidate awaiting physical acceptance. See
[V1 validation](../V1-VALIDATION.md) for automatic results, firmware checksums
and the manual gate. Historical single-card debug results do not certify the
new release. Use [hardware checklist](hardware-regression-checklist.md).

## Known scope boundaries

There are no reminder or checkup apps, business alerts, habits, notes,
recording, provisioning, network access or speech-to-text. Records that older
firmware left on a card or in NVS are neither read nor erased. Kicks has no per-session history
editor/deletion UI; its 400-record limit requires planning around a backed-up
card dataset. Weight history edits the existing date's value, not its date.
The shared index holds at most 1,024 distinct keys including deleted keys;
there is no tombstone or journal garbage collection in this candidate. Full
capacity rejects new keys and never silently drops records. User data has no
Flash fallback.
