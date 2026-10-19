# Firmware architecture

This is the concise architecture entry point for the current repository. The
deeper product-specific description is in `pregnancy/architecture.md`.

```text
app_main
  -> board/device initialization
  -> SD lifecycle worker -> Sticky SD adapter -> board SD/shared SPI2
  -> app registry + framework coordinator
       -> active app and launcher
            -> domain/content services
                 -> asynchronous SD repository adapters
       -> Canvas and display refresh policy
  -> board/device HAL
```

## Layers and ownership

| Layer | Main paths | Owns |
| --- | --- | --- |
| Composition | `src/main.cpp`, `src/app/app_registry.cpp` | Boot order and registered app descriptors |
| Coordinator | `src/app/sticky_app.*`, `app_manager.*` | Launcher, active app, input routing, display hand-off, sleep/wake |
| Applications | `src/apps/*` | Page state, user actions and domain orchestration |
| Domain/content | `src/pregnancy/*` | Pregnancy calculations, content and bounded record rules |
| Shared UI | `src/ui/*` | Canvas, typography, layout helpers, localization and assets |
| Persistence/system | `src/storage/*`, `src/system/*` | Dataset-bound records and next-event scheduling |
| Hardware abstraction | `src/board/*`, `src/devices/*`, `src/display/*`, `src/input/*`, `src/sensors/*` | Buses, power and peripheral drivers |
| Components | `components/*` | GT911 and e-paper controller implementations |

Dependencies point downward. A device driver must not know about a pregnancy
page; a renderer must not acquire a hardware bus; an app must not activate
another app directly.

## Boot and runtime

SPI2 initialization is owned by `board_shared_spi_init()`; display and SD reuse
it. This startup-only API is not a concurrent hotplug manager.

`app_main()` establishes the power latch, charger, shared SPI2, sensor I2C,
RTC, battery, display, touch, buzzer, IMU, NVS and language state before the
application coordinator runs. Required display/input failures halt safely;
optional readings such as RTC or battery can degrade without blocking the UI.

`app_registry.cpp` is the application composition root. Four descriptors are
registered: Baby Week, Weight, Kicks and Settings. Historical app IDs remain
valid persistent values; unregistered IDs fall back to Baby Week. Each
supplies start, pause, resume, pre-sleep, timeout and sleep-policy callbacks.
`StickyAppManager` starts Baby Week by default, validates persisted IDs, pauses
the old owner before switching and restores it when activation fails.

The coordinator owns global events and the one shared `Canvas`. Apps may keep
local state but must stop consuming display/touch resources while paused.

## Input and orientation

GT911 events are converted from physical coordinates to the current logical
orientation before page hit testing. Button/touch callbacks enqueue or expose
events; they do not perform long display or storage work. The IMU publishes a
settled orientation, and every registered app uses `FollowDevice` policy.

## Display path

```text
domain state -> page renderer -> Canvas -> overlay/rotation -> SSD1677 driver
```

The Canvas supports logical grayscale, while current product pages normally use
black and white. The display layer performs the physical 180-degree transform,
frame comparison and controller transaction. Stable entry uses full monochrome
refresh; small mutations use partial refresh; transition counting and Settings
provide cleanup refreshes. The display layer also promotes a partial request to
a full refresh after ten consecutive partial refreshes, so no page can
accumulate ghosting beyond that ceiling.

## Data and scheduling

All service collections have fixed capacities: 366 weights and 400 kick
sessions. Pregnancy/weight/preferences use the journaled MicroSD repository,
including Kicks (kind 10). Canonical payloads use schema/version validation,
bounded strings and checksums. Namespaces fail independently so one corrupt
record does not invalidate unrelated data.

The next-version SD path starts with the hardware-independent
`storage/sd_card_state.*` lifecycle rule. It exposes mount generations and
read/write admission for Absent, Mounting, read-write, read-only, Full, Fault,
Ejecting and Unmounted states. It owns no bus, file or task.
`devices/sticky_sdcard.*` is the single runtime adapter over the low-level
`board_sd_card.*` operations. It samples card-detect outside interrupt context,
revokes access before cleaning up an unexpected removal, and splits safe eject
into begin/drain and finish/unmount phases. Board cleanup failure remains a
Fault instead of being reported as safely removed. Board cleanup is staged so
FatFs unmount, CS restoration and card power-off can be retried safely.

`storage/sd_storage_service.*` is the production lifecycle owner. Its one
FreeRTOS task polls card detect, mounts after insertion, retries cleanup and
handles a bounded control queue. Normal boot starts it without making a missing
or unsupported card fatal. Before deep sleep, the framework coordinator waits
for this owner to finish safe eject; failure or timeout blocks deep sleep. The
same bounded FIFO accepts explicit manifest and journal requests and stores
terminal results in separate eight-slot service-owned completion pools. Queueing
is not persistence: callers retain an opaque handle and collect the result once.
`sd_manifest_completion.*` owns the fixed slot state machine. Slots are not
reused until collection or cancellation; request IDs reject stale handles and
late completion attempts after reuse. Results preserve both the generation
captured at submission and the generation observed by the worker. Every request
is rechecked against card state and mount generation inside the worker.
Initialization additionally requires an explicit confirmation flag.
The submission lock closes file admission before a sleep command enters the
FIFO. A separate bounded lease gate tracks longer-running
record work, binds every lease to mount generation plus dataset ID, and makes sleep
wait for those operations before queuing unmount. Late completion after a card
switch releases its slot but returns a stale/dataset error. The worker also owns
an independent eight-slot production repository pool. Its read/list/commit/
erase requests recheck generation and dataset identity; mutations write and
verify the immutable object, durably append the journal transaction, then
rebuild the published recovery view from disk before completion. The FIFO holds
only the slot handle, not the maximum-size payload or result. No app or domain
repository calls this production API yet, and existing user records still
persist in NVS. Apps and pages must not access the adapter or filesystem
directly.

The owner also publishes FAT total/free bytes. Business writes use actual free
bytes. A zero-free card enters Full and polling can restore writable state after
space becomes available.

The first S1-02 format layer is hardware independent. `storage_crc32.h`
provides the shared incremental CRC32/IEEE implementation, while
`sd_storage_layout.h` freezes `/sdcard/PREGJNL/` and its manifest files to
uppercase ASCII 8.3 names. `sd_manifest.*` strictly encodes and decodes a
bounded canonical JSON record containing schema, a 128-bit lowercase-hex
dataset ID, nonzero commit generation and CRC. It selects the newest valid A/B
copy and rejects any pair whose valid copies name different datasets. Normal
boot still does not write these files. `sd_manifest_reader.*` is the first
worker-owned file boundary: after each successful mount it opens the two fixed
slots read-only,
validates bounded contents, and reports NotInitialized, Ready, Degraded,
Corrupt or IoError. Missing slots never trigger directory or file creation.

`sd_manifest_writer.*` is an explicit, hardware-independent transaction
boundary called only for an admitted worker request. Initialization creates only the
fixed application and manifest directories, never formats media, and writes
generation 1 to slot A. Later commits require the same dataset and exactly the
next generation, write only the inactive parity slot, then flush, `fsync`,
close, read back and re-probe the pair. The previous active slot remains
untouched if open, short-write, sync, close or verification fails. Retrying an
already selected record is idempotent. The design does not depend on FAT rename
atomicity. `sd_manifest_request.*` performs the hardware-independent confirmation,
card-state, generation and error-mapping checks used by that worker boundary.
Settings initialization and the physical generation 1/A to 2/B write path have
passed on the target device, including remount after sleep/wake. Completion-pool
saturation, slot reuse and stale/late result isolation are host-tested. Recovery
log framing is defined by `sd_journal.*`: each append frame binds a nonzero,
monotonic transaction ID to the selected dataset and protects a bounded opaque
payload with payload and whole-frame CRC32. The scanner accepts only complete
valid frames, reports the exact valid prefix for a truncated tail, and blocks on
middle corruption, unknown schema/type or another dataset.
`sd_journal_file.*` adds the unexposed file transaction boundary: it streams one
bounded frame at a time, refuses to append past any invalid tail, performs
append/flush/`fsync`/close, and replays the segment before confirming success.
Exact last-frame retries are idempotent, while same-ID/different-content retries
are conflicts. `sd_journal_payload.*` freezes bounded transaction references and
checkpoint payloads; `sd_repository_recovery.*` rebuilds a bounded visible view
only from complete payloads, validates a whole batch before mutation, preserves
tombstones against late results and rejects checkpoint live-count mismatch.
A checkpoint is its own strictly newer journal transaction: its payload names
the immediately preceding visible transaction represented by the immutable
index. Recovery accepts it only when that snapshot transaction equals the
already replayed prefix, preventing a future, stale or skipped snapshot from
being installed.
Injected short writes leave a blocked truncated tail; ambiguous
sync/close outcomes are resolved by the next read-first retry. Runtime worker
admission and payload validation are integrated; physical removal/power-loss
validation remains pending before business repositories may write real card
data. See
[the S1 journal format](S1-JOURNAL-FORMAT.md).

The isolated partial-journal profile exercises that rule without timing luck:
it persists exactly half of a known next frame after a verified prefix. Startup
may publish only the earlier complete records as degraded; the incomplete
transaction is invisible and the file is never repaired or extended.

`sd_record_object_file.*` is the immutable object boundary used before a
transaction can reference a record payload. It accepts only fixed `RECORDS`
paths and bounded content, probes before writing, treats an exact existing file
as an idempotent retry, and refuses to overwrite any partial or conflicting
file. A new object is acknowledged only after flush, `fsync`, close and exact
read-back verification. The isolated object power-test profile reaches this
boundary only through the storage worker and deliberately performs no journal
or index mutation. Its partial-write companion uses a second fixed fixture and
intentionally persists only a verified 512-byte prefix, allowing reboot
recovery to prove that a short uncommitted object remains isolated and is
never extended or repaired.

`sd_index_snapshot.*` freezes the bounded `PJIX` recovery snapshot and its
immutable checkpoint-candidate write boundary. The snapshot binds sorted,
versioned record references to one dataset and last transaction, carries entry
and file CRCs, and is accepted only after full readback. It is not authoritative
by presence: only a complete journal checkpoint with matching identity, size
and CRC may select it. Unreferenced complete or partial files remain inert. The
isolated checkpoint profile verifies this boundary by referencing the existing
transaction-36 snapshot from checkpoint transaction 37 without rewriting the
snapshot. A separate partial-index profile persists only a verified prefix
under a different filename and proves that an unreferenced truncated candidate
cannot replace the checkpoint-selected snapshot.

`sd_recovery_loader.*` is the read-only startup composition layer owned by the
storage worker. It discovers validated journal records, accepts only a
checkpoint that names the immediately preceding visible transaction and its
actual segment, reads the exact immutable index named by that checkpoint,
restores the bounded recovery index, replays later records and streams every
live object to verify its recorded length and CRC. Only after the whole chain
succeeds does the service publish a ready recovery view. Missing/mismatched
index or object files block the view; a valid truncated journal tail publishes
only its verified prefix as degraded. This layer never repairs or deletes card
content and does not enable business writes.

Before sleep, the pure `app/sticky_sleep_schedule.*` policy resets the scheduler
and selects daily 03:00 maintenance or an active-app future system event. The
ESP32 timer is armed up to 15 seconds early. There are no business wake
alarms. The index and snapshot share a 1,024-key bound (including
tombstones); the worker allocates recovery buffers in PSRAM. Adapters bind their
loaded state to a dataset/mount generation until explicitly reset for reload.

## Extension points

- New app: descriptor + stable `StickyAppId` + launcher entry + lifecycle hooks.
- New domain: model + pure service + storage adapter + tests.
- New peripheral: board-owned bus/power contract + device driver + app-facing
  result; never access it from a page renderer.
- New visual asset: editable source + deterministic generator + generated C++.

See `COMPONENT-GUIDELINES.md` and `refactor/adding-an-app.md` for checklists.
