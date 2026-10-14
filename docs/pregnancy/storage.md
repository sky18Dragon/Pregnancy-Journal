# Storage and privacy

The four-app candidate stores pregnancy profiles, weights, Kicks and preferences
in the selected MicroSD `PREGJNL` dataset. Old reminder/checkup and network
records remain supported by storage and backup, with their UI deferred to V2.
There is no fallback to Flash for missing, read-only, full or damaged cards.

## MicroSD repository

The application root is `/sdcard/PREGJNL/`. A/B manifests carry a stable
32-character dataset ID and monotonically increasing commit generation.
Repository commits run only on the shared storage worker and follow this order:

1. Write an immutable content-addressed object and verify it by readback.
2. Append, flush, `fsync`, close and replay one canonical journal transaction.
3. Reload the recovery view from disk before reporting success.

Only committed journal references are visible. A complete object written before
a failed journal commit remains an isolated orphan and is never silently
published, overwritten or repaired. Requests bind both mount generation and
dataset ID, so a completion from a removed card cannot be accepted for the next
card. Numeric tombstones retain the high-water mark and record IDs are not
reused after restart.

Stable repository kinds separate pregnancy, reminder, checkup, weight,
preference, Kicks (appended kind 10), habit and habit-event records. Existing daily/weekly reminders stay
reminders; migration does not reinterpret them as habits. Habit kinds are
reserved for the later S3 model.

## Canonical domain records

`pregnancy/services/domain_repository_records.*` assigns fixed 32-character
lowercase-hex keys and explicit payloads to the pregnancy profile, reminder,
checkup, weight record, kick session, weight preference, device preference and network
authorization records. Each payload contains a little-endian envelope, an
independent magic and model version, bounded fields and CRC32/IEEE. Raw C++ or
NVS structs, enum widths and compiler padding are never part of the card format.

Application tasks use asynchronous SD adapters and poll service-owned completion
handles. Maximum request/result workspaces live in static storage rather than on
the 4.6–5.1 KiB application stacks.

## Legacy NVS migration

Settings → MicroSD Storage → Old Device Data performs an explicit, idempotent
import. The reader covers the historical `pregnancy`, `reminders`, `checkups`,
`weight` and `sticky_core` namespaces, including the due-date-only pregnancy
format and alternating device-setting slots. Every valid record is converted to
the canonical card payload.

Migration never overwrites a different card value. Matching values count as
already imported; conflicts and corrupt source records remain visible and block
cleanup. Newly imported values are read back from the repository before the run
can be marked verified. Clearing old copies is a separate confirmation action
enabled only after a conflict-free verified run on the same mount generation
and dataset. The profile/weight/settings cleanup never clears reminders,
checkups or Kicks. The separate Kicks reader scans exactly 42 `k00`–`k41` slots,
validates the legacy FNV/version/layout and imports stable IDs into kind 10.
Its independently confirmed cleanup erases only `kicks`. Neither cleanup touches
SDK or unrelated application namespaces. Tombstoned target IDs count as conflicts.

## Capacity and memory

Weight capacity is 366 live records; Kicks remains 42 sessions. The recovery
index and schema-1 snapshot accept 1,024 distinct keys including tombstones;
existing 64-entry snapshots are compatible. A full index rejects new keys even
after deletion because tombstones are retained. There is no garbage collection
or key reuse. Back up the entire dataset before changing cards or explicitly
deleting it; deletion is not an automatic capacity-recovery action.

The index, decode aggregate and encoded snapshot buffer are allocated once in
PSRAM by the storage service (about 205 KiB; the allocation log reports the exact ABI size), never
on an app/worker stack. Allocation failure stops storage startup. The power-test
fixture keeps only its 2,960-byte encoded buffer. Actual free heap and stack
headroom must be recorded on hardware; debug recovery logs expose them.
Journal segments remain bounded to 256 KiB and numbered 1–99,999. Historical
segments, orphan objects and old object versions are not reclaimed. Each commit
currently replays/verifies the store; long-journal latency is a physical gate,
not an assumed unlimited-performance guarantee.

## Network authorization and NVS audit

Network authorization is a preference record on MicroSD. It stores the provider
ID (`coze` for the first provider), service region, configuration and
authorization versions, and an AES-256-GCM credential envelope. Plain SSID,
password and gateway device token exist only in bounded RAM buffers and are
cleared after use. The envelope is bound to the station-MAC-derived device ID
and a random 256-bit local wrapping key.

The wrapping key is the only new application-owned NVS value
(`sticky_keys/net_root`). It is cryptographic device material, not user data;
moving the card to another device or resetting the local key makes the envelope
unusable and requires explicit reauthorization. Production deployments should
enable ESP-IDF NVS/Flash encryption when resistance to direct Flash extraction
is required.

Before any station credentials are applied, the adapter calls
`esp_wifi_set_storage(WIFI_STORAGE_RAM)`. No Wi-Fi SSID or password is written
to the SDK's persistent Wi-Fi configuration. ESP-IDF may still maintain its own
PHY calibration/system namespaces; these are not user records and neither the
migration tool nor device reset erases them. The V1 build contains no domain NVS write adapters. Pregnancy, reminder,
checkup, weight and Kicks legacy modules retain read/validation support only.

## Settings and destructive actions

The storage page reports card/access state, dataset recovery state, capacity,
dataset ID prefix and generation. Safe Eject closes admission, drains accepted
work and unmounts the card; it remains unmounted until physical removal.

Destructive operations are intentionally separate:

- **Reset This Device** removes only the local network wrapping key and clears
  the RAM-only Wi-Fi configuration. It does not mount or alter the card.
- **Delete PREGJNL Data** removes only the application directory from the
  currently mounted card through the storage worker. It does not format the
  card or delete sibling directories/files. Repository caches and scheduled
  events are invalidated immediately after success.
- **Clear Old Copies** removes only verified migrated NVS namespaces and is
  unavailable before a successful migration/readback report.

The application-data delete helper validates the exact `PREGJNL` directory name,
bounds traversal depth/path length and is host-tested against a sibling file
that must remain unchanged.

## Backup and recovery

Copy the complete `PREGJNL` directory as one dataset and validate it with
`tools/validate_sd_backup.py`; never merge two dataset IDs. Network credential
envelopes restored to another device cannot be opened and must not cause queued
cloud work to be resubmitted. Full recovery and restore steps are documented in
[S1 backup and recovery](../S1-BACKUP-RECOVERY.md).
