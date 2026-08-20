# Desktop Pet Assets

This directory stores the original monochrome visual system for the Sticky desktop pet.

## Directories

- `concepts/`: character lineage and visual-development references.
- `source/`: approved high-resolution source artwork for each life stage and action.
- `firmware/`: generated one-bit previews that match the packed firmware assets.
- `qa/`: portrait firmware render and side-by-side reference comparison.
- `library/`: audited dialogue sources and machine-readable behavior rules with
  exact upstream provenance.

## Current Reference

`concepts/growth_lineage_v1.png` shows the shared Egg, Hatchling, and Child stages followed by Foodie, Affectionate, and Active Youth-to-Adult branches.

`concepts/home_selected_v1.png` is the approved portrait home-screen reference. `concepts/home_selected_480x800.png` is its normalized comparison copy.

The current `source/` set contains the approved room, Hatchling and Child identity art, plus three distinct Youth and Adult personality forms. Every Youth and Adult form has its own idle, signature movement, feed, pet, and play poses. The `firmware/` directory contains the generated 1-bit previews, subject masks, four action icons, and love icon.

Run the asset generator from the project root:

```bash
python3 tools/generate_desktop_pet_assets.py
```

The generator crops, scales, thresholds, creates an opaque character mask, and writes `src/ui/assets/desktop_pet_assets.cpp`. The mask lets each white rabbit body cover room lines while the surrounding background remains transparent.
