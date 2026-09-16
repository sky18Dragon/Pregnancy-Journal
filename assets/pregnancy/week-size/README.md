# Weekly baby-size illustrations

This directory contains the replaceable source artwork for pregnancy weeks
6–40. Every `week-NN-name.png` was generated with OpenAI ImageGen as a
black-and-white e-paper pixel illustration with no text.

- Source PNGs live in this directory.
- Normalized 148×148, 1-bit previews live in `firmware/`.
- `src/ui/assets/pregnancy_week_assets.cpp` is generated firmware data.

## Replace an illustration

1. Replace the matching source PNG without changing its filename.
2. Keep one centered object on a white square canvas, with no text or border.
3. Run `python3 tools/generate_pregnancy_week_assets.py`.
4. Run `tools/run_host_tests.sh` and build the three PlatformIO environments.

The generator crops exterior whitespace, fits the object inside a 148×148
canvas with an 8 px safety margin, converts it to pure 1-bit black/white, checks
the black-pixel ratio, emits the normalized preview, and repacks the C++ asset.

## Week mapping

| Week | Object | Week | Object | Week | Object |
|---:|---|---:|---|---:|---|
| 6 | blueberry | 18 | bell pepper | 30 | cabbage |
| 7 | coffee bean | 19 | mango | 31 | pineapple |
| 8 | raspberry | 20 | banana | 32 | jicama |
| 9 | cherries | 21 | carrot | 33 | celery |
| 10 | strawberry | 22 | coconut | 34 | cantaloupe |
| 11 | whole apricot | 23 | grapefruit | 35 | whole small winter melon |
| 12 | whole kiwi | 24 | corn | 36 | romaine lettuce |
| 13 | lemon | 25 | cauliflower | 37 | Swiss chard |
| 14 | peach | 26 | lettuce | 38 | leek |
| 15 | apple | 27 | zucchini | 39 | watermelon |
| 16 | avocado | 28 | eggplant | 40 | pumpkin |
| 17 | pomegranate | 29 | acorn squash |  |  |

## ImageGen prompt pattern

Mode: generate. Use case: production UI icon.

> Draw one large `[OBJECT]`, centered on a plain pure-white square canvas and
> filling about 70% of its height. Bold black-and-white 1-bit e-paper pixel
> art, chunky crisp pixels, strong black outline, sparse internal details,
> pure black and white only. No transparency, grayscale, soft shading, glow,
> shadow, border, text, letters, numbers, labels, watermark, hands, faces, or
> extra objects. It must remain recognizable at 148×148 pixels.

All illustrations use a whole exterior view. Cut faces, cross-sections, and
anatomical imagery are intentionally excluded.
