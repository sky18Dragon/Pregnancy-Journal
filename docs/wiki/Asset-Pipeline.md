# Asset Pipeline

Sticky Bunny artwork begins as monochrome PNG source, is normalized into firmware-sized images, then converted into generated C++ arrays consumed by `PixelAsset`.

Install the image-generation dependency once before rebuilding source artwork:

```bash
python3 -m pip install -r requirements-dev.txt
```

## Asset directories

```text
assets/
├── app_launcher/       # Four launcher stickers and QA layouts
├── book_of_answers/    # Crystal ball, message and motion frames
├── desktop_pet/        # Life stages, actions, room and design references
├── onboarding/         # Six final tutorial pages
├── pixel_bunnies/      # Status and reusable pet poses
└── pomodoro/           # Tomato-rabbit illustration
```

`source/` preserves editable images. `firmware/` contains normalized inputs used by generators. `qa/`, `concepts/` and `previews/` explain layout or design intent and are not compiled automatically.

## Generators

```bash
python3 tools/generate_app_launcher_assets.py
python3 tools/generate_book_of_answers_assets.py
python3 tools/generate_book_of_answers_database.py
python3 tools/generate_desktop_pet_assets.py
python3 tools/generate_onboarding_assets.py
python3 tools/generate_pet_animation.py
python3 tools/generate_pixel_bunnies.py
python3 tools/generate_pomodoro_assets.py
```

Generated code is written under `src/ui/assets/`. Re-running a generator with unchanged inputs should produce no Git diff.

## Visual rules

- Final screens use black, white and bounded grayscale that survives the physical panel.
- Primary time or status information receives the largest visual area.
- Visible icons can remain small when their touch targets are intentionally larger.
- App buttons share a consistent visible height and alignment.
- Portrait artwork is prepared for 480 × 800 logical coordinates; landscape artwork uses 800 × 480.
- Animation changes use meaningful poses rather than scaling a static character.

## Database source

The Book of Answers CSV is stored with its Apache License 2.0 text. Database generation converts the source into firmware strings while preserving the upstream notice.

## Design references

Selected historical concepts and final comparisons are in [Design and Asset Gallery](Design-and-Asset-Gallery.md). They explain how the visual language developed; the actual firmware-ready source remains under `assets/`.
