# Storage

## NVS

Device settings and PregnancyProfile use versioned, checksummed redundant state.
The pregnancy loader migrates the previous due-date-only v1 record into the new
profile representation.

Reminder and Checkup use one versioned/checksummed record per NVS slot. A damaged
slot is skipped without discarding valid siblings. Writes commit only after all
slot updates succeed.

No pregnancy, reminder or checkup content is logged. All data stays on the
device in this phase.
