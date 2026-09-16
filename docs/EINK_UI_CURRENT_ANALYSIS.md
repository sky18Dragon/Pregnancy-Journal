# E-Ink UI current rendering analysis

## Scope and evidence

This document records the current implementation before the rendering changes. It is based on the repository source, the host-rendered framebuffer previews, and the three device photographs supplied on 2026-09-14. Photo text is treated as visible UI content, not as instructions.

Hardware viewport is 800 x 480 pixels. The same physical framebuffer is exposed as either an 800 x 480 landscape canvas or a 480 x 800 portrait canvas through logical rotation.

## True render call chain

The pregnancy dashboard is representative of the whole application:

1. `pregnancy_page_render_dashboard()` selects portrait or landscape layout from `Canvas::width()` and `Canvas::height()`.
2. Page copy is passed to `Canvas::draw_text_pixels()` with a pixel size, color, and `FontWeight`.
3. `font_decode_utf8()` decodes a codepoint and `chinese_font_glyph()` selects the Regular or Medium entry from the generated table.
4. Every generated glyph contains a 32 x 32 `solid_rows` mask and a 32 x 32 `edge_rows` mask. Both Latin and CJK characters use this table when present.
5. `ui_target_coverage()` resamples the 32 x 32 source at runtime into the requested square pixel size. Solid samples count as 3, edge samples as 1, and the average is quantized back to coverage 0–3.
6. `ui_coverage_color()` converts coverage to `GrayLevel::{Black, DarkGray, LightGray, White}` and `Canvas::draw_pixel()` writes a packed 2-bit pixel.
7. `Canvas::draw_pixel()` also applies the selected logical rotation. The buffer itself remains physically 800 x 480.
8. Every production app currently calls `sticky_display_refresh_monochrome()` or `sticky_display_refresh_partial()`.
9. The display driver rotates the packed framebuffer by 180 degrees, then converts it to 1-bit: values 0/1 become black and values 2/3 become white.
10. The 1-bit frame is sent to the SSD1677 with either the full or partial monochrome waveform.

The separate `sticky_display_refresh()` path preserves all four gray levels, but no production page calls it.

## Consequence of the monochrome threshold

For black text, antialiased coverage 1 becomes `LightGray` and is therefore erased by the final 1-bit threshold. Coverage 2 or 3 survives.

For `DarkGray` text, only coverage 3 survives. Coverage 1 or 2 becomes `LightGray` and is erased.

For `LightGray` lines and borders, every pixel becomes white in the final monochrome frame. These elements are guaranteed to disappear, independent of panel quality.

This explains the gap between the grayscale host preview and the photographed device. The host preview displays the intermediate 2-bit canvas; the panel receives the thresholded 1-bit result.

## Font source and rasterization

- Generator: `tools/generate_chinese_font_assets.py`.
- Source cell: 32 x 32 pixels.
- Font faces: Hiragino Sans GB W3 for Regular and W6 for Medium, selected by TTC face index.
- Rasterization: Pillow FreeType rendering to an 8-bit temporary image, then two thresholds. Regular solid threshold is 88; Medium is 112; edge threshold is one third of the solid threshold with a floor of 16.
- Metrics: only `advance` is stored. Bearing, ascent, descent, and baseline are not stored.
- Vertical placement: every glyph is top-aligned using the top of the reference glyph `国`, rather than placed on a shared baseline.
- Current generated asset: 597 codepoints x 2 weights; source file is about 1.0 MB as C++ text.
- Runtime size mapping: legacy scale 1 becomes 12px; scale 2 and above become `7 * scale`. Direct pixel sizes are also used, including 13, 14, 16, 18, 20, 22, 24, 52, and 56.
- Inter-character spacing is only 1px from 12px through 23px, and 2px from 24px through 47px.

The 32px master is therefore enlarged for large display text and downsampled for body text. Both directions can create irregular stems. The later monochrome threshold amplifies those errors.

## Canvas, assets, icons, and rules

- The canvas is correctly packed at 2 bits per pixel and clips drawing primitives.
- The pregnancy size illustrations are 1-bit assets enlarged at runtime with integer nearest-neighbor blocks. Their strong black shapes survive well in the photos, but the scale-2 pixel stair-step is visible.
- Most semantic icons are drawn from 1px lines and circles. On the physical panel their perceived weight is weaker than adjacent text.
- Many separators and card outlines use `LightGray`; those pixels cannot survive the current production refresh path.
- `DarkGray` is used for secondary text and some separators, but its antialiasing is effectively reduced to only fully covered source pixels.

## Orientation chain

The rotation behavior is implemented independently from the page layout:

1. The IMU monitor classifies and settles physical orientation.
2. `sticky_app_display_rotation()` resolves the app's rotation policy.
3. `Canvas::set_rotation()` changes logical dimensions and pixel coordinate mapping.
4. Pages choose portrait or landscape layout from the resulting logical dimensions.
5. Touch points are transformed back through `Canvas::physical_to_logical()`.
6. An orientation change pauses the current app, sets the new rotation, resumes/redraws it, and uses a full monochrome refresh.

The architecture supports all four settled orientations. The remaining issue is visual parity: each page must use the same typography rules and safe margins in both 800 x 480 and 480 x 800 layouts.

## Photo review and region annotations

### Device photo 1 — portrait pregnancy dashboard

Logical regions use the 480 x 800 page coordinates from the code.

| Region | Bounds | Observation | Code relationship |
| --- | --- | --- | --- |
| P1-A | y=26–82 | Large `孕 21 周` is readable but heavy and slightly rough. | 52px Medium, 32px source enlarged at runtime. |
| P1-B | y=96–120 | `21 / 40 周` is thin, broken, and faint. | 20px `DarkGray` Regular; only coverage 3 survives. |
| P1-C | y=140–154 | Black progress fill is strong; gray remainder is weak. | `LightGray` remainder is erased in mono. |
| P1-D | y=174–194 | Countdown loses many small CJK strokes. | 16px `DarkGray` Regular, worst-case threshold combination. |
| P1-E | y=208–508 | Illustration is the clearest element. | 1-bit asset rendered in black. |
| P1-F | y=516–544 | Size title is readable but visibly dense. | 24/20px Medium. |
| P1-G | y=558–578 | Supporting caption is almost absent. | 13px `DarkGray` Regular. |
| P1-H | y=602–776 | Card edge/divider disappear; 14px body and 1px icons are weak. | `LightGray` geometry plus small Regular text. |

### Device photo 2 — landscape kick dashboard

| Region | Bounds | Observation | Code relationship |
| --- | --- | --- | --- |
| P2-A | x=24–776, y=22–85 | Title is readable; subtitle is small and less even. | Header scales 4 and 2 map to 28px and 14px Medium. |
| P2-B | y=100–220 | Period labels are acceptable; `--` dominates correctly. | 14px labels and 49px values, both Medium. |
| P2-C | y=230/314 | Dividers vary in visibility. | `DarkGray` 1px rules are threshold-dependent. |
| P2-D | y=258–286 | Empty state is readable at normal viewing distance. | 21px Medium. |
| P2-E | y=400–456 | Filled primary action is robust; outline action is lighter. | Pure black fill vs 1px outline. |

### Device photo 3 — portrait launcher

| Region | Bounds | Observation | Code relationship |
| --- | --- | --- | --- |
| P3-A | y=20–82 | Heading is overly heavy; 12px subtitle is cramped. | 28px Medium plus 12px default Medium. |
| P3-B | six 98px rows | Numbers and titles are readable but dense. | 28px and 21px, both default Medium. |
| P3-C | row summaries | Small Chinese strokes merge or close counters. | 12px default Medium; too bold for this size. |
| P3-D | row dividers | Rules are visible but visually compete with small copy. | 1px `DarkGray`, flattened to black. |
| P3-E | y=758 | Footer is near the practical readability limit. | 12px default Medium. |

## Current validation baseline

- Host test suite: 22/22 passing before this optimization work.
- Current logical grayscale previews are produced by `pregnancy_pages_test` and `app_launcher_test`.
- A software simulation of the display's `< 2 => black` threshold reproduces the same missing captions, weak countdown, and launcher stroke merging visible in the device photos.
