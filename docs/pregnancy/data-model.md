# Data model

All persisted records carry a schema version. User-facing text uses bounded
character arrays rather than dynamic strings.

## PregnancyProfile

- schema version
- last menstrual period
- estimated due date
- primary input source (confirmed due date or LMP)

Both dates are stored so calculations do not depend on repeating conversion at
every render. A confirmed due date is preferred when supplied.

## PregnancyProgress

- signed gestational day
- week plus day
- days until due date
- normalized progress and integer percent
- trimester
- before-start and overdue flags

## Reminder

- id, type, scheduled date/time
- repeat rule: none, daily or weekly
- enabled and completed flags
- bounded title and note

Capacity is 12. Completion rolls recurring reminders forward and marks one-time
reminders complete.

## Checkup

- id, appointment date/time
- recommended week and reference-template marker
- completed flag
- bounded title, location and note

Capacity is 8. Entries are organizer data; they are not medical recommendations.

The current editor creates a new item for today + 7 days at 09:00. The service
also retains the hour/minute fields for future editing support.

## WeightProfileSettings

- height in centimetres (default 165; valid range 100–220)
- pre-pregnancy baseline in tenths of a kilogram (auto-filled by the first saved
  weight when unset)
- display unit flag (`kg` or `lb`)

These settings are stored with the weight namespace. Weight values are always
stored as tenths of a kilogram; lb is a display/input conversion only.

## WeightRecord

- id and calendar date
- weight stored as integer tenths of a kilogram
- bounded note

Capacity is 32. Adding another value on the same date updates the existing
record. Height and pre-pregnancy baseline are stored as profile settings. BMI,
total gain and weekly change are derived at runtime.

## KickSession

- id, date and start time
- period: morning, afternoon or evening
- duration in minutes and counted movements
- bounded note

Capacity is 42. Sessions are valid for 0–120 minutes and 0–500 movements; the
current UI caps an active session at 60 minutes and debounces taps within five
seconds. Daily summaries total the three periods, calculate the 12-hour
estimate as `sampled_total × 4`, and compare the sampled total and each period
with the previous day. Attention states are prompts to contact the care team,
never diagnoses.
