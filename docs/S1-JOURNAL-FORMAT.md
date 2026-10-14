# S1 transaction journal format

This document freezes the framing, business payload, segmentation and file
transaction layers for the MicroSD recovery journal. Journal requests are
admitted through the single storage worker with card generation and dataset
isolation. No current product repository writes business data to this journal
until S1 recovery validation is complete; existing product records therefore
remain in NVS during this stage.

## Physical location

The reserved first segment is:

```text
/sdcard/PREGJNL/JOURNAL/TXN00001.LOG
```

`JOURNAL` and `TXN00001.LOG` are uppercase ASCII 8.3 names. Segments are named
contiguously from `TXN00001.LOG` through `TXN99999.LOG`, are limited to 256 KiB
each, and roll before appending a frame that would cross that limit. A gap,
empty segment, oversized segment or unknown entry blocks replay. A checkpoint
is due after 128 records.

## Frame v1

Every integer is unsigned little-endian. The fixed header is 72 bytes and the
opaque domain payload is limited to 1,024 bytes, so one frame is at most 1,096
bytes.

| Offset | Bytes | Field | Rule |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | ASCII `PJTL` |
| 4 | 2 | schema | `1` |
| 6 | 2 | header length | `72` |
| 8 | 4 | total length | header plus payload |
| 12 | 4 | payload length | `0..1024` |
| 16 | 1 | record type | commit `1`, tombstone `2`, checkpoint `3` |
| 17 | 7 | reserved | all zero |
| 24 | 8 | transaction ID | nonzero and strictly increasing within a segment |
| 32 | 32 | dataset ID | lowercase hexadecimal; no terminator stored |
| 64 | 4 | payload CRC32 | CRC32/IEEE over the payload |
| 68 | 4 | frame CRC32 | CRC32/IEEE over header bytes `0..67`, then payload |
| 72 | N | payload | versioned `PJTX` transaction or `PJCP` checkpoint |

The frame checksum deliberately excludes its own four-byte field. The payload
checksum allows a caller to identify payload damage independently, while the
frame checksum also protects lengths, type, transaction ID and dataset ID.
Worker append admission decodes the payload before file I/O: commit frames must
contain at least one upsert, tombstones contain only deletes, and a checkpoint's
`last_transaction_id` names the immediately preceding visible transaction and
is lower than its enclosing checkpoint frame transaction ID.

## Recovery rules

1. Obtain the expected dataset ID from the selected valid manifest; never infer
   it from the journal alone.
2. Scan frames in order and expose only complete frames whose lengths, schema,
   type, dataset ID and both checksums validate.
3. Transaction IDs may have gaps but must increase. A duplicate or lower ID is
   a blocking error, not an idempotent replay signal.
4. A final partial magic/header/payload is `TailTruncated`. The scanner reports
   the exact valid-prefix byte count and never delivers that partial frame.
5. A complete frame with a bad checksum, an invalid middle frame, another
   dataset, unknown schema or unknown type blocks recovery and future writes.
   The codec does not skip forward looking for another magic value.
6. A consumer may reject a valid frame before it becomes visible. This is kept
   separate from media corruption so domain validation failures are explicit.
7. The codec never truncates, repairs, formats or deletes files. A future worker
   recovery request must require an explicit policy and preserve backup data.

## File transaction boundary

`sd_journal_file.*` owns the bounded file operation used by the future storage
worker. A probe streams one frame at a time through a fixed 1,096-byte buffer;
it does not load the segment into RAM. Missing and empty segments remain
distinct from I/O failure. The probe preserves the valid prefix and last valid
record summary while reporting a truncated tail, corruption, unknown schema or
type, dataset mismatch, or a non-monotonic transaction.

Appending follows this order:

1. Encode the requested canonical frame and scan the existing segment.
2. Refuse to write across any damaged or truncated tail.
3. Treat an exact retry of the last verified frame as success without writing;
   reject the same transaction ID with different content as a conflict.
4. Create only the fixed application and journal directories when needed, open
   the fixed segment in append mode, write the complete frame, flush, `fsync`
   and close it.
5. Stream the segment again and return success only if its byte count, record
   count and last frame identity match the requested append.

An open, short-write, sync or close failure is reported separately. A short
write may leave a recognizable truncated tail; the next request is blocked
instead of appending past it. A sync or close failure has an uncertain outcome,
so retry first scans the file: if the exact frame is already complete, the
retry succeeds idempotently without a duplicate. This layer never silently
repairs or truncates a failed append.

## Current verification

`sd_journal_test` covers complete replay, zero- and maximum-sized payloads,
every possible final-frame truncation point, corrupt payload CRC, bad frame CRC,
unknown schema/type, dataset mismatch, non-monotonic transaction IDs, invalid
arguments and consumer rejection. `sd_journal_file_test` adds missing/empty
segments, streaming real-filesystem replay, verified append, exact retry,
transaction conflicts, directory/open/short-write/sync/close/readback failures,
and refusal to append after a truncated tail. `sd_repository_recovery_test`
adds transaction-level atomic replay, exact retry, revision conflict,
tombstone/late-result isolation, capacity and checkpoint-count validation.
The full host suite passes 59/59. On 2026-09-29, an isolated-device control run
persisted transactions 1 through 16. A second run was reset unsafely during its
write window; reboot recovery reported Ready with 26 complete records,
`3952` total and valid bytes, and transaction 26 as the last visible record.
Transactions 27 through 32 were not exposed. The reset landed between complete
frames, so this is physical evidence for valid-prefix recovery and transaction
visibility boundaries, not yet for the `TailTruncated` branch. A real partial
frame and the object/index power-interruption stages remain pending.

A follow-up used 344-byte canonical transaction frames with four references
and no intentional inter-frame delay. Transactions 34 and 35 completed; reset
occurred after transaction 36 was submitted but before its completion was
delivered to the controller. Reboot found transaction 36 complete and reported
Ready with 36 frames and `6048` total/valid bytes. This covers the uncertain
completion case on real media: recovery, rather than the lost in-memory result,
decides whether the transaction committed. It still did not produce a partial
physical tail.

The object-before-journal follow-up wrote the deterministic 1,024-byte
`RECORDS/OBJPWR01.BIN` fixture and received an unsafe reset before any journal
or recovery-index mutation. Reboot readback found the object exact while the
journal remained at 36 records and transaction 36, then reported
`PASS - COMPLETE ORPHAN ISOLATED`. No automatic cleanup, overwrite or repair
was attempted. This is target evidence for the complete-object/uncommitted
transaction branch; a physical partial-object write and index interruption are
still pending.

The read-only computer validator and backup/restore procedure are documented in
[S1 MicroSD backup and recovery](S1-BACKUP-RECOVERY.md).

## Remaining integration

- Run physical removal/power-loss tests and recovery-policy tests on disposable
  card data before allowing a business repository to write real card data.
