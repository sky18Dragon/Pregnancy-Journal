# E-Ink UI optimization plan

## Rendering contract

The default application UI is monochrome. Every production text, icon, border, and divider must be intentionally valid after 1-bit conversion. Four-level grayscale is an opt-in page mode and must use a gray4 refresh end-to-end.

## Phase 1 — current project analysis (complete)

- Trace page copy from layout call site through UTF-8 decode, glyph lookup, rasterization, canvas packing, rotation, display conversion, and SSD1677 refresh.
- Record the currently unused gray4 path.
- Record host baseline: 22/22 tests passing.

## Phase 2 — Sticky Calculator comparison (complete)

- Verify source implementation rather than infer from screenshots.
- Adopt fixed native-size bitmap strikes, baseline metrics, pixel-for-pixel drawing, binary icon strokes, and final-output validation.
- Preserve this project's four-way orientation behavior.

## Phase 3 — device-photo review (complete)

- Annotate three photos by logical screen region.
- Map each visual defect back to its code size, weight, color, and geometry.
- Use a software mono-threshold preview to reproduce the device failure.

## Phase 4 — root-cause ranking (complete)

- P0: gray authoring combined with mono output.
- P0: runtime resize plus double quantization.
- P1: missing baseline metrics and semantic weights.
- P1: sub-budget icon/rule strokes.
- P2: implementation-driven sizes/spacing and preview mismatch.
- P3: scaled 1-bit illustration edges.

## Phase 5 — target visual system (complete)

The initial token set will be encoded in `ui/ui_tokens.h`:

| Role | Target | Weight | Output color | Notes |
| --- | --- | --- | --- | --- |
| Micro test only | 12px | Regular/Medium | Black | Not approved for normal CJK body copy. |
| Caption / compact label | 14px | Medium | Black | Short labels only; must pass hardware test. |
| Secondary body | 16px | Regular | Black | Minimum normal Chinese copy. |
| Primary body | 18px | Regular | Black | Preferred information text. |
| Emphasis / metric | 20px | Medium | Black | Short values or strong labels. |
| Section heading | 22/24px | Medium | Black | One line. |
| Page heading | 28px | Medium | Black | Avoid for long text. |
| Display value | 52/56px | Medium | Black | Fixed strikes, never enlarged from a smaller source. |

Additional tokens:

- Portrait content margin: 24px.
- Landscape content margin: 24–32px depending on dashboard split.
- Minimum Chinese body line height: font size + 6px.
- Minimum interactive control height: 48px; preferred 56px.
- Standard icon stroke: 2px; emphasis stroke: 3px.
- Standard rule: 1px black; strong rule/control border: 2px black.
- Secondary hierarchy is expressed through size, spacing, grouping, or 1-bit dither—not unstable gray text.

## Phase 6 — Font Test Screen (complete)

Add a compile-time debug screen behind `UI_DEBUG_FONT_TEST` containing:

- Chinese and Latin samples at 12, 14, 16, 18, 20, 22, and 24px.
- Regular and Medium side-by-side.
- Numerals, punctuation, `%`, `/`, `cm`, `g`, and mixed CJK/Latin baselines.
- 1px and 2px horizontal/vertical lines, borders, circles, and representative icons.
- Both portrait and landscape layouts.
- Labels indicating the strike, weight, and output mode.

The page is implemented in `src/ui/font_test_screen.cpp`, is enabled by the
`sticky-font-test` and `sticky-font-test-portrait` PlatformIO environments, and is rendered through the same
final monochrome conversion as production. Host coverage verifies every native
strike from 12px through 24px in both weights. The screen contains the required
complex Chinese phrases, complete numeral/Latin rows, 1px/2px lines and
borders, and 16/20/24px icon samples in both orientations.

## Phase 7 — preview and overflow validation (complete in software)

- Produce logical grayscale and final monochrome screenshots for current and optimized pregnancy dashboard and launcher layouts.
- Add width assertions for all dynamic week/object strings and launcher copy.
- Check wrapping, clipping, centered alignment, and touch-target geometry in portrait and landscape.
- Run the full UI checklist and AI-slop detector from the UI design skill.

The final-buffer artifacts are stored in `docs/images/eink-ui/`. Width,
wrapping, clipping, binary-output, native-strike, and both-orientation checks
are covered by the host tests. The visual audit found no decorative gradients,
floating-card stacks, excessive pill shapes, icon-in-text misuse, ornamental
hero copy, or desktop-first responsive leftovers. The remaining gate is the
photo-based hardware matrix in `EINK_UI_REAL_DEVICE_TEST.md`.

## Phase 8 — implementation order and gates (complete)

1. Add tokens and final-output conversion helpers/tests; build and host tests.
2. Add fixed native-size font strikes and baseline metrics; build and host tests.
3. Add Font Test Screen; build and host tests.
4. Migrate shared headers/buttons/launcher; build and host tests.
5. Migrate pregnancy dashboard portrait/landscape; build and host tests.
6. Migrate kick/checkup/reminder/weight/settings pages; build and host tests.
7. Thicken icons/rules and remove production reliance on gray geometry; build and host tests.
8. Generate before/after previews and audit overflow; release build.

All eight implementation gates passed. The final host suite is 25/25. The
release firmware built with 24,476 bytes RAM (7.5%) and 4,105,244 bytes flash
(48.9%); its upload image is 4,105,904 bytes. The `sticky-font-test` build uses
23,740 bytes RAM (7.2%) and 4,040,676 bytes flash (48.2%); the portrait variant
uses 4,040,680 bytes. Their upload images are 4,041,072 and 4,041,088 bytes.

A final source-level interface audit found five compact controls whose visual
and hit rectangles were still 40–44px tall despite the 48px token contract:
shared header actions, launcher Close, pregnancy detail Back, landscape
Reminder filters, and the Weight unit selector. They now consume
`ui_tokens::kMinimumTouchHeight`; the shared-header and launcher boundaries are
covered by host assertions. All three firmware profiles rebuilt successfully,
and the corrected release was reflashed with hash verification.

The last scaled 5 x 7 missing-glyph fallback was also removed from the draw
path. Unsupported codepoints now resolve to the native-size `?` glyph in the
selected strike, and host coverage verifies identical measurement and pixels.

## Phase 9 — device matrix (complete for the confirmed scope)

After flashing the optimized build, capture and record:

- Cold Boot
- Full Refresh
- Partial Refresh x10
- Sleep
- Wake
- Portrait
- Landscape

Acceptance criteria are defined in `docs/EINK_UI_REAL_DEVICE_TEST.md`. A software preview is not sufficient evidence for hardware completion.

The optimized release was flashed successfully and its image hash was verified
by esptool. Post-reset logs show the touch controller polling normally at the
portrait logical map of 480 x 800. Straight-on Font Test and production photos
now pass Cold Boot, Full Refresh, Sleep, Wake, Portrait, and Landscape. The
operator explicitly requested that the Weight editor Partial x10 case be
skipped, so it is recorded as a non-blocking matrix item; its photographs and
successful partial-refresh logs remain supplemental evidence. All registered
apps use `FollowDevice`; their portrait and landscape controls share the same
layout-derived hit regions.
