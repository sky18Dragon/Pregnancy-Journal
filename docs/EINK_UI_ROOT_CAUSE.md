# E-Ink UI root-cause assessment

## P0 — final refresh mode invalidates the authored pixels

All production pages use monochrome refresh, but the UI is authored as four-level grayscale. `DarkGray` and `LightGray` are therefore not stable presentation colors:

- `LightGray` always maps to white and disappears.
- Antialiased black pixels with coverage 1 map to `LightGray` and disappear.
- `DarkGray` text keeps only fully covered source pixels.

This is the direct cause of the missing pregnancy caption, broken countdown, weak week progress, and absent card outline.

## P0 — glyphs are resampled at runtime before a second quantization

Every CJK/Latin UI glyph begins as a 32 x 32 two-mask source. Body text is downsampled to 12–24px and display text is enlarged to 52–56px. Coverage is first quantized to four levels and later thresholded to one bit. Thin strokes, counters, and diagonal joins can move across the final threshold from one size to the next.

The resulting defects are deterministic software artifacts, not random panel defects.

## P1 — font metrics do not model a baseline

Generated glyphs store only advance. All glyphs are placed in a square cell from a shared top derived from `国`. There is no ascent, descent, bearing, or baseline. Mixed numerals, punctuation, Latin units, and CJK can therefore appear vertically inconsistent even when their bounding boxes fit.

## P1 — semantic weight is missing

`Canvas::draw_text()` defaults to Medium. Many body, subtitle, summary, footer, and button call sites omit a weight and therefore use Hiragino W6. On a 1-bit panel this closes small counters and merges nearby strokes. Conversely, Regular text drawn in `DarkGray` is over-thinned by the final threshold. The current API creates both extremes.

## P1 — critical geometry is below the physical stroke budget

Pregnancy card outlines/dividers use `LightGray`, and icons use 1px outlines. The gray geometry vanishes in mono mode; the 1px black icon geometry is less stable after panel waveform, viewing angle, and camera capture. Important icons and controls need a minimum 2px binary stroke.

## P2 — typography sizes and spacing are implementation-driven

Legacy scale values map to 12, 14, 21, 28, 35, 42, 49, 56, and 70px. Direct pixel sizes add a second system. The launcher uses 12px Medium summaries and footer copy, while the pregnancy card uses 14px Regular copy. These are close to the physical limit for complex Simplified Chinese without size-specific hinting.

Spacing is similarly generic: 1px letter spacing for all sizes below 24px. It is not tied to role, weight, or orientation.

## P2 — preview and panel validation use different outputs

Host PPM screenshots show the 2-bit logical canvas, not the 1-bit buffer delivered to the panel. A layout can look balanced on desktop while secondary text and borders are missing on hardware. The preview pipeline needs both logical-gray and final-mono output.

## P3 — large bitmap illustrations are scaled at runtime

The 1-bit pregnancy comparison illustrations are enlarged with nearest-neighbor integer blocks. They survive the panel well, but exhibit staircase edges and do not share a consistent optical size across objects. This is less urgent than text because the silhouettes remain understandable.

## Causal summary

```text
32px antialiased source
  -> runtime resize to arbitrary size
  -> 0..3 coverage quantization
  -> optional DarkGray/LightGray styling
  -> 2-bit canvas
  -> hard 1-bit threshold
  -> SSD1677 waveform
  -> lost thin strokes / merged heavy strokes / invisible gray rules
```

The P0 fixes must land before judging smaller spacing or layout adjustments. Otherwise layout tuning is being performed against pixels the panel never receives.
