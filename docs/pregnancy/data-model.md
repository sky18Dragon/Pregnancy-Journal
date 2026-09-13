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
- completed flag
- bounded title and note

Capacity is 12. Completion rolls recurring reminders forward and marks one-time
reminders complete.

## Checkup

- id, appointment date/time
- recommended week and reference-template marker
- completed flag
- bounded title, location and note

Capacity is 8. Entries are organizer data; they are not medical recommendations.
