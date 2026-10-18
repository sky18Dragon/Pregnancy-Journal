# Hardware regression checklist

Run this checklist on a Seeed Studio reTerminal Sticky after flashing the
current `sticky-release` image. Host tests and a successful build do not replace
these physical checks. Record the date, firmware commit and serial port with
the result.

## Boot and device services

- [ ] Cold boot reaches `phase=ready` without panic or watchdog reset.
- [ ] RTC, battery gauge, SSD1677 e-paper, GT911 touch, IMU, buttons and buzzer
  initialize in the serial log.
- [ ] Device time survives a power cycle and invalid time input is rejected.
- [ ] English and Simplified Chinese glyphs are legible; launcher language
  toggle updates the visible copy.

## Launcher and navigation

- [ ] Top-button single click opens and closes Launcher.
- [ ] Bottom-edge upward swipe opens it; downward swipe from the lower 30%
  closes it.
- [ ] Bottom-edge swipe left/right (Launcher closed) switches to the next/previous
  app and wraps around; a diagonal or short swipe, or one starting mid-screen,
  does nothing; taps near the footer buttons still work; no accidental switches
  while tapping the kick counter or typing digits.
- [ ] Header action labels: APPS on the Weight dashboard, Kicks Today and Settings
  main (opens Launcher); BACK goes up one level (Kicks Feel check-in → Record
  type; Compare → the page it came from); CANCEL abandons an entry.
- [ ] Double-click returns to Baby Week from every app.
- [ ] Baby Week, Weight, Kicks and Settings each open correctly; old Reminder/Checkup
  last-app states fall back to Baby Week without renumbering IDs.
- [ ] Launcher closes after inactivity and inactive apps do not receive touch.
- [ ] Selecting an app persists the last app across a normal deep-sleep wake.

## Baby Week

- [ ] First use accepts `YYYYMMDDHHMM` device time; tapping a field (year, month,
  day, hour, minute) edits only that field, a completed field moves on, an
  impossible digit (month 13, hour 25) is refused, and DEL clears the field
  before stepping back. The same holds for Settings device time and the due
  date / last-period entry.
- [ ] Confirmed due date setup saves and restores.
- [ ] LMP setup saves and restores.
- [ ] Dashboard shows week/day, trimester, due-date countdown and 40-week
  progress; overdue dates remain readable.
- [ ] Baby and Mom tabs show the expected offline content for the current week.

## V1 applications and deferred data

- [ ] Reminder/Checkup UI, business timer wake and alarm paths stay disabled;
  old source records and IDs remain intact.
- [ ] Weight create/edit supports kg/lb and 7/30 calendar-day filters, including
  missing days, unsorted imports and leap dates. No normal/abnormal gain label.
- [ ] History pages select the correct dated record; same-day replacement and
  deletion require confirmation; latest value/chart update after success only.
- [ ] Weight 366-record limit rejects additions without loss; updates still work.
  Height and baseline restore consistently, including interrupted preference saves.
- [ ] Kicks honors 400 ms (not five seconds), undo and cancel; a 60-minute timeout
  saves once, with the session's original date/time retained across midnight.
- [ ] Failed/incomplete Kicks commits retain a draft and reconcile the same ID on
  retry. A switched card cannot accept the old draft or display old statistics.
- [ ] Today/seven-day summaries show actual recorded data without extrapolation.
  At 42 sessions new sessions fail explicitly; no automatic overwrite.
- [ ] Ten partial Kicks updates trigger cleanup (limit 10 since 2026-10-18), with
  acceptable legibility and responsive touch after repeated taps.
- [ ] Pregnancy home <-> baby page (partial-refresh trial): switch back and forth
  more than ten times; the eleventh change is promoted to a full refresh, ghosting
  stays acceptable, and about 5 s after the last touch one full cleanup refresh
  runs. Home <-> Mom and the launcher round trip still use the full refresh.
- [ ] Entering the pregnancy home from the launcher shows the home page after one
  full refresh with no "LOADING FROM MICROSD" page; after removing or swapping the
  card the page falls back to the storage message or the cold read.
- [ ] Kicks import scans 42 slots read-only, survives interruption and re-import,
  detects damage/conflicts and blocks cleanup until readback on the same card.
- [ ] Common cleanup preserves reminder/checkup/Kicks source namespaces;
  independently confirmed Kicks cleanup removes only its verified source.
- [ ] Validate copied/restored `PREGJNL` using the backup tool; preserve unrelated
  card files and explain device-bound credentials before Device Reset.
- [ ] Record storage-worker free stack and internal/PSRAM free heap with the full
  366-weight + 42-session + legacy dataset. Measure long-journal commit/reboot time.
- [ ] Review the inherited health-information wording in both languages before release.

## Display and power

- [ ] Full and partial refresh complete without timeout.
- [ ] Repeated app changes trigger cleanup refresh without unacceptable ghosting.
- [ ] Settings clean refresh removes residual ghosting.
- [ ] Stable pages enter deep sleep after 60 seconds without external power;
  Settings uses three minutes and editors/active kick counting stay awake.
- [ ] Top-button wake restores the app and the e-paper image remains visible.
- [ ] A two-second side-button chord enters sleep; charge/no-charge GPIO levels
  match the hardware baseline.

## MicroSD lifecycle

- [ ] Settings → MicroSD shows only card/access/data status, free space, the
  initialize/safe-eject button and MORE. MORE opens the old-data import rows
  (greyed out and not tappable without a usable dataset), delete card data and
  reset device binding; Back from the confirmations and the import report
  returns to MORE.

Evidence below is from the 2026-09-27 through 2026-09-29 single-card runs with
`sticky-debug` and isolated test firmware; repeat it with `sticky-release` and
the full card matrix before release.

- [x] Cold boot detects and mounts the inserted card without formatting it.
- [x] Hot removal revokes access, unmounts, restores CS and powers the card off
  without resetting or blocking the UI.
- [x] Reinserting the same card mounts it with a new runtime generation.
- [x] Side-button sleep safely unmounts the card before display/device shutdown.
- [x] Top-button EXT1 wake restarts the worker, remounts the card and restores
  the main UI.
- [x] The updated debug worker mounts the existing card, reports
  `manifest=not_initialized access=read_only_probe`, creates nothing and
  continues to the ready UI with touch and IMU running.
- [x] With the bounded manifest queue firmware, normal boot and sleep/wake still
  create no files when no explicit request is submitted.
- [x] Settings shows the writable/uninitialized state, requires a separate
  confirmation page, and creates the card dataset only after the final user
  action. The 2026-09-28 physical run completed successfully with normal UI.
- [x] Sleep/wake after initialization reopens the same valid dataset, reports
  the manifest as ready and keeps repeated initialization disabled.
- [x] The isolated next-generation commit test changes generation 1 / slot A to
  generation 2 / slot B and reports PASS after readback verification. Confirmed
  on the initialized card on 2026-09-28.
- [x] After reflashing the normal debug firmware, sleep/wake remounts the card
  and Settings still reports the generation-2 dataset as ready.
- [x] The isolated journal test completed a 0–16 control run, then received an
  unsafe `power_on` reset during the second write window. Recovery published
  only the complete prefix through transaction 26 (`3952` valid bytes) and no
  transaction from 27 through 32. This reset landed on a frame boundary; the
  physical truncated-tail branch remains pending.
- [x] The maximum-frame follow-up reset after transaction 36 was submitted but
  before its completion reached the controller. Reboot recovery classified it
  as a complete commit: Ready, 36 frames, `6048` file/valid bytes and last
  transaction 36. The card now contains 45 live test references; no more test
  appends are allowed on this dataset.
- [ ] Optional stress coverage: cut power during an actual partial journal
  frame and confirm the same result as the deterministic test below. This is
  no longer a release gate because the exact persisted tail state has been
  exercised on real media.
- [x] Run the deterministic `sticky-journal-partial-test`, confirm the
  172/344-byte transaction-8 prefix is synced on the fresh-card baseline,
  reset, and require 2,580 file bytes, 2,408 valid bytes, 7 records, last
  transaction 7 and
  `PASS - TRUNCATED TAIL ISOLATED` with later writes blocked. Confirmed on
  2026-09-30; the first read exposed and led to a fix for omitted aggregate
  prefix counts on the `TailTruncated` path, after which the unchanged card
  state passed.
- [x] Reset after the 1,024-byte object reached media but before any journal or
  index mutation. Reboot reported `PASS - COMPLETE ORPHAN ISOLATED`, kept the
  journal at 36 records / transaction 36, and left the exact object unreferenced
  without cleanup or repair (2026-09-29).
- [x] Run `sticky-object-partial-test`, confirm the controlled 512/1,024-byte
  prefix reaches `PARTIAL FILE SYNCED - RESET NOW`, reset the device, and
  require `PASS - PARTIAL ORPHAN ISOLATED`, 512 bytes and an unchanged 37/37
  journal without repair or extension. Completed on 2026-09-29; the photographed
  reboot screen showed all expected values and no automatic repair.
- [x] Reset after the 2,960-byte, 45-entry index snapshot reached media but
  before any checkpoint commit. Reboot reported `PASS - COMPLETE INDEX ORPHAN`,
  kept the journal at 36 records / transaction 36 and preserved the exact
  unreferenced index without cleanup or repair (2026-09-29).
- [x] Run `sticky-index-partial-test` on the 37 / 37 baseline, wait for
  `PARTIAL INDEX SYNCED - RESET NOW`, reset, and require a 1,480-byte
  `IDXPWR02.BIN`, `PASS - PARTIAL INDEX ORPHAN` and unchanged 37 / 37 journal.
  Completed on 2026-09-29; all four values were confirmed on the reboot screen.
- [x] From that exact 36 / 36 baseline, review and commit checkpoint transaction
  37 referencing snapshot transaction 36; require 37 / 37 and a matching index
  on the completion screen, then reset and require the same PASS state again.
  Completed on 2026-09-29: the initial append returned `changed=1`; after a
  controlled reset the screen again reported PASS, 37 / 37 and a 2,960-byte
  index without another append.
- [x] Boot the read-only storage status profile against that committed
  checkpoint and require the normal worker recovery path to publish
  `CHECKPOINT READY`, journal/last transaction 37, snapshot transaction 36,
  45 live records, 45 verified object references and `VIEW: READY`. Completed
  on 2026-09-29; serial output and the photographed screen agreed, and the
  profile did not create or modify files. The first attempt exposed a recovery
  workspace stack temporary and the corrected in-place clear passed after
  reflashing.
- [ ] Random resets during the underlying object/index writes remain optional
  destructive-stress coverage; both deterministic short-file recovery branches
  have passed on hardware.
- [ ] Read-only media is detected and user-data mutations are rejected.
- [x] Mount/unmount fault injection leaves the card in Fault, blocks unsafe
  sleep and recovers through the bounded retry path.
  - [x] 2026-09-28 unmount-failure half passed: the first injected failure
    entered Fault and blocked sleep; bounded cleanup then unmounted and
    remounted the same dataset, and the following sleep attempt succeeded.
  - [x] 2026-09-28 mount-failure half passed: generation 1 failed before the
    board mount, cleanup cleared the unmounted Fault, and the automatic retry
    mounted the same card as generation 2 with `result=recovered`.
- [ ] Repeat with the supported multi-card/capacity matrix and release profile.

## Scheduled wake and fault cases

- [ ] Daily 03:00 maintenance and future app-system events determine the wake
  deadline; old reminder/checkup data never contributes a business wake deadline.
- [ ] Timer wake up to 15 seconds early never starts a business alarm or buzzer.
- [ ] Daily maintenance returns to Baby Week in its scheduled window; button
  wake retains the expected app and global navigation remains responsive.
- [ ] RTC or battery unavailable states remain navigable.
- [ ] Corrupt/removed/read-only MicroSD states block user-data writes without
  Flash fallback; verified legacy migration remains retryable.

## Verification record

- Firmware: `2.0.0`, four V1 apps; candidate only, not accepted for release.
- Current release SHA-256:
  `7bc9b30ba742f28f5e45880347aef7cfaf89f4fc4d38e20e070c24f14f3daba1`.
- Host suite: 76/76 C++ plus 8 Python tests; three build configurations passed.
- Current package: `dist/v1-kickfix-20261010/`; the dirty workspace includes
  uncommitted files, so Git HEAD alone cannot identify this candidate.
- Confirmed core flows: pregnancy save/return, weight draft preservation/save/history,
  kicks counter preservation/save/today summary, and post-deep-sleep recovery.
  The kicks stack overflow was reproduced, fixed and retested; runtime stack
  margin at completion was 4,096 bytes. Earlier storage and initialization
  failures remain documented as historical evidence.
- See [V1 hardware results](../V1-HARDWARE-RESULTS.md) for per-version observations,
  current acceptance mapping and remaining gates. Keep checklist items open if
  only part of their scenario has passed. Historical S1 checks above retain
  their original date/profile and do not count as current release validation.
- Record firmware hash, card identity/capacity/dataset, power source, serial
  port, steps, result, log and screen evidence for every physical run.
