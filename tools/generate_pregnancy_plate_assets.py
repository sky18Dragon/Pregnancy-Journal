#!/usr/bin/env python3
"""Pack the pregnancy home-page "plates" into firmware assets.

Inputs
  assets/pregnancy/week-plates/week-NN-name.png   340x340 pure black/white stipple
      engravings for weeks 6-40 (black = ink), used by both orientations. Produce or
      replace them with `python3 tools/home_lab/stipple_art.py <dir> --sheet
      <engraving sheet> --size 340`, or drop in your own 340x340 PNGs.
  a condensed display font for the big week numerals (default: Bebas Neue).

Output
  src/ui/assets/pregnancy_plate_assets.{h,cpp}: the 35 plates, their English
  captions and two sizes of 0-9 numeral bitmaps.

Run `python3 tools/generate_pregnancy_plate_assets.py [--font PATH]` after changing
any input and commit the regenerated files.
"""
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "pregnancy" / "week-plates"
OUTPUT_CPP = ROOT / "src" / "ui" / "assets" / "pregnancy_plate_assets.cpp"
OUTPUT_H = ROOT / "src" / "ui" / "assets" / "pregnancy_plate_assets.h"

PLATE_SIZE = 340
FIRST_WEEK, LAST_WEEK = 6, 40
NUMERAL_HEIGHT = 84
INK_THRESHOLD = 140
MIN_MARGIN = 8   # px of clear paper required between the ink and the plate border
DEFAULT_FONT = ("/System/Library/PrivateFrameworks/FontServices.framework/Versions/A/"
                "Resources/Fonts/ApplicationSupport/BebasNeue.otf")

CAPTIONS = {
    6: "LENTIL", 7: "GRAPE", 8: "KIDNEY BEAN", 9: "CHERRY", 10: "STRAWBERRY", 11: "LIME",
    12: "PLUM", 13: "LEMON", 14: "AVOCADO", 15: "APPLE", 16: "ORANGE", 17: "POMEGRANATE",
    18: "MANGO", 19: "GRAPEFRUIT", 20: "BANANA", 21: "CARROT", 22: "CORN", 23: "BELL PEPPER",
    24: "EGGPLANT", 25: "RUTABAGA", 26: "CAULIFLOWER", 27: "CABBAGE", 28: "BUTTERNUT SQUASH",
    29: "COCONUT", 30: "HONEYDEW", 31: "PINEAPPLE", 32: "JICAMA", 33: "ROMAINE LETTUCE",
    34: "CANTALOUPE", 35: "WINTER MELON", 36: "NAPA CABBAGE", 37: "WATERMELON", 38: "PUMPKIN",
    39: "SPAGHETTI SQUASH", 40: "JACKFRUIT",
}

CAPTIONS_ZH = {
    6: "扁豆", 7: "葡萄", 8: "芸豆", 9: "樱桃", 10: "草莓", 11: "青柠",
    12: "李子", 13: "柠檬", 14: "牛油果", 15: "苹果", 16: "橙子", 17: "石榴",
    18: "芒果", 19: "西柚", 20: "香蕉", 21: "胡萝卜", 22: "玉米", 23: "甜椒",
    24: "茄子", 25: "芜菁甘蓝", 26: "花菜", 27: "卷心菜", 28: "奶油南瓜",
    29: "椰子", 30: "蜜瓜", 31: "菠萝", 32: "凉薯", 33: "罗马生菜",
    34: "哈密瓜", 35: "冬瓜", 36: "大白菜", 37: "西瓜", 38: "南瓜",
    39: "金丝瓜", 40: "菠萝蜜",
}


def pack(image: Image.Image) -> bytes:
    """MSB-first, row-padded 1-bit data; black pixels are ink (bit set)."""
    width, height = image.size
    pixels = image.load()
    stride = (width + 7) // 8
    data = bytearray(stride * height)
    for y in range(height):
        for x in range(width):
            if pixels[x, y] < INK_THRESHOLD:
                data[y * stride + x // 8] |= 1 << (7 - x % 8)
    return bytes(data)


def format_array(name: str, data: bytes) -> str:
    rows = []
    for offset in range(0, len(data), 16):
        rows.append("    " + ", ".join(f"0x{b:02X}" for b in data[offset:offset + 16]) + ",")
    return f"constexpr uint8_t {name}[] = {{\n" + "\n".join(rows) + "\n};\n"


def load_plates(directory: Path, size: int) -> dict[int, bytes]:
    plates: dict[int, bytes] = {}
    for path in sorted(directory.glob("week-*-*.png")):
        week = int(path.name.split("-")[1])
        if not FIRST_WEEK <= week <= LAST_WEEK:
            continue
        image = Image.open(path).convert("L")
        if image.size != (size, size):
            raise ValueError(f"{path} must be {size}x{size}, got {image.size}")
        ink = image.point(lambda v: 255 if v < INK_THRESHOLD else 0).getbbox()
        if ink is None:
            raise ValueError(f"{path} is blank")
        margin = min(ink[0], ink[1], size - ink[2], size - ink[3])
        if margin < MIN_MARGIN:   # ink touching the border means the object was cropped
            raise ValueError(f"{path}: object runs to the plate edge (margin {margin} px < {MIN_MARGIN}); it is cut off")
        plates[week] = pack(image)
    missing = [w for w in range(FIRST_WEEK, LAST_WEEK + 1) if w not in plates]
    if missing:
        raise FileNotFoundError(f"missing plates for weeks {missing} in {directory}")
    return plates


def numerals(font_path: str, height: int) -> list[tuple[int, int, bytes]]:
    font = ImageFont.truetype(font_path, 400)
    canvases = {}
    for ch in "0123456789":
        img = Image.new("L", (900, 700), 255)
        ImageDraw.Draw(img).text((100, 520), ch, font=font, fill=0, anchor="ls")
        canvases[ch] = img
    boxes = {ch: im.point(lambda v: 255 if v < INK_THRESHOLD else 0).getbbox() for ch, im in canvases.items()}
    top = min(b[1] for b in boxes.values())
    bottom = max(b[3] for b in boxes.values())
    scale = height / (bottom - top)
    out = []
    for ch in "0123456789":
        b = boxes[ch]
        crop = canvases[ch].crop((b[0], top, b[2], bottom))
        small = crop.resize((max(1, round(crop.width * scale)), height), Image.LANCZOS)
        out.append((small.width, small.height, pack(small)))
    return out


def write_header() -> None:
    OUTPUT_H.write_text('''#pragma once

#include <cstdint>

#include "pixel_asset.h"

// Generated by tools/generate_pregnancy_plate_assets.py - do not edit by hand.
// 生成文件，请勿手工修改。

// 340x340 1-bit engraving ("plate") for weeks 6-40; other weeks clamp to the
// nearest one. Black bits are ink.
const PixelAsset &pregnancy_plate_asset(uint8_t week);

// Upper-case English caption of the plate's object ("CARROT") and its Chinese name.
const char *pregnancy_plate_caption(uint8_t week);
const char *pregnancy_plate_caption_zh(uint8_t week);

// Condensed bold digits 0-9, all the same height, tight-cropped horizontally.
struct PlateNumeral {
  PixelAsset digit[10];
  uint8_t advance[10]; // pen advance including the gap to the next digit
  uint8_t height;
};

const PlateNumeral &pregnancy_plate_numeral(); // 84 px high
''', encoding="utf-8")


def write_cpp(plates: dict[int, bytes], font_path: str) -> None:
    parts = ['#include "pregnancy_plate_assets.h"\n\n// Generated by tools/generate_pregnancy_plate_assets.py - do not edit by hand.\n\n',
             "namespace {\n\n"]
    for week in range(FIRST_WEEK, LAST_WEEK + 1):
        parts.append(format_array(f"kPlate{week:02d}Data", plates[week]))
        parts.append("\n")
    refs = ", ".join(f"{{{PLATE_SIZE}, {PLATE_SIZE}, kPlate{w:02d}Data}}" for w in range(FIRST_WEEK, LAST_WEEK + 1))
    parts.append(f"constexpr PixelAsset kPlates[] = {{{refs}}};\n\n")

    caps = ",\n    ".join(f'"{CAPTIONS[w]}"' for w in range(FIRST_WEEK, LAST_WEEK + 1))
    parts.append(f"constexpr const char *kCaptions[] = {{\n    {caps}}};\n\n")
    caps_zh = ",\n    ".join(f'"{CAPTIONS_ZH[w]}"' for w in range(FIRST_WEEK, LAST_WEEK + 1))
    parts.append(f"constexpr const char *kCaptionsZh[] = {{\n    {caps_zh}}};\n\n")
    glyphs = numerals(font_path, NUMERAL_HEIGHT)
    gap = max(3, NUMERAL_HEIGHT // 12)
    for index, (_, _, data) in enumerate(glyphs):
        parts.append(format_array(f"kNumeral{index}Data", data))
    items = ", ".join(f"{{{w}, {h}, kNumeral{i}Data}}" for i, (w, h, _) in enumerate(glyphs))
    adv = ", ".join(str(w + gap) for w, _, _ in glyphs)
    parts.append(f"const PlateNumeral kNumeral = {{{{{items}}}, {{{adv}}}, {NUMERAL_HEIGHT}}};\n\n")
    parts.append("} // namespace\n\n")
    parts.append(f'''const PixelAsset &pregnancy_plate_asset(uint8_t week)
{{
    const uint8_t clamped = week < {FIRST_WEEK} ? {FIRST_WEEK} : (week > {LAST_WEEK} ? {LAST_WEEK} : week);
    return kPlates[clamped - {FIRST_WEEK}];
}}

const char *pregnancy_plate_caption(uint8_t week)
{{
    const uint8_t clamped = week < {FIRST_WEEK} ? {FIRST_WEEK} : (week > {LAST_WEEK} ? {LAST_WEEK} : week);
    return kCaptions[clamped - {FIRST_WEEK}];
}}

const char *pregnancy_plate_caption_zh(uint8_t week)
{{
    const uint8_t clamped = week < {FIRST_WEEK} ? {FIRST_WEEK} : (week > {LAST_WEEK} ? {LAST_WEEK} : week);
    return kCaptionsZh[clamped - {FIRST_WEEK}];
}}

const PlateNumeral &pregnancy_plate_numeral()
{{
    return kNumeral;
}}
''')
    OUTPUT_CPP.write_text("".join(parts), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", default=DEFAULT_FONT, help="condensed display font for the numerals")
    args = parser.parse_args()
    if not Path(args.font).exists():
        raise SystemExit(f"numeral font not found: {args.font} (pass --font; use an OFL font)")
    plates = load_plates(SOURCE_DIR, PLATE_SIZE)
    write_header()
    write_cpp(plates, args.font)
    print(f"{len(plates)} plates + numerals -> {OUTPUT_CPP.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
