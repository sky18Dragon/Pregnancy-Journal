> The UI includes Pregnancy, Weight, Kicks and Settings only.

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

## WeightProfileSettings

- height in centimetres (default 165; valid range 100–220)
- pre-pregnancy baseline in tenths of a kilogram (auto-filled by the first saved
  weight when unset)
- display unit flag (`kg` or `lb`)

These settings are stored in the MicroSD preference repository. Weight values are always
stored as tenths of a kilogram; lb is a display/input conversion only.

## WeightRecord

- id and calendar date
- weight stored as integer tenths of a kilogram
- bounded note

Capacity is 366. Adding another value on the same date updates the existing
record. Height and pre-pregnancy baseline are stored as profile settings. BMI,
total gain and weekly change are derived at runtime.

## KickSession

- id, date and start time
- period: morning, afternoon or evening
- duration in minutes, elapsed seconds and counted movements
- mode: 1-hour count (Timed), Count to 10, or Feel check-in (Awareness)
- feel (check-ins only): as usual, weaker, fewer or pattern changed
- bounded note

Schema 2 adds mode, feel and elapsed seconds. Schema-1 MicroSD payloads decode
as Timed sessions with elapsed seconds derived from minutes; the frozen 60-byte
legacy NVS layout (`KickSessionV1`) is used only by the old-device import.
Check-ins must have a feel and zero count; counted sessions must have no feel;
Count to 10 never exceeds ten movements.

Capacity is 400. Sessions are valid for 0–120 minutes and 0–500 movements; the
current UI caps an active session at 60 minutes and filters repeat taps within
400 ms. Daily summaries include period totals, session count, total duration
and the latest session. Seven-day summaries retain each calendar day's raw
recorded total and derive recorded-day/session counts, average per recorded day
and time-of-day distribution. No model field represents a diagnosis, universal
target, inferred 12-hour count or normal/abnormal state.

`kick_comparison.*` derives, at runtime only, a comparison against the median
of the same record type in the seven days before a record (Timed: same period,
movements per hour; Count to 10: seconds, with an hour-long unfinished session
counted as 60 minutes). Three baseline records are required. The result holds
direction (less/more/same), absolute difference and percentage; nothing is
persisted.
