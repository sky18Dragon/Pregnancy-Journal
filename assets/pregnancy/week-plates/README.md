# Pregnancy home-page plates

340×340 1-bit stipple engravings for weeks 6–40, used by both orientations, packed into
`src/ui/assets/pregnancy_plate_assets.cpp` by
`python3 tools/generate_pregnancy_plate_assets.py`.

- **Provenance**: since 2026-10-17 the set is AI-generated artwork. Each object was drawn
  by the Codex CLI's image-generation tool (1536–2048 px, one shared style prompt, no
  reference image passed) and converted to 340×340 1-bit stipple by
  `tools/home_lab/stipple_art.py --dir`. The prompts are in `prompts/` (`style.txt` is
  shared, `objects.tsv` has one line per week, `gen_one.sh` is the runner). The full-size
  originals are not committed. The earlier set came from the owner's reference sheet and
  was replaced because its licence was not recorded. Before release the owner should
  confirm that the generator's terms allow commercial use of its output.
- **Replace one plate**: put a pure black/white 340×340 PNG with the same file name
  here, then run the generator and the host tests. Re-create the whole set by generating
  `week-NN-name.png` originals into a folder and running
  `tools/home_lab/stipple_art.py <out> --dir <folder> --size 340`.
- **Rule exception**: dithered fills conflict with the "no dither / no hatch shading"
  rule in `CLAUDE.md`; this page follows the owner's explicit design direction.
- Captions (English, shown under each plate) live in the generator.
- Numerals use Bebas Neue (SIL OFL 1.1), vendored with its license in
  `assets/fonts/bebas-neue/`. The generator reads it by default; pass `--font` to use
  another condensed OFL font.
