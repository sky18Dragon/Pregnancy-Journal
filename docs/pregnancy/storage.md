# Storage and privacy

All product records are local to the device. The firmware has no account,
network, telemetry, cloud log, OTA or AI integration.

## NVS namespaces

| Namespace | Data | Record strategy |
| --- | --- | --- |
| `sticky_core` | language and last selected app | alternating `settings_a` / `settings_b` records with version, sequence and FNV checksum |
| `pregnancy` | due date, LMP and primary source | `config` plus `config_b`; newest valid record wins |
| `reminders` | up to 12 reminders | one checksummed record per `r00`–`r11` slot |
| `checkups` | up to 8 appointments | one checksummed record per slot |
| `weight` | up to 32 records plus height/baseline/unit profile | one checksummed record per `w00`–`w31` slot and a `profile` blob |
| `kicks` | up to 42 sessions | one checksummed record per `k00`–`k41` slot |

Pregnancy, reminder, checkup, weight-record and kick-session payloads carry
explicit schema versions and bounded character arrays. Their storage wrappers
also validate a magic value, payload size and FNV-1a checksum before accepting a
record. The weight `profile` blob is validated by its exact size and height
range; it is not a historical measurement record.

## Recovery behavior

- Device settings select the newest valid alternating slot. An invalid last-app
  ID falls back to Baby Week; an unknown language falls back to English.
- Pregnancy profile loading first tries `config`, then the backup. A valid
  legacy due-date-only v1 record is migrated to the current profile format and
  written back.
- Reminder, checkup, weight and kick loaders skip missing or corrupt slots and
  retain valid siblings. Saving rewrites active slots and erases unused slots.
- A failed write is returned to the app layer; a subsequent load accepts only
  records that still pass their namespace validation.

## Data lifecycle

Records are loaded when their app starts or resumes and saved after each user
mutation. “Remove latest” in Weight deletes the chronologically latest weight;
“Reset Today” in Kicks deletes every session for the current RTC date. There is
currently no export, backup, account sync or bulk erase screen.

The device stores health-related notes and measurements without encryption in
its local NVS partition. Users should protect physical access to the device and
consult the care team for medical interpretation.
