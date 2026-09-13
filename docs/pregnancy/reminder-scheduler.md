# Reminder and scheduler

Apps never program RTC wake sources directly. At sleep preparation the
framework coordinator gathers independent candidates and arms one timer wake.

## Candidates

- the next daily pregnancy refresh at 03:00;
- the active app's requested wake (Baby Week requests the next day at 03:00);
- the earliest enabled, incomplete reminder;
- the earliest incomplete checkup.

`StickyScheduler` holds up to eight typed events and selects the closest future
epoch. Event IDs remain separate so one source cannot overwrite another. The
chosen ESP32 timer wake is scheduled up to 15 seconds before the event.

## Due alerts

After a timer wake the coordinator checks uncompleted reminders and checkups in
the window from 60 seconds before to 30 seconds after the current epoch. If any
item matches, the existing repeating buzzer starts. A touch or button stops the
buzzer before normal app handling. The selected app and display rotation are
restored from RTC no-init context; a daily refresh window (02:59–03:02) starts
Baby Week when no alert is due.

## Reminder semantics

The reminder service stores up to 12 records. It supports six types (General,
Supplement, Checkup, Water, Activity and Rest), enabled/disabled state,
completion, deletion and repeat rules `None`, `Daily` and `Weekly`.

- One-time completion sets `completed=true`.
- Daily completion advances the date by one day and clears completion.
- Weekly completion advances the date by seven days and clears completion.
- Upcoming results include only enabled, incomplete future items and are sorted
  by epoch. The UI displays up to four rows.

The current add screen creates a one-time reminder for today's date and an hour
chosen from the RTC. Editing title, note and recurrence is a service capability
but is not exposed by the current UI.

## Checkup semantics

Checkups are independent records (capacity 8) with completion and deletion. The
current add screen starts at today + 7 days, adjusts by −1/+1/+7 days and saves
at 09:00. The scheduler considers the earliest incomplete appointment; all
appointment details must be confirmed with the user's care team.

On every new sleep cycle candidates are recomputed from persisted NVS data, so
completing, deleting or advancing a recurring reminder changes the next wake.
