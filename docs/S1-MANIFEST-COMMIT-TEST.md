# S1 manifest next-generation physical test

This isolated firmware verifies one real A/B manifest transition through the
same bounded storage worker used by the product firmware. It is not a product
screen and it does not migrate NVS records, create user records, record audio,
format the card or delete unrelated files.

## Safety boundary

- Boot and card discovery remain read-only.
- The test accepts only a mounted, writable card with a valid selected
  manifest.
- No write begins until **Review Write** and then **Write Next** are touched.
- Immediately before submission it revalidates the mount generation, dataset
  ID and current commit generation.
- It derives exactly the next generation for the same dataset and lets the
  storage worker write only the inactive A/B slot.
- The worker flushes, `fsync`s, closes, reads back and verifies both copies
  before reporting success. It does not retry automatically after failure.

## Build and flash

Because the workspace path contains spaces, use the no-space mirror described
in [Development workflow](DEVELOPMENT.md):

```bash
pio run -e sticky-manifest-commit-test
pio run -e sticky-manifest-commit-test -t upload \
  --upload-port /dev/cu.usbmodemXXXX
```

## Physical sequence

1. Keep the initialized card inserted and boot the test firmware.
2. Confirm the first page reports the expected current generation and active
   slot. The first initialized copy should normally be generation 1 in slot A.
3. Touch **Review Write**. Confirm the target is current generation plus one and
   the opposite slot. For the first transition this is generation 2, slot B.
4. Touch **Write Next** once and keep the card inserted.
5. Record the final **PASS** or **FAIL** screen. PASS requires the worker result,
   readback generation and written slot all to match.
6. Reflash `sticky-debug`, sleep and wake the device, then open Settings →
   MicroSD. The dataset must still report Ready and initialization must remain
   disabled.

Do not repeat this profile merely to obtain another PASS: each successful run
advances the manifest generation by one.

## Recorded result

The 2026-09-28 physical run passed on the initialized card. The user confirmed
generation 1 in slot A on the review page, submitted generation 2 to slot B
once, and observed **PASS**, generation 2, slot B after worker readback
verification. The test was not repeated. The normal `sticky-debug` firmware was
then restored; after sleep, wake and remount, Settings still reported the
dataset as ready and did not offer initialization again.
