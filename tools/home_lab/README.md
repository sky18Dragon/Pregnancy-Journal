# Home-page design lab

Host-only sandbox for exploring the pregnancy home page (week progress +
baby-size comparison). It compiles the real `Canvas`, CJK font strikes,
`ui_tokens`, `prototype_ui` and the 148×148 week illustrations, renders three
poster-style concepts × both orientations × any weeks as 1-bit PNGs, lints them with
`tools/ui_preview/render_png.py`, and builds a viewer.

```bash
tools/home_lab/render_lab.sh            # weeks 6,12,21,28,34,40,41 -> build/home_lab/lab.html
tools/home_lab/render_lab.sh all        # weeks 6-41
tools/home_lab/render_lab.sh 21 g_specimen
tools/home_lab/render_lab.sh 21 i_plate "" engrave   # layout I with the engraving art (cut from assets/concepts/references/)
tools/home_lab/render_lab.sh 21 h_riso "" riso   # H layout with the overprint style
tools/home_lab/render_lab.sh 21 e_medallion "" line    # illustration style: solid (default) | line | pixel (old firmware art); 3rd arg is the output dir
```

| File | Role |
| --- | --- |
| `home_lab.cpp` | concepts E 勋章表盘 / F 年轮 / G 标本图版 / H 叠印海报 (desk-ornament posters; H = whole page after a risograph print series: condensed title, overprint-flip object on an offset black disc, 40-tick ruler; I = engraved-plate calendar: big numeral + WEEKS, stipple engraving hero, letter-spaced English caption, 8×5 week map) |
| `stipple_art.py` | converts engraving-style illustrations (the owner's 8×5 reference sheet via `--sheet`, or a folder of ImageGen PNGs via `--dir`) into 1-bit stipple assets + `labels.tsv`; used by art style `engrave` and layout I |
| `geo_art.py` | generates the 35 geometric week illustrations in two styles (`solid` negative-space silhouettes, `line` uniform outlines) into `assets/concepts/geo-week-size/{solid,line}/`; drop hand-made/ImageGen PNGs with the same names to replace one |
| `make_display_assets.py` | packs the calendar-style numerals (Helvetica Neue Condensed Black), the 孕周 label and the chosen illustration style into build/ — fonts are macOS system fonts, never committed; use OFL fonts before shipping |
| `week_size_reference.tsv` | placeholder length/weight (unreviewed, never shipped) |
| `build_viewer.py` | writes `lab.html` (scrubber, comparison, hit overlay, lint) |
| `render_lab.sh` | build + run + lint + viewer |

The guided workflow, rules and porting steps are in
`.claude/skills/home-page-design/SKILL.md`. Output is a host preview only and
does not reflect refresh behaviour or ghosting on the panel.
