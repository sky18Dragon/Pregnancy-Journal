# Pregnancy home-page plates

340×340 1-bit stipple engravings for weeks 6–40, used by both orientations, packed into
`src/ui/assets/pregnancy_plate_assets.cpp` by
`python3 tools/generate_pregnancy_plate_assets.py`.

- **Provenance**: converted by `tools/home_lab/stipple_art.py` from the owner's
  8×5 reference sheet (`assets/concepts/references/week-fruit-engraving-sheet.webp`,
  supplied 2026-10-16, licence not recorded). The source objects are only ~100 px, so
  these are upscaled and soft. Replace them with high-resolution art for release.
- **Replace one plate**: put a pure black/white 340×340 PNG with the same file name
  here, then run the generator and the host tests. Re-create the whole set with
  `tools/home_lab/stipple_art.py <dir> --sheet <sheet> --size 340`.
- **Rule exception**: dithered fills conflict with the "no dither / no hatch shading"
  rule in `CLAUDE.md`; this page follows the owner's explicit design direction.
- Captions (English, shown under each plate) live in the generator.
- Numerals use Bebas Neue (SIL OFL 1.1). The generator reads it from the macOS system
  font folder by default; pass `--font` to use another condensed OFL font.
