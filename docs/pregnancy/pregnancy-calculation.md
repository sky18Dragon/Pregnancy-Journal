# Pregnancy calculation

The service is pure C++ and has no ESP32 dependency.

- LMP input derives the estimated due date as LMP + 280 days.
- Confirmed due-date input derives the calculation origin as due date - 280 days.
- Gestational day is `today - origin`.
- Week/day is floor division by seven for non-negative days.
- Trimesters are weeks 0–13, 14–27 and 28 onward.
- Display progress is clamped to 0–100% over 280 days.
- Dates before the origin return a valid `before_start` state.
- Dates after the due date preserve `40+N`/`42+N` information and set `overdue`.
- Calendar conversion supports leap years and validates year, month and day.

The UI is informational and never treats the calculation as a diagnosis. The
saved estimated due date can be changed from Pregnancy Settings.

Host coverage includes leap dates, due-date/LMP round trips, trimester
boundaries, the due date, pre-start dates and post-term dates.
