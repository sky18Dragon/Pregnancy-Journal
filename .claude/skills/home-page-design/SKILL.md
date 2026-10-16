---
name: home-page-design
description: Redesign or explore the pregnancy home page (孕周首页 / Baby Week dashboard) of the reTerminal Sticky firmware — week progress plus the baby-size comparison. Use when asked to redesign, restyle, compare layouts for, or review the home/dashboard page. Renders candidate layouts through the real firmware renderer as 1-bit PNGs and lints them against the e-paper rules.
---

# Home-page design lab

The home page is `pregnancy_page_render_dashboard()` in
`src/apps/pregnancy/pregnancy_pages.cpp` (landscape 800×480 + portrait 480×800).
It must show: week headline, progress, due-date countdown, the baby-size
comparison (148×148 1-bit object picture + name), this week's note, and the
entries to the Baby / Mom pages and the launcher menu.

Rules live in `CLAUDE.md` (e-ink constraints) and `DESIGN.md`. Read the
"墨水屏 UI 约束" section before designing. The short version: black/white only,
no gray, no animation, text ≥ 16 px (≥ 18 px if the user must read it to act),
2 px borders, 24 px margin, hit targets ≥ 48 px, one `Rect` for drawing and
hit-testing, `ui_tokens::` names instead of numeric literals, selected = inverted.

## Workflow

1. **Brief.** Ask only what the code cannot answer: what the page should feel
   like, what must stay one tap away. Cite the `docs/ui-audit.md` items the work
   addresses (V7 information density, I2 tap affordance, C3 duplicated layouts).
2. **Choose the register first.** The page is also a desk ornament that a
   pregnant woman glances at from across the room, so default to the *poster*
   register, not an app screen: one dominant fact (an oversized serif week
   numeral), generous white space, no framed cards, no button chrome (menu = three
   strokes, entries = underlined text links with ≥ 48 px hit areas), the baby-size
   object as the specimen, progress drawn as an artwork element (week-dot dial,
   growth rings, tick ruler) instead of a bar + percentage. Reference artwork:
   `assets/concepts/pregnancy-week-posters-20260927/`. Fall back to a utility
   layout only when asked.
3. **Explore in the lab, not in firmware.**
   ```bash
   tools/home_lab/render_lab.sh            # weeks 6,12,21,28,34,40,41
   tools/home_lab/render_lab.sh all        # every week 6–41 (scrubber)
   tools/home_lab/render_lab.sh 21 g_specimen
   ```
   Open `build/home_lab/lab.html`: single concept or side-by-side, landscape /
   portrait, week scrubber (←/→), hit-area overlay (H), per-frame lint.
4. **Add or change a concept** in `tools/home_lab/home_lab.cpp`: write
   `render_xxx(Canvas&, const Input&, const Facts&, Targets&, bool portrait)` and
   list it in `kConcepts`. Fill `Targets` with the rectangles you actually drew
   (`menu`, `baby`, `mom`) so the overlay and the 48 px lint are honest. Add
   the name/description in `CONCEPTS` in `tools/home_lab/build_viewer.py`.
   Shared pieces: `numeral` (serif bitmap digits, sizes Xl/Lg/Md), `kLabelWeek`,
   `week_dial`, `ring_nest`, `week_ruler`, `text_link`, `menu_icon`, `fruit`.
   Poster typography comes from `make_display_assets.py` (bitmaps in build/);
   the firmware font stays for body text.
5. **Always check the extremes**, not only week 21: week 6 (tiny object, no
   fixed weight), 37 (longest object names), 41 (overdue wording, bar capped at
   100 %), both orientations. Long copy must wrap at word boundaries — never
   split a word like 胡萝/卜.
6. **Pick with the owner**, then port one page at a time:
   - Put one `PageLayout` per orientation in `pregnancy_pages.cpp`; render and
     `pregnancy_page_targets()` / `pregnancy_page_action_at()` use the same rects.
   - Reuse `prototype_ui::draw_frame / draw_progress_bar / draw_button`; the
     segmented trimester bar and week strip are new components — add them to
     `src/ui/` with tokens rather than inlining.
   - Update `test/pregnancy_pages_test.cpp` (targets on screen, ≥ 48 px, no
     overlap) and keep `tools/ui_preview/ui_preview.cpp` registration.
   - Run `tools/ui_preview/render_all.sh build/ui_preview pregnancy_dashboard`,
     `./tools/run_host_tests.sh`, `python3 tools/check_markdown_links.py`.
   - Refresh policy stays: page change → full refresh, in-page change → partial.
     Do not touch `src/display/`, `src/app/` or the drivers.
7. **Report honestly**: "仅宿主预览，未经真机验证". Ghosting, refresh time and
   readability at arm's length need the device (`docs/EINK_UI_REAL_DEVICE_TEST.md`).

## Decisions so far (owner, this session)

- Layout: **E 勋章表盘** (medallion: 40 week-dots around the object, big week
  number left, text links bottom-left). F/G stay in the lab as alternatives.
- Week numeral: **calendar-style** heavy condensed grotesque (prototype font
  Helvetica Neue Condensed Black; OFL stand-ins: Anton, Archivo Black / Archivo
  Narrow Black, Oswald Bold).
- Illustrations: the **naive brush-ink doodle** style was tried and rejected as
  too rough. Current direction is **geometric Bauhaus-style silhouettes**
  (`tools/home_lab/geo_art.py`): clean ellipses/lenses/splines, constant-width
  curves, white gaps between overlapping parts, details as negative space, no
  faces. Two renderings of one geometry: `solid` and `line`; pick one with the
  owner (4th arg of `render_lab.sh`). Replaceable per file (`PROMPT.md` beside
  the PNGs). Flash cost of 35 × 280×280 1-bit ≈ 343 KB raw — plan compression or
  a smaller size before porting.

- Latest direction (owner shared a Dribbble risograph fruit/veg poster series as
  the reference and asked for the **whole page** to follow it): layout **H**
  (`h_riso`) with illustration style `riso` — heavy condensed English title
  (`CARROT //`), leaves/stems overprinting the fruit (overlap flips to paper),
  object crossing an offset black disc (ink XOR), 40-tick ruler for progress.
  Colour overprint is approximated in 1-bit; no gray, no halftone.

- Newest direction (owner shared an engraved fruit/veg week sheet and said all the
  earlier styles were wrong): layout **I** (`i_plate`) with art style `engrave`
  (`stipple_art.py`). Caveats to raise with the owner every time:
  1. **Rule conflict**: dithered/stipple fills contradict CLAUDE.md "no dither /
     no hatch shading". Needs an explicit owner decision (or a reviewed 4-level
     gray page) before it can ship.
  2. **Source quality/licence**: the lab art is cut from the owner's low-resolution
     reference sheet (objects ≈100 px, upscaled) and written to build/ only. For
     production, regenerate each week at ≥1024 px (ImageGen, "engraved stipple
     botanical plate, black ink on white, no text") and feed `--dir`.
  3. **Fonts**: some Chinese names in that set need glyphs the firmware font lacks
     (扁豆 葡萄 芸豆 青柠 梅子 橙子 芜菁 凉薯 大白菜 金丝瓜 木菠萝…); previews use
     stand-ins (`PREVIEW_ZH`). Add the real characters with the font generator.
  4. Object mapping differs from `pregnancy_week_visual` (e.g. week 24 eggplant vs
     corn); reconcile the content table before porting.
  5. Flash: 35 × 280×280 1-bit ≈ 343 KB raw, but stipple does not compress well.

## Data and content cautions

- **Poster typefaces are not firmware-ready.** The lab rasterizes digits and 孕周
  from macOS system fonts (Helvetica Neue, STHeiti) for prototyping only. Shipping needs an
  OFL typeface (e.g. Anton / Archivo Black for digits, Noto Sans SC for 孕周), generated into `src/ui/assets/` by a committed generator, ~10 digits ×
  2 sizes × a few hundred bytes–4 KB each (check flash budget).

- Object names, sentences and weekly notes come from
  `src/pregnancy/content/` (`pregnancy_week_visual`, `content_service`); the
  lab uses them unchanged. Do not invent new medical statements.
- **Length / weight are placeholders.** `tools/home_lab/week_size_reference.tsv`
  holds rounded public averages, unreviewed by a clinician, and not in the
  firmware. A concept may show them to test layout, but shipping any number
  needs a reviewed data source first — flag this whenever you present a concept
  that uses them. The length series changes measuring convention at week 20
  (visible step in concept C).
- Every new Chinese character must exist in the generated font strikes. The lab
  reports `MISSING GLYPH U+XXXX`; fix copy or regenerate fonts with the
  generators in `tools/`, never ship a box glyph. (Example caught already: 曲.)
- Pregnancy progress is personal record-keeping, not a verdict: no scores, no
  "normal / abnormal" wording.

## Reading the lint

`render_png.py` output (also shown per frame in `lab.html`): gray pixels,
hit areas < 48 px, overlapping hit areas, text below 16 px, text outside the
canvas or overlapping other text, lines cut by the line limit, labels overflowing
their target, missing glyphs. All must be zero. 16–17 px caption-tier text is
only counted: confirm by eye that it never carries an action.
The lint does not see non-text collisions (a tag over the curve, a picture
touching a frame) — look at the images.
