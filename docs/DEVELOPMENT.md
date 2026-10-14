# Development workflow

## Toolchain

- PlatformIO 6.x
- `espressif32@6.11.0`
- ESP-IDF 5.4.1 framework package
- C++17
- Python 3 and Pillow 11.3.0 for deterministic image generation
- Target board: `sticky_esp32s3`, ESP32-S3, 32 MB flash, 8 MB octal PSRAM

Install the asset dependency only when needed:

```bash
python3 -m pip install -r requirements-dev.txt
```

## Build profiles

```bash
pio run -e sticky-release
pio run -e sticky-debug
pio run -e sticky-power-test
```

`sticky-release` is the default production profile. `sticky-debug` enables
diagnostic logging; `sticky-power-test` enables the power-test behavior. The
optional `sticky-font-test` profiles render glyph coverage screens.

`sticky-chinese-input-test` runs the isolated S0 Chinese input page with real
touch handling; `sticky-chinese-input-test-portrait` rotates the same renderer
and converts physical touch coordinates to its 480×800 logical axes. Neither
profile persists entered text: Save only changes its label and Cancel clears the
in-memory test state. The current ranking-acceptance seed is `排序验收：` with
`yun` already entered; `孕` must appear on the first candidate page. See the
[S0 Pinyin resource prototype](S0-PINYIN-RESOURCE-PROTOTYPE.md) for the exact
test sequence and current evidence.

`sticky-sd-audio-test` is an isolated S0 hardware prototype, not production
firmware. See [SD/audio test procedure](S0-SD-AUDIO-TEST.md) before flashing it.

`sticky-sd-display-test` exercises display refresh before/after SD unmount
without recording audio or writing test files. Confirm the final screen
physically; a successful driver return alone is not visual acceptance.

`sticky-manifest-commit-test` is an isolated S1 write-path test. It waits for a
previously initialized dataset and requires two touches before writing exactly
the next generation to the inactive manifest slot. Read
[the manifest commit test procedure](S1-MANIFEST-COMMIT-TEST.md) before flashing
it; every successful run advances the on-card generation.

`sticky-journal-power-test` is the isolated S1 journal power-loss test. Boot
performs a read-only journal probe and never writes automatically. After two
separate confirmations, the storage owner first creates and verifies the
immutable `RECORDS/PWRTEST.BIN` fixture, then the user must explicitly start a
bounded loop of at most 7 maximum-size canonical journal commits. A write-start
audio cue follows the static screen refresh, and the commits then run without
an intentional inter-frame delay so a reset is more likely to overlap real
storage I/O. On the
next boot, a complete journal remains `Ready`; a partial final frame is
reported as `TailTruncated`, preserves its valid prefix, blocks later writes
and is never repaired automatically. The worst-case baseline gate prevents a
repeated run from exceeding the 64-slot recovery index. Use only a disposable
test dataset.

`sticky-journal-partial-test` is the deterministic final-tail variant for the
fresh-card 7-record / transaction-7 / 2,408-byte baseline. After two explicit
touches it appends, flushes, `fsync`s and closes only 172 of the 344 bytes in
the canonical transaction-8 frame. Reboot must report `TailTruncated`, file
size 2,580 bytes, valid prefix 2,408 bytes, 7 records and last transaction 7.
The profile never truncates, repairs or resumes this journal; all future
writes to this test dataset remain blocked.

`sticky-object-power-test` isolates the object-write stage without appending a
journal transaction or changing the recovery index. Boot first probes the
existing journal and the fixed `RECORDS/OBJPWR01.BIN` fixture read-only. If the
object is missing, two separate touches expose and start one 1,024-byte write;
the audio cue marks the point at which a non-safe reset should be applied. On
the next boot an exact object is reported as a complete isolated orphan and a
short/mismatched object as a partial isolated orphan. Neither result is made
visible through the journal, overwritten, deleted or repaired. Use only a
disposable test card; once either orphan exists this test intentionally refuses
to write it again.

`sticky-object-partial-test` uses a separate `RECORDS/OBJPWR02.BIN` fixture to
exercise the exact short-object recovery branch deterministically. After two
touch confirmations the storage worker writes, flushes, `fsync`s and closes
only the first 512 bytes of the known 1,024-byte payload, verifies the prefix
CRC, then stops on a persistent reset instruction. Reboot must report
`PASS - PARTIAL ORPHAN ISOLATED`, 512 bytes, and an unchanged journal. The
profile refuses any pre-existing fixture and never repairs, deletes, extends
or references the short object.

`sticky-index-power-test` isolates the index-before-checkpoint stage. It first
requires the known 36-record / transaction-36 journal baseline, then probes the
fixed `INDEX/IDXPWR01.BIN` candidate read-only. Two touches start one 2,960-byte
immutable snapshot write; the profile never appends a checkpoint. Reset at the
audio cue, then verify that reboot reports a complete or partial index orphan
while the journal remains 36 / 36. See the
[S1 index snapshot](S1-INDEX-SNAPSHOT.md) for format and safety rules.

`sticky-index-partial-test` targets the later 37 / 37 checkpoint baseline and
a separate `INDEX/IDXPWR02.BIN` fixture. After two confirmations the worker
persists, flushes, `fsync`s and closes exactly 1,480 of the generated 2,960
snapshot bytes, validates that prefix by size and CRC, and holds a persistent
reset instruction. Reboot must report `PASS - PARTIAL INDEX ORPHAN`, 1,480
bytes and an unchanged 37 / 37 journal. The committed `IDXPWR01.BIN` remains
authoritative through checkpoint 37; the short candidate is never selected,
extended, deleted or repaired.

`sticky-checkpoint-commit-test` closes that exact orphan into a committed
recovery point. It accepts only the 36 / 36 journal and exact 2,960-byte
`IDXPWR01.BIN` fixture. One touch opens a review page and a second, separate
touch appends checkpoint transaction 37; the checkpoint payload identifies the
snapshot through transaction 36. The profile never rewrites the index. After
`PASS`, reset once and require the same PASS state with journal 37 / 37 before
recording physical acceptance. Use only the designated disposable test card.

The S1 storage diagnostics are isolated from production builds:

- `sticky-storage-status-test` mounts and reports card, manifest, capacity,
  audio inventory and a one-byte business-write policy check. It also executes
  the formal read-only startup recovery chain and displays journal/snapshot
  transaction IDs, live records, verified object references and whether the
  bounded view is ready. It does not create or modify files.
- `sticky-storage-read-only-test` uses the same read-only diagnostic while
  injecting `ReadyReadOnly` after a real mount. The physical filesystem is not
  remounted or probed by writing; the expected policy result is `CardReadOnly`.
- `sticky-storage-full-test` reports the real total capacity but injects zero
  free bytes. The expected state is `Full` and policy result is `NoSpace`; it
  does not fill the card.
- `sticky-storage-unmount-fault-test` runs the normal debug UI but makes the
  first unmount call return `ESP_FAIL` before touching the board driver. A sleep
  attempt must be blocked in `Fault`, after which the bounded cleanup retry
  performs the real unmount.
- `sticky-storage-mount-fault-test` makes the first mount call return
  `ESP_FAIL` before powering or mounting the card. The lifecycle must enter
  `Fault`, clear the unmounted failure safely and retry a real mount with a new
  generation. These compile-time hooks are absent from release, debug and
  power-test profiles.

`sticky-voice-classifier-test` is an isolated S0 hardware prototype. Its
PlatformIO post script generates and validates the ESP-SR model image, then
uploads it at the address declared by `partitions.voice.csv`. It does not mount
MicroSD, save audio or use the network. Review the current thresholds, resource
evidence and remaining hardware matrix in
[the S0 voice progress record](S0-VOICE-CLASSIFIER-PROGRESS.md) before using it.

The normal firmware now starts the MicroSD storage worker. It detects and
mounts supported media without formatting it, handles removal/cleanup, and must
unmount successfully before the coordinator enters deep sleep. Boot probes the
two manifest slots read-only and never creates missing files. Settings can
explicitly initialize an uninitialized card only after a separate confirmation
page. A successful manifest request refreshes the bounded recovery view before
publishing the selected manifest and completing initialization, so an empty
dataset can be used immediately without remounting. Current business repositories
submit through the asynchronous SD adapter,
and Settings owns
the explicit verified legacy migration, safe-eject and bounded delete flows.
There is no user-record fallback to NVS.

PlatformIO rejects some project paths containing spaces. This repository's
workspace name contains spaces, so build an exact temporary mirror rather than
renaming or moving the user's checkout:

```bash
work=/private/tmp/reterminal_sticky_build
mkdir -p "$work"
rsync -a --delete --exclude .git --exclude .pio ./ "$work/"
cd "$work"
pio run -e sticky-release
```

## Fast verification

Run these for documentation or hardware-independent changes:

```bash
python3 tools/check_markdown_links.py
./tools/run_host_tests.sh
```

Host tests compile pure C++ modules with C++17, warnings enabled and warnings as
errors. Add a focused test under `test/` and register it in
`tools/run_host_tests.sh`.

For firmware changes, the release gate is all host tests plus all three build
profiles. GitHub Actions performs the Markdown link check, host suite and a
three-profile PlatformIO matrix.

## Flash and monitor

```bash
pio device list
pio run -e sticky-release -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --filter time --rts 0 --dtr 0
```

Use the port returned by `pio device list`. If automatic download mode fails,
hold BOOT, tap RESET, release BOOT and retry. Flashing replaces the firmware and
can make existing local NVS data incompatible with a schema change, so record
migration expectations before device testing.

## Source map

- `src/app/`: registry, lifecycle, routing, orientation and sleep coordinator.
- `src/apps/`: page state and interaction for the four V1 apps; deferred app
  sources remain available for V2.
- `src/pregnancy/`: domain models, content, services and storage.
- `src/ui/`: Canvas, typography, localization and generated visual assets.
- `src/board/`, `devices/`, `display/`, `input/`, `sensors/`: hardware layers.
- `components/`: reusable GT911 and Seeed e-paper components.
- `assets/`: editable source artwork.
- `tools/`: generators, host test runner and documentation validation.
- `docs/`: current specifications, test evidence and historical refactor notes.

## Common change workflows

### UI or page

Update renderer and hit map together, add both orientations, update bilingual
strings/glyphs, run host tests, build firmware and validate refresh/ghosting on
the panel.

### Domain or persistence

Write the pure model/service rule first, add boundary and corruption tests,
then connect the app and storage adapter. Bump schema versions deliberately and
document migration/recovery.

### Assets

Edit source files in `assets/`, run the matching `tools/generate_*.py`, and
commit both source and generated `src/ui/assets/*`. Do not hand-edit generated
byte arrays.

The offline single-character Pinyin index is generated from the pinned,
licensed sources under `third_party/pinyin-data/`:

```bash
python3 tools/fetch_unihan_frequency.py
python3 tools/generate_pinyin_index.py
python3 tools/report_chinese_input_budget.py
python3 tools/fetch_chinese_input_font.py
python3 tools/generate_chinese_input_font_assets.py
```

The first command downloads the pinned Unicode 15.1 Unihan archive, verifies its
SHA-256 and regenerates the committed GB2312 Level-1 `kFrequency` extract. The
second command combines that extract with pinned readings and the editable
`assets/chinese-input/candidate-priority.tsv` source to regenerate
`src/ui/assets/pinyin_index_data.{h,cpp}`. The third reports the historical
planning estimate. The final two commands fetch
the pinned OFL font with SHA-256 verification and regenerate the committed
18/24 px `src/ui/assets/chinese_input_font_assets.cpp`; the downloaded source
font stays under ignored `tools/.cache/`. See the
[S0 Pinyin resource prototype](S0-PINYIN-RESOURCE-PROTOTYPE.md) before changing
coverage or candidate ordering.

### Hardware

Confirm pin/bus ownership in `HARDWARE-REFERENCE.md` and `src/board/pin_config.h`.
Record exact power source, profile, interaction, serial result and screen result.
Complete `pregnancy/hardware-regression-checklist.md` after flashing.

## Pull request completion

Include outcome, motivation, tests/builds run, real-device status and screenshots
for visible changes. Update README files, relevant detailed docs and
`CHANGELOG.md` when shipped behavior changes.
