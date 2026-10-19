# Storage and privacy

The four-app candidate stores pregnancy profiles, weights, Kicks and preferences
in the selected MicroSD `PREGJNL` dataset. There is no fallback to Flash for missing, read-only, full or damaged cards.

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

Stable repository kinds are pregnancy (1), weight (4), preference (9) and
Kicks (10, appended). The values 2, 3 and 5–8 belonged to retired reminder,
checkup, habit and habit-event records. The code no longer names them, but
`repository_kind_valid()` still accepts 1–10, so a card written by older
firmware mounts and its journals and snapshots replay unchanged. Those records
are never read, written or migrated.

## Canonical domain records

`pregnancy/services/domain_repository_records.*` assigns fixed 32-character
lowercase-hex keys and explicit payloads to the pregnancy profile, weight record, kick session, weight preference and
device preference records. Each payload contains a little-endian envelope, an
independent magic and model version, bounded fields and CRC32/IEEE. Raw C++ or
NVS structs, enum widths and compiler padding are never part of the card format.

Application tasks use asynchronous SD adapters and poll service-owned completion
handles. Maximum request/result workspaces live in static storage rather than on
the 4.6–5.1 KiB application stacks.

## Legacy NVS migration

Settings → MicroSD Storage → Old Device Data performs an explicit, idempotent
import. The reader covers the historical `pregnancy`,
`weight` and `sticky_core` namespaces, including the due-date-only pregnancy
format and alternating device-setting slots. Every valid record is converted to
the canonical card payload.

Migration never overwrites a different card value. Matching values count as
already imported; conflicts and corrupt source records remain visible and block
cleanup. Newly imported values are read back from the repository before the run
can be marked verified. Clearing old copies is a separate confirmation action
enabled only after a conflict-free verified run on the same mount generation
and dataset. The profile/weight/settings cleanup never clears Kicks or any namespace it
did not import. The separate Kicks reader scans exactly 42 `k00`–`k41` slots,
validates the legacy FNV/version/layout and imports stable IDs into kind 10.
Its independently confirmed cleanup erases only `kicks`. Neither cleanup touches
SDK or unrelated application namespaces. Tombstoned target IDs count as conflicts.

## Capacity and memory

Weight capacity is 366 live records; Kicks holds 400 sessions (schema 2; the
legacy NVS import still reads the 42 historical slots). The recovery
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

## NVS audit

The firmware writes no domain data to NVS; the legacy modules for pregnancy,
weight, settings and Kicks only read and validate. ESP-IDF may still maintain
its own PHY calibration/system namespaces; these are not user records and the
migration tool does not erase them.

## Settings and destructive actions

The storage page reports card/access state, dataset recovery state, capacity,
dataset ID prefix and generation. Safe Eject closes admission, drains accepted
work and unmounts the card; it remains unmounted until physical removal.

Destructive operations are intentionally separate:

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
`tools/validate_sd_backup.py`; never merge two dataset IDs. Full recovery and restore steps are documented in
[S1 backup and recovery](../S1-BACKUP-RECOVERY.md).
