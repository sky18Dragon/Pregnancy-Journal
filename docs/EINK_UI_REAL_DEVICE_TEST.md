# E-Ink UI real-device test matrix

## Build under test

- Firmware revision: optimized working tree, `sticky-release`
- Device: reTerminal Sticky, SSD1677, 800 x 480
- Test date: 2026-09-14 to 2026-09-15
- Tester: Codex automated build/flash; user-provided physical photographs

Build and flash record:

- Host tests: 25/25 passed, including all-app `FollowDevice` and battery-overlay
  contrast guards.
- Release build: 24,476 bytes RAM (7.5%), 4,105,244 bytes flash (48.9%).
- Landscape and portrait Font Test builds: 23,740 bytes RAM (7.2%),
  4,040,676/4,040,680 bytes flash (48.2%); binaries are
  4,041,072/4,041,088 bytes.
- Upload port: `/dev/cu.usbmodem5C843369951`.
- Esptool wrote the final 4,105,904-byte release image, verified the image
  hash, and hard-reset the device.
- Post-reset runtime sampling showed normal GT911 polling with `map=480x800`
  and no I2C errors.
- On 2026-09-15, esptool wrote the fixed-portrait Font Test image (4,040,640
  bytes), verified its hash, and hard-reset the device. The resulting
  straight-on photograph passed the typography and binary-geometry review.
- The fixed-landscape Font Test image was then written (4,040,640 bytes), its
  hash was verified, and the device hard-reset. Its photograph passed the
  800 x 480 layout, native-strike, and binary-geometry review.
- The production release image was restored after diagnostics and then
  reflashed after the final portrait countdown/heart alignment adjustment.
  Esptool wrote 4,105,952 bytes, verified the hash, and hard-reset the device.
  Runtime sampling showed continuous GT911 polling without I2C errors and
  `map=480x800`. The preceding production run settled from `face_up` to
  `landscape_0`, followed by successful full monochrome refreshes (`2022 ms`
  and `2000 ms`, both `ESP_OK`).
- After the final 48px touch-target correction, the release was flashed again
  and every written region passed esptool hash verification. The hard reset
  captured a complete cold boot from `rst:0x1 (POWERON)`. The running image
  reported ELF SHA prefix `0554646d6`, matching the local release ELF SHA-256
  `0554646d6f295317977d7d54cadc2ad08449a785f50e9435db1b4b875c89e798`.
  Pregnancy was restored as the current app and its full monochrome refresh
  completed in 2007 ms with `ESP_OK`; the log is archived below.

The three photographs supplied on 2026-09-14 establish the current-build baseline. They do not count as optimized-build acceptance evidence.

## Acceptance criteria

- 16px Regular Chinese body text is readable at normal handheld distance with no systematic missing horizontal/vertical strokes.
- 12–24px Font Test Screen samples show a predictable progression; Regular and Medium remain distinguishable.
- Mixed CJK/Latin/numeral rows share a stable baseline.
- 1px black rules remain continuous; 2px rules and icon strokes remain optically consistent.
- No meaningful text, border, or icon relies on pixels erased by final mono conversion.
- Portrait and landscape layouts have no clipping, overlap, unintended wrap, or off-center control labels.
- When partial-refresh endurance is in scope, repeated updates must not produce unacceptable ghosting or progressive stroke growth/loss. The operator explicitly skipped this case for the final acceptance run on 2026-09-15.
- Sleep and wake preserve the correct orientation and do not leave stale overlay pixels.

## Matrix

| Case | Procedure | Required evidence | Status |
| --- | --- | --- | --- |
| Cold Boot | Power-cycle or reset into the pregnancy dashboard. | Photo plus boot log showing selected orientation and full refresh. | Pass: the same monitored run captured `rst:0x1 (POWERON)`, ELF prefix `0554646d6`, pregnancy restoration, a 2007ms full refresh with `ESP_OK`, and the dashboard photograph indexed below |
| Full Refresh | Force a full monochrome refresh on Font Test Screen and dashboard. | Straight-on photos of both screens. | Pass: portrait/landscape Font Test and production dashboard photographs received; binary text and geometry survive the full mono refresh |
| Partial x10 | Trigger the same small update ten times without changing orientation. | Photos after update 1, 5, and 10; note ghosting/stroke changes. | Skipped by operator request on 2026-09-15; not a blocking acceptance item. Supplemental `67.3 / 67.7 / 68.2 kg` photos and successful partial-refresh logs are archived but are not claimed as the formal endurance test |
| Sleep | Enter deep sleep from a stable screen. | Photo of retained frame and sleep indicator. | Pass: portrait dashboard frame is retained cleanly and the crescent sleep indicator is present |
| Wake | Wake into the saved app. | Photo plus log; compare against pre-sleep orientation/layout. | Pass: user-labelled immediate wake photograph shows the restored portrait pregnancy dashboard without the sleep overlay, stale pixels, clipping, or orientation mismatch; the serial session re-enumerated the device and returned to the same firmware/dashboard runtime |
| Portrait | Hold both portrait directions until the IMU settles. | One photo per direction; confirm logical 480 x 800 layout. | Pass: both physical portrait grips render the same upright 480 x 800 dashboard hierarchy; no clipping, stale rotation pixels, or off-center content is visible |
| Landscape | Hold both landscape directions until the IMU settles. | One photo per direction; confirm logical 800 x 480 layout. | Pass: both physical landscape grips render upright 800 x 480 production layouts; dashboard and Weight editor remain unclipped and correctly aligned |

## Current-build baseline observations

| Page | Baseline finding |
| --- | --- |
| Portrait pregnancy dashboard | Large title and 1-bit illustration survive; DarkGray small text is broken; LightGray card/rules disappear; 14px body is weak. |
| Landscape kick dashboard | Main values and filled action survive; small subtitle and gray dividers are less stable; outline action is lighter. |
| Portrait launcher | 12px Medium summaries are dense/merged; titles and numbers are readable; footer is near the practical limit. |

## 2026-09-15 small-text follow-up

Later production photographs of the Kick Today and seven-day Trend pages
exposed a narrower typography defect not visible in the earlier Font Test
acceptance shots: 14–16px W3 Regular Chinese retained its bounding box and
baseline, but several interior horizontal and vertical segments were only one
final pixel wide and became incomplete on the panel. Headings, large numerals,
rules, and W6 Medium labels in the same frames remained intact, which rules out
layout clipping and a general refresh failure.

The system font generator now treats 12–18px as an optical-size family. Small
Regular strikes use the W6 source outline at threshold 176; Medium remains W6
at threshold 144, and larger Regular strikes remain W3 at threshold 128. This
keeps page geometry and text advances stable, adds no runtime scaling or pixel
dilation, and applies to every app using the shared font renderer. These new
strikes require a fresh real-device Font Test and Kick-page photo pass before
the previous small-text acceptance can be considered restored.

## Result log

Record for each failure: page, orientation, refresh count, ambient lighting, affected region, expected output, actual output, and photo filename. Do not approve based only on desktop previews.

2026-09-14 optimized rendering observations:

- Portrait launcher photo: 14px summaries are readable, 22px titles remain
  distinct, 1px row rules are continuous, and the earlier merged/missing-stroke
  failure is no longer present.
- Portrait dashboard photo: size heading, 14px caption, 16px body copy, rounded
  card outline, divider, and 2px icons all survive the panel conversion. The
  visual hierarchy matches the final-mono preview.
- These photos predate the final all-app `FollowDevice` patch but use the same
  font strikes and monochrome rendering path. They validate typography and the
  launcher/dashboard portrait compositions, not the new portrait layouts of
  Reminder, Checkup, Weight, Kicks, or Settings.

2026-09-15 portrait Font Test observations:

- 12px Regular and Medium remain identifiable, but 12px CJK is retained only
  as a diagnostic sample and is not approved for normal product copy.
- 14px Regular is acceptable for short labels and physically verified
  single-line compact card summaries; complex characters are visibly denser,
  so general paragraphs continue to start at 16px Regular.
- 16px Regular passes as the minimum CJK body size. 18px Regular is the
  preferred body setting and has the most balanced open counters/stem weight.
- Medium is clearly distinguishable at every sample size. 16px and above are
  robust for compact labels/headings without the old blurred gray edge.
- `孕馨醒餐检健康期`, numerals, upper/lower Latin and mixed `孕周 Ag12`
  baselines are readable. No systematic horizontal/vertical stroke loss or
  top/bottom clipping is visible.
- Both 1px and 2px rules are continuous and optically distinct. Both border
  weights close cleanly, and 16/20/24px icon circles retain their shapes.
- The photograph contains no EXIF orientation and its raw pixels were upside
  down; the archived evidence is rotated 180 degrees for review. This fixed
  diagnostic build does not exercise the production IMU orientation policy.

2026-09-15 landscape Font Test observations:

- The 800 x 480 composition reflows into two balanced columns with no clipping,
  collision, unintended wrap, or undersized edge margin.
- All seven Regular/Medium size pairs match the portrait weight progression;
  there is no orientation-specific raster distortion.
- Required CJK sentences, dense characters, numerals, uppercase/lowercase
  Latin, 1px/2px lines and borders, and 16/20/24px circles remain readable.
- The right-column 14px Regular samples confirm the policy decision: suitable
  for short diagnostic/secondary lines, while normal product paragraphs remain
  at 16px or 18px Regular.
- The raw JPEG contains no EXIF orientation and was rotated 90 degrees for the
  archived review image. The fixed diagnostic layout is not evidence for the
  production IMU settling policy by itself.

2026-09-15 restored-release dashboard observations:

- The production landscape photograph matches the intended asymmetric
  two-column composition: gestational status and the growth card remain on the
  left, while the week illustration and size comparison form the right focal
  area. Text, card edges, progress geometry, and icons are fully binary and
  show no clipping or overlap.
- The third landscape card summary wraps to two lines within its allocated row
  without crossing the card border. The other summaries retain clear icon/text
  alignment and consistent left edges.
- The portrait photograph retains the complete active dashboard composition
  after entering deep sleep. The crescent indicator is visible at the top
  right, with no stale overlay box or orientation mismatch, so the Sleep case
  passes.
- The portrait hierarchy remains stable in the smaller logical width: centered
  title/progress, dominant week illustration, size statement, and bottom growth
  card. The 14px compact card summaries are readable on this panel, validating
  their narrowly scoped exception to the 16px general-body rule.

2026-09-15 reverse-grip and partial-refresh observations:

- The newly supplied reverse-landscape dashboard photograph retains the same
  two-column grid, baseline alignment, binary card geometry, and safe edge
  margins as the first landscape direction. The compact battery treatment and
  menu action remain separated at the top right.
- The newly supplied reverse-portrait dashboard photograph is upright in the
  opposite physical grip. Its title, progress group, illustration, caption,
  and growth card retain their centered vertical rhythm without stale pixels
  from the previous orientation. Portrait intentionally reserves the top-right
  corner for the menu and does not draw the landscape-only compact battery
  component.
- Three Weight editor photographs captured at 65.2 kg, 63.6 kg, and 63.1 kg
  show a stable focal metric, consistent numeral baselines, equal-width 48px
  adjustment controls, and a clean full-width primary action. No accumulated
  ghosting or damaged strokes are visible across the three frames.
- The three earlier Weight values do not by themselves prove which photograph
  followed tap 1, 5, or 10. They remain useful visual samples, but the operator
  explicitly removed Partial x10 from the final acceptance scope.
- These photographs predate the final 40–44px to 48px compact-control correction.
  The dashboard and orientation code paths were unchanged, so their typography
  and rotation evidence remains applicable.

2026-09-15 final operator-labelled sleep/wake and boot observations:

- The monitored cold-boot run reports `POWERON`, the exact final ELF prefix,
  restored Pregnancy state, and a successful 2007ms full refresh. The dashboard
  photograph from that run is clean and correctly laid out.
- The operator identifies `wake-home-2026-09-15.jpg` as the immediate wake
  frame. It restores the portrait dashboard without the moon indicator and
  without stale sleep-overlay or rotated-frame pixels.
- The operator identifies `sleep-home-2026-09-15.jpg` as the sleep frame. The
  same dashboard is retained and the top-right battery plus crescent indicator
  are present; all content remains legible and aligned.
- The serial monitor observed the USB device disconnect during the power/sleep
  transition and reconnect to the same release image. The reconnect again
  reported ELF prefix `0554646d6` and completed the dashboard full refresh with
  `ESP_OK`.
- A later `67.3 / 67.7 / 68.2 kg` photo set is also archived. It is consistent
  with +0.1 changes after 1, 5, and 10 adjustments, while the display log
  reported successful partial sequences. Per the operator's explicit request,
  this remains supplemental evidence and Partial x10 is recorded as skipped.

Production restoration log excerpt:

```text
sticky_imu: placement settled from=face_up to=landscape_0
sticky_display: refresh_begin mode=monochrome_full
sticky_display: refresh_done mode=monochrome_full elapsed_ms=2022 result=ESP_OK
sticky_display: refresh_begin mode=monochrome_full
sticky_display: refresh_done mode=monochrome_full elapsed_ms=2000 result=ESP_OK
GT911: poll addr=0x14 status=0x00 map=480x800
```

## Physical acceptance closure

Cold Boot, Full Refresh, Sleep, Wake, Portrait, and Landscape are now backed by
real-panel photographs; the relevant boot/runtime output is archived. Partial
x10 was explicitly skipped by the operator and is not a blocking acceptance
item for this delivery. No further physical capture is required under the
revised scope.

## Software evidence

The serial evidence below is historical (2026-09-15), not acceptance evidence
for current firmware. Curated text copies are tracked in Git; device identifiers
and personal pregnancy/weight values have been redacted. Raw local `.log` files
remain excluded by the repository log policy.

| Artifact | Purpose |
| --- | --- |
| [Current portrait dashboard](images/eink-ui/pregnancy-dashboard-portrait-current-mono.png) | Baseline final-mono simulation matching the original device-photo defects. |
| [Optimized portrait dashboard](images/eink-ui/pregnancy-dashboard-portrait-optimized.png) | Final-mono layout and text-stroke check. |
| [Optimized landscape dashboard](images/eink-ui/pregnancy-dashboard-landscape-optimized.png) | Landscape reflow and clipping check. |
| [Current portrait launcher](images/eink-ui/launcher-portrait-current-mono.png) | Baseline final-mono launcher comparison. |
| [Optimized portrait launcher](images/eink-ui/launcher-portrait-optimized.png) | Portrait row hierarchy and divider check. |
| [Optimized landscape launcher](images/eink-ui/launcher-landscape-optimized.png) | Landscape row hierarchy and divider check. |
| [Font test portrait](images/eink-ui/font-test-portrait.png) | Native-strike, baseline, geometry, and icon test. |
| [Font test landscape](images/eink-ui/font-test-landscape.png) | Same test in the 800 x 480 composition. |
| [Settings portrait](images/eink-ui/settings-main-portrait-optimized.png) | Responsive settings rows and values. |
| [Settings time editor portrait](images/eink-ui/settings-time-portrait-optimized.png) | Responsive keypad and input layout. |
| [Pregnancy detail landscape](images/eink-ui/pregnancy-baby-landscape-optimized.png) | Final 120×48px Back control, matching hit boundary, and unclipped landscape detail composition. |
| [Device launcher portrait](images/eink-ui/device-launcher-portrait-optimized.jpg) | Real-panel launcher typography and rule evidence. |
| [Device dashboard portrait](images/eink-ui/device-dashboard-portrait-optimized.jpg) | Real-panel dashboard typography, card, and icon evidence. |
| [Device Font Test portrait](images/eink-ui/device-font-test-portrait.jpg) | Real-panel native-strike, baseline, rule, border, and icon evidence. |
| [Device Font Test landscape](images/eink-ui/device-font-test-landscape.jpg) | Real-panel 800 x 480 reflow and matching native-strike evidence. |
| [Device dashboard landscape release](images/eink-ui/device-dashboard-landscape-release.jpg) | Restored production layout, wrapping, binary geometry, and `landscape_0` evidence. |
| [Device dashboard portrait sleep](images/eink-ui/device-dashboard-portrait-sleep.jpg) | Retained portrait frame and crescent sleep-indicator evidence. |
| [Device dashboard reverse landscape](images/eink-ui/device-dashboard-landscape-reverse.jpg) | Opposite landscape grip, two-column alignment, compact battery, and binary geometry evidence. |
| [Device dashboard reverse portrait](images/eink-ui/device-dashboard-portrait-reverse.jpg) | Opposite portrait grip, centered hierarchy, and orientation-cleanup evidence. |
| [Device Weight partial sample 1](images/eink-ui/device-weight-partial-sample-1.jpg) | Landscape Weight editor partial-refresh sample at 65.2 kg. |
| [Device Weight partial sample 2](images/eink-ui/device-weight-partial-sample-2.jpg) | Landscape Weight editor partial-refresh sample at 63.6 kg. |
| [Device Weight partial sample 3](images/eink-ui/device-weight-partial-sample-3.jpg) | Landscape Weight editor partial-refresh sample at 63.1 kg. |
| [Cold-boot dashboard](images/eink-ui/evidence/cold-boot-home-2026-09-15.jpg) | Dashboard photograph from the monitored final-release cold-boot session. |
| [Weight partial checkpoint 1](images/eink-ui/evidence/weight-partial-01-2026-09-15.jpg) | Supplemental 67.3 kg checkpoint after the first +0.1 adjustment. |
| [Weight partial checkpoint 5](images/eink-ui/evidence/weight-partial-05-2026-09-15.jpg) | Supplemental 67.7 kg checkpoint after five +0.1 adjustments. |
| [Weight partial checkpoint 10](images/eink-ui/evidence/weight-partial-10-2026-09-15.jpg) | Supplemental 68.2 kg checkpoint after ten +0.1 adjustments. |
| [Wake dashboard](images/eink-ui/evidence/wake-home-2026-09-15.jpg) | User-labelled immediate wake frame with active portrait UI restored. |
| [Sleep dashboard](images/eink-ui/evidence/sleep-home-2026-09-15.jpg) | User-labelled retained sleep frame with battery and crescent indicator. |
| [Cold-boot serial evidence](evidence/cold-boot-2026-09-15.txt) | Final release upload, running-image hash match, `POWERON` boot, pregnancy restoration, and successful full-refresh log. |
| [Final physical-session evidence](evidence/final-physical-session-2026-09-15.txt) | Curated photo labels, partial-refresh checkpoint mapping, sleep disconnect, and release reconnect evidence. |
