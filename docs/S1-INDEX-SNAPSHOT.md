# S1 recovery index snapshot

This document freezes the bounded checkpoint-index format and its immutable
file boundary. An index is only a recovery accelerator: it becomes committed
when a complete journal checkpoint names its exact file size and CRC. A file in
`INDEX` that has no such checkpoint reference is an orphan and must not change
the visible repository state.

The snapshot transaction ID is the last visible transaction represented by the
index. The following checkpoint is a new, strictly greater journal transaction;
its payload names that snapshot transaction ID rather than claiming that the
snapshot already includes the checkpoint frame itself.

## Location and bounds

Index candidates live under `/sdcard/PREGJNL/INDEX/` and use uppercase ASCII
8.3 names. Schema v1 contains at most 64 entries and is at most 4,176 bytes.
All integers are unsigned little-endian.

### Header v1 (80 bytes)

| Offset | Bytes | Field | Rule |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | ASCII `PJIX` |
| 4 | 2 | schema | `1` |
| 6 | 2 | header length | `80` |
| 8 | 4 | total length | `80 + entry_count × 64` |
| 12 | 4 | entry count | `0..64` |
| 16 | 4 | live count | exact non-tombstone count |
| 20 | 4 | reserved | zero |
| 24 | 8 | last transaction ID | nonzero; must match the snapshot ID named by the checkpoint payload |
| 32 | 32 | dataset ID | lowercase hexadecimal |
| 64 | 4 | entries CRC32 | CRC32/IEEE over all encoded entries |
| 68 | 4 | file CRC32 | CRC32/IEEE over header bytes `0..67`, then entries |
| 72 | 8 | reserved | zero |

### Entry v1 (64 bytes)

| Offset | Bytes | Field | Rule |
| ---: | ---: | --- | --- |
| 0 | 1 | repository kind | known `RepositoryKind` |
| 1 | 1 | flags | `0` live, `1` tombstone |
| 2 | 2 | reserved | zero |
| 4 | 4 | revision | nonzero |
| 8 | 4 | object bytes | live: `1..1024`; tombstone: zero |
| 12 | 4 | object CRC32 | tombstone: zero |
| 16 | 32 | record ID | lowercase hexadecimal |
| 48 | 12 | object filename | live: uppercase 8.3; tombstone: empty |
| 60 | 4 | reserved | zero |

Entries are strictly sorted by repository kind and record ID. Duplicate or
out-of-order keys, unknown schema, bad lengths, nonzero reserved bytes, invalid
references, count mismatch or either CRC mismatch reject the entire snapshot.

## Durable write boundary

`sd_index_snapshot.*` encodes/decodes the format and owns the immutable file
operation. Before writing, it probes the complete candidate:

- an exact existing file is an idempotent success;
- any partial or different existing file is a conflict and is preserved;
- a missing file may be created only inside the fixed `INDEX` directory;
- success requires full write, flush, `fsync`, close and exact readback.

No code in this boundary appends a journal checkpoint, deletes an orphan,
truncates a partial file or repairs media.

## Isolated physical test

`sticky-index-power-test` is tied to the current disposable test baseline of 36
journal records and transaction 36. Boot probes the journal and
`INDEX/IDXPWR01.BIN` read-only. After two separate confirmations it writes one
2,960-byte, 45-entry deterministic snapshot but deliberately never appends a
checkpoint. The audio cue marks the unsafe-reset point.

After reboot:

- an exact 2,960-byte file is `PASS - COMPLETE INDEX ORPHAN`;
- a different/partial file is `PASS - PARTIAL INDEX ORPHAN`;
- the journal must still be 36 records / transaction 36;
- neither outcome is overwritten, deleted, repaired or published.

Use only the designated disposable test card. Once the fixed filename exists,
the test intentionally refuses to write it again.

The 2026-09-29 physical run first exposed a storage-worker stack overflow
before file I/O. Snapshot generation and pre-write validation were changed to
stream one entry at a time instead of materializing two maximum-size snapshot
aggregates on the 8 KiB worker stack. After rebuilding, the reset test produced
an exact 2,960-byte orphan. Reboot reported `PASS - COMPLETE INDEX ORPHAN`,
while the journal remained at 36 records / transaction 36. No checkpoint,
cleanup, overwrite or repair occurred. The physical partial-index branch is
still pending.

## Checkpoint commit closure

`sticky-checkpoint-commit-test` continues only from the exact physical state
above. Boot first probes the 36-record / transaction-36 journal and verifies
the immutable 2,960-byte `IDXPWR01.BIN` snapshot. It performs no automatic
write. After a review touch and a separate commit touch, it appends one `PJCP`
frame as transaction 37 whose payload contains:

- snapshot transaction 36;
- journal segment 1 and 45 live records;
- the exact index byte length and full-file CRC;
- filename `IDXPWR01.BIN`.

The frame transaction is deliberately newer than the snapshot it commits.
After append and full journal replay, success requires 37 records, last
transaction 37, a checkpoint last frame and an exact payload CRC. A subsequent
reboot must repeat the read-only index verification and report the same PASS
state.

The 2026-09-29 physical run appended exactly one checkpoint: the worker
reported `changed=1`, payload CRC `F00C6724`, 37 records and last transaction
37. A controlled reset then repeated the read-only journal and exact-index
probes; the device again displayed `PASS - CHECKPOINT COMMITTED`, journal
37 / transaction 37 and index 2,960 bytes. No second append or index rewrite
occurred.
