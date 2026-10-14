# S1 MicroSD backup and recovery

This procedure copies and validates the complete application dataset without
changing the source card. It does not repair a damaged filesystem, merge two
datasets, format media or migrate a newer schema.

## What a complete backup contains

The backup unit is the whole `PREGJNL` directory. Copying only `AUDIO`, only
the newest manifest or only journal files is not a recoverable backup. Keep all
managed directories, including empty directories and `TMP` leftovers:

```text
PREGJNL/
  MANIFEST/
  JOURNAL/
  RECORDS/
  AUDIO/
  TASKS/
  INDEX/
  TMP/
```

Files elsewhere on the card are not owned by this application and must not be
deleted or moved by a backup or recovery operation.

## Create and validate a backup

1. On the device, request **safe MicroSD removal** and wait until it reports
   that writes are drained and the card is unmounted. If safe removal fails,
   do not claim that the copy is a consistent backup.
2. Remove the card and mount it on a computer using a card reader.
3. Copy the complete `PREGJNL` directory to a new timestamped directory. Keep
   the source card unchanged until the copy has validated and another copy
   exists if the data is important.
4. From the repository root, validate either the copied `PREGJNL` directory or
   its parent directory:

   ```sh
   python3 tools/validate_sd_backup.py "/path/to/backup/PREGJNL"
   ```

   Automation can request JSON output:

   ```sh
   python3 tools/validate_sd_backup.py --json "/path/to/backup"
   ```

Exit status `0` and a leading `VALID` line mean that the selected manifest,
dataset identity, journal segments, transaction/checkpoint schemas and every
committed record/index reference are internally consistent. Referenced `PJIX`
snapshots are also checked for dataset/transaction identity, bounds, ordering,
live count and both CRCs. A single manifest
slot is accepted as `degraded` and reported as a warning; the remaining valid
slot still identifies the dataset.

Exit status `1` means the copy must not be restored as a trusted dataset. The
validator rejects, among other cases:

- unknown manifest, journal, transaction or checkpoint schema;
- manifest slot/parity errors, non-adjacent generations or mixed dataset IDs;
- unknown journal entries, segment gaps, oversized segments or truncated
  tails;
- non-monotonic transactions, cross-dataset frames or CRC failures;
- invalid mutation/revision data; and
- missing, resized or checksum-mismatched committed record/index files.

The tool is read-only. It never truncates a tail, deletes an orphan, rewrites a
manifest or formats a card.

## Restore

1. Validate the backup before copying it.
2. Use a supported, already formatted card. Do not ask the firmware or this
   tool to format it.
3. The destination must not contain another `PREGJNL` dataset. Never merge
   directory contents from different dataset IDs. Preserve an existing
   destination dataset as a separate backup before replacing it.
4. Copy the complete validated `PREGJNL` directory to the card, flush/eject the
   card through the operating system, then insert it into the powered-off
   device.
5. After boot, confirm that the settings page reports the same dataset ID and
   selected manifest generation. Network credentials are device-bound; a card
   restored to another device requires explicit Wi-Fi and provider
   re-authorization. Archived tasks must not be automatically re-submitted.

If validation fails, retain an untouched copy and use computer filesystem
diagnostics on another copy. The device must remain fail-closed and must not
automatically format or guess around damaged/unknown data.
