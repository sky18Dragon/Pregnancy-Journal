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
- Registered apps: Baby Week, Reminders, Checkups, Weight, Kicks and Settings.
- Languages: English and Simplified Chinese. The language setting is stored in
  device NVS and can also be changed from the launcher.
- Data boundary: no account, network, telemetry, cloud log, OTA or AI service;
  all pregnancy records remain on the device.

## Launcher and navigation

The launcher is a fixed landscape layout for the 800×480 panel. Baby Week is a
large home card; Checkups, Reminders, Weight and Kicks are smaller cards;
Settings is a header action. A selected card is filled black and the remaining
cards are outlined.

- Top-button single click toggles the launcher.
- Swipe up beginning in the bottom edge opens it; swipe down beginning in the
  lower 30% closes it.
- Swipes must be predominantly vertical, travel at least 96 px (or 20% of the
  panel height), and finish within 60–1600 ms.
- A launcher with no activity closes after 30 seconds.
- Double-clicking the top button always returns to Baby Week.
- Touching the language control toggles English and 简体中文.

## App behavior

### Baby Week (Pregnancy)

First use is a two-part setup: enter device time, then choose a confirmed
estimated due date or the first day of the last menstrual period. Dates are
entered as `YYYYMMDD`; clock input is `YYYYMMDDHHMM`. The profile is saved in a
redundant, checksummed NVS record and can be edited later.

The dashboard shows week, day, trimester, days until (or days past) the due
date and a progress bar capped at the nominal 40-week/280-day duration. The
Baby and Mom tabs show bundled offline content for the current week. Content is
broad, informational guidance and is clamped to weeks 1–40.

### Reminders

The list switches between Today and Upcoming views and shows up to four rows at
once. A reminder can be created from six types: General, Supplement, Checkup,
Water, Activity and Rest. The current UI chooses today's date and an hour; the
domain model also supports daily and weekly recurrence, completion, deletion,
per-item enabled state and a global enable/disable switch. Capacity is 12.

Completing a one-time reminder marks it complete. Completing a daily or weekly
reminder advances its date by one or seven days and clears completion. The
earliest enabled, incomplete future reminder is considered for the next sleep
wake.

### Checkups

The app stores dated appointments (capacity 8), shows up to four rows, and
supports add, complete and delete. Add starts seven days from the current RTC
date and uses a fixed 09:00 default; the date can be moved by −1 day, +1 day or
+7 days. The saved title is “Routine Checkup”. Appointment timing must be
confirmed with the user's care team.

### Weight

Weight is stored internally as tenths of a kilogram (30.0–200.0 kg). The app
records one value per calendar day; saving the same day replaces that day's
record. The dashboard shows the latest value, up to seven recent points, BMI,
change from baseline and a weekly rate when a prior record exists 5–14 days
earlier. Units can be displayed and edited as kg or lb.

Height is adjustable from 100–220 cm and defaults to 165 cm. The first saved
weight becomes the baseline when no baseline exists. The trend warning uses
fixed BMI-category maximum total gains and flags a gain above that limit or a
weekly rate above 0.5 kg/week. Capacity is 32 records. “Remove latest” deletes
the chronologically latest record.

### Kicks

The app tags each session as morning, afternoon or evening and groups sessions
into those three periods in the daily summary. Starting a session infers the
period from the RTC (before 12:00, 12:00–17:59, or 18:00 onward). Tapping the
central area counts
one movement; taps within five seconds are ignored to avoid duplicates. Finish
saves the session, and an unfinished session is forced to finish after 60
minutes. Capacity is 42 sessions.

The summary totals the three periods, estimates 12 hours as `sampled total × 4`
and marks the day complete only when all three periods have data. Attention is
flagged when a period changes by more than 50% from the previous day or a
complete day's estimate is below 20. “Reset Today” removes all sessions for the
current date. These indicators are prompts to contact the care team, never
diagnoses.

### Settings

Settings provides language, device-time editing, Pregnancy Settings, e-paper
clean refresh and Return to Baby Week. Time input is `YYYYMMDDHHMM`; invalid
dates or RTC writes are rejected. The time editor cannot time out to sleep;
Settings' main page uses a three-minute timeout.

## Sleep, wake and refresh

Stable pages normally enter deep sleep after 60 seconds without activity when
external power is absent. Settings keeps the device awake for three minutes;
date/time editors and an active kick-count session do not time out. The top
button is a wake source, and a two-second chord on both side buttons requests
sleep.

Before sleep, the coordinator recomputes one next wake from daily 03:00 refresh,
the active app's requested event, the earliest reminder and the earliest
checkup. The timer is armed up to 15 seconds early. On timer wake, a due
reminder or checkup starts the repeating buzzer; any touch or button stops it.

Stable page entry uses a full refresh; mutations use partial refresh. After five
fast app transitions the display receives a cleanup full refresh. Settings also
offers an explicit cleanup action.

## Verification status

- Host suite: 20/20 tests pass, including Weight and Kicks services.
- PlatformIO release, debug and power-test profiles are the supported build
  gates; release was built successfully for the current scope.
- Physical-device regression for the current six-app image remains a separate
  manual gate. Use [hardware-regression-checklist.md](hardware-regression-checklist.md)
  after flashing.

## Known scope boundaries

The current UI does not provide free-form reminder title/note editing,
recurrence editing, checkup time editing, historical weight browsing or manual
kick-period selection. These are implementation limitations, not promised
features. Network sync, cloud backup, OTA, mobile configuration, voice and AI
features remain outside the product.
