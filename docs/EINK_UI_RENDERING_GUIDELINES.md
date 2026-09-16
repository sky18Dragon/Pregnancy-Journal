# E-Ink UI rendering guidelines

## 1. Choose the panel mode first

- Default application screens are monochrome.
- Monochrome pages use only binary presentation pixels for text and meaningful geometry.
- A gray4 page must explicitly request gray4 refresh; it must not be followed by a monochrome threshold and still be evaluated as gray4.
- Do not use `DarkGray` or `LightGray` as a substitute for semantic hierarchy on a page that will be refreshed in mono mode.

## 2. Fonts

- Generate one bitmap strike for each supported target size and weight.
- Render a strike pixel-for-pixel. Runtime glyph bitmap scaling is prohibited.
- Store width, height, bearing-x, bearing-y, advance, ascent, descent, and baseline information needed by the renderer.
- Use the same metrics for drawing, measurement, centering, wrapping, and overflow checks.
- User-facing CJK body copy starts at 16px. Prefer 18px when layout permits.
  A 14px Regular strike is allowed only for a single-line compact card summary
  that has width assertions and has passed straight-on real-panel review; it is
  not the default paragraph size.
- Use Regular for body copy and Medium for headings, metrics, short labels, and controls.
- For the SSD1677 production profile, Regular strikes at 12–18px use the W6
  source outline with a stricter 176 threshold. This is an offline optical-size
  correction: it protects small CJK strokes while staying lighter than the
  Medium W6/144 family. Regular strikes above 18px continue to use W3/128.
- Do not synthesize weight by expanding pixels at runtime.
- Missing user-facing glyphs are a build-time failure. An unexpected codepoint
  may only fall back to the native-size `?` glyph from the selected strike;
  runtime-scaled fallback faces are prohibited.

## 3. Antialiasing and hinting

- Monochrome strikes may be rasterized from an antialiased FreeType source offline, but the committed/generated strike must already be the final 1-bit result.
- Tune threshold and hinting per size/weight family with the Font Test Screen.
- Avoid relying on isolated one-pixel segments at 12–18px. The small Regular
  strike may use a heavier source outline, but it must keep open counters and
  remain measurably lighter than Medium.
- Do not create a four-level AA edge and expect a later 1-bit threshold to preserve it.
- Gray4 AA is permitted only when the same four levels reach the panel.

## 4. Baseline and spacing

- Position glyphs from a common baseline using stored bearings.
- Test CJK with numerals, punctuation, `%`, slash, Latin units, parentheses, and full-width punctuation.
- Never top-align mixed scripts by a reference glyph bbox.
- Letter spacing, line height, and paragraph gap are typography tokens, not arbitrary per-call values.
- Verify centered copy by measured advance, not bitmap bounds.

## 5. Lines, borders, and icons

- A normal monochrome divider is 1px pure black.
- Control outlines and important icon strokes are at least 2px pure black.
- Use 3px for emphasis icons or inverse marks when optical balance requires it.
- If a lighter divider is required, use an intentional binary dash/dither pattern and test it on hardware.
- Do not draw meaningful boundaries in `LightGray` on a mono page.
- Prefer filled high-contrast symbols for critical status; avoid intricate 1px outline icons.

## 6. Images and illustrations

- Monochrome icons are committed or generated as final 1-bit black/white data.
- Alpha and grayscale source pixels must be resolved offline; text, icons,
  dividers, and control borders must never depend on dithering.
- Continuous-tone illustrations may use an intentional fixed dither pattern,
  but the exact final panel buffer must be previewed and reviewed.
- Do not apply runtime bilinear or fractional scaling to bitmap assets. If an
  illustration must be enlarged, use an integer nearest-neighbor factor and
  inspect the resulting stair-step on the panel; prefer a native target-size
  asset for production.
- Thresholds belong to the asset-generation pipeline and must be reproducible
  from source files and generator arguments.

## 7. Layout and orientation

- Design both 800 x 480 and 480 x 800 variants; rotating a finished screenshot is not a layout strategy.
- Determine layout from logical canvas dimensions after the orientation policy is applied.
- Keep primary content at least 24px from logical edges.
- Preserve 48px minimum touch targets and keep visual labels optically centered inside them.
- Re-run width, wrap, and clipping checks in both orientations for every dynamic string.
- Touch coordinates must continue through `physical_to_logical()`; visual changes must not bypass the existing orientation mapping.

## 8. Refresh

- Partial refresh sends a full comparison frame on SSD1677; page code should mutate only intended areas.
- Preserve periodic full cleanup and verify ghosting after ten repeated partial refreshes.
- Log each partial refresh with its sequence number since the last successful
  full/clear refresh, elapsed time, and result so physical photographs can be
  tied to the exact update count.
- Orientation changes require a full refresh because the whole logical composition changes.
- Sleep and wake must retain or rebuild a frame with the same rotation and typography tokens.

## 9. Preview and testing

Every visual regression artifact must include both:

1. the logical 2-bit canvas preview; and
2. the exact final 1-bit (or gray4) buffer representation sent to the panel.

Required checks:

- Font Test Screen in portrait and landscape.
- Current vs optimized pregnancy dashboard and launcher screenshots.
- Width and wrap assertions for dynamic content.
- 1px/2px geometry and icon samples.
- Host test suite and clean release build.
- Device matrix: Cold Boot, Full, Partial x10, Sleep, Wake, Portrait, Landscape.

## 10. Review checklist

- Is the intended panel mode explicit?
- Does every visible text role use a tokenized size and weight?
- Is any glyph bitmap scaled at runtime?
- Are baseline metrics used for mixed scripts?
- Can every meaningful line/icon survive the final output format?
- Does the final-buffer preview match the expected hierarchy?
- Do both orientations fit without truncation or overlap?
- Has the page been photographed on the real device after full and repeated partial refresh?
