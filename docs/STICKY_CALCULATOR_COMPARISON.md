# Sticky Calculator rendering comparison

Reference repository: <https://github.com/Lukilyy/reterminal-sticky-calculator>

The comparison is based on the reference repository's actual `main/ui`, `main/assets`, `main/devices`, and `main/app` source. The useful lesson is its rendering contract, not its calculator visual style.

| Area | Sticky Calculator | Current project | Decision for this project |
| --- | --- | --- | --- |
| Canvas | Packed 2-bit canvas. | Packed 2-bit canvas with logical four-way rotation. | Keep current canvas and rotation architecture. |
| Production refresh | Explicit 1-bit monochrome full/partial refresh; periodic full cleanup. | Production pages now author text and meaningful geometry as binary pixels before using 1-bit full/partial refresh. | Keep monochrome as the explicit default contract; gray4 remains opt-in only. |
| Font strategy | Modern calculator uses several pre-generated Inter bitmap strikes. | Regular/Medium Hiragino strikes are generated at every supported native target size and copied pixel-for-pixel. | Adopted; no glyph bitmap is resized at runtime. |
| Glyph metrics | Each Inter glyph stores width, height, bearing-x, bearing-y, and advance. Text is drawn from a baseline. | Each generated glyph now stores width, height, both bearings and advance; each strike stores ascent/descent. | Adopted for stable mixed Latin/CJK placement. |
| Antialiasing | Inter strike data is 1-bit and copied pixel-for-pixel. | FreeType gray coverage is thresholded reproducibly offline into committed 1-bit strike data. | Adopted; no gray AA is emitted by the text draw loop. |
| Size fallback | Result text chooses among fixed large/medium/small/compact strikes by measured width. | Semantic tokens and native strikes cover body/display roles; width fallback selects among native strikes. | Keep the fallback list small and role-based. |
| Basic ASCII path | 5 x 7 glyphs use integer block scaling. | Latin/ASCII is included in every generated native strike. Missing codepoints use that strike's native-size `?`. | Deliberately differs: no runtime-scaled text fallback remains. |
| Icons | Important icons use explicit 2–3px thick strokes and filled shapes. | Production semantic icons use standardized 2px strokes, with 3px reserved for emphasis. | Adopted at device scale. |
| Lines | High-contrast black/white design. | Production dividers and borders use intentional 1px/2px black geometry. | Adopted; hierarchy no longer depends on gray rules. |
| Refresh cadence | Dedicated update task, 30ms input coalescing, full refresh every 15 partial updates. | Page/app refresh is direct; app transitions force cleanup after five fast transitions. | Keep the current safe transition cleanup initially; add test coverage before changing cadence. |
| Orientation | Reference UI is fixed 480 x 800 portrait and converted to native 800 x 480. | All six registered apps use `FollowDevice`; page layouts reflow at logical 800 x 480 or 480 x 800 and touch uses the same rotation transform. | Preserve the four-way orientation system. |

## Reference call chain

For the reference modern calculator, `IphoneCalculatorView` chooses a pre-generated `IphoneFontStrike`, measures text from each glyph's `advance`, and calls `draw_inter_text()`. The draw loop reads the glyph's exact 1-bit bitmap at its stored width/height, positions it from a baseline using `bearing_x` and `bearing_y`, and writes black or white canvas pixels directly. No font bitmap scaling occurs in the draw loop.

The reference display driver converts its portrait 2-bit canvas to a native 1-bit frame using the same `gray < 2` threshold, then sends a full comparison frame for both partial and full refreshes. The font rendering remains stable because the intended font pixels are already binary before that conversion.

## What not to copy

- The fixed portrait coordinate model does not meet this project's orientation requirement.
- The reference's 5 x 7 general text path is not suitable for Chinese UI copy.
- The calculator's large black visual theme is specific to its product page and should not replace the pregnancy application's calm white information layout.
- Its exact refresh interval is not automatically correct for this app's navigation and sleep behavior.

## Adopted principles

1. Design and rasterize for the final panel mode.
2. Use fixed native-size strikes with real glyph metrics.
3. Measure text with the same metrics used to draw it.
4. Use binary geometry for monochrome pages.
5. Make every target size and both orientations visible on a hardware test page.
