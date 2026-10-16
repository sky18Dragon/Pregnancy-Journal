#!/usr/bin/env python3
"""Rasterize display typography for the home-page lab as 1-bit pixel assets.

The firmware only ships small UI text sizes, so poster-style numerals need
their own bitmaps. This renders the digits 0-9 (a high-contrast "Didone" serif)
at two heights and the label 孕周 (Chinese serif), downsampling from 4x with
a hairline-preserving threshold, and writes display_assets.inc for home_lab.cpp.

LICENSING: the default fonts are macOS system fonts. They are used only to
prototype and the generated .inc is written to build/ (never committed). Before
anything reaches firmware, regenerate from an OFL font (for example Anton or Archivo Black
for digits, Noto Sans SC for 孕周) with --digits-font /
--label-font.

Usage: make_display_assets.py <out_dir> [--digits-font PATH[:index]] [--label-font PATH[:index]]
"""
import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# Tear-off-calendar numerals (heavy condensed grotesque) and a plain sans label.
DEFAULT_DIGITS = "/System/Library/Fonts/HelveticaNeue.ttc|Condensed Black"
DEFAULT_LABEL = "/System/Library/Fonts/STHeiti Medium.ttc"
SIZES = {"Xl": 200, "Lg": 160, "Md": 120}  # digit heights in px
PLATE_DIGITS = "/System/Library/Fonts/HelveticaNeue.ttc|Condensed Bold"   # tear-off-plate numerals
PLATE_SIZES = {"PlateMd": 72, "PlateLg": 84}
LABEL_HEIGHT = 30
SUPER = 4
THRESHOLD = 150  # < 128 would thin Didone hairlines away on a 1-bit panel


def open_font(spec, size, prefer=None):
    spec, _, style = spec.partition("|")
    prefer = style or prefer
    path, _, index = spec.partition(":")
    if index:
        return ImageFont.truetype(path, size, index=int(index))
    if prefer:
        for i in range(12):
            try:
                font = ImageFont.truetype(path, size, index=i)
            except OSError:
                break
            if font.getname()[1] == prefer:
                return font
    return ImageFont.truetype(path, size)


def render(text, font, pad=8):
    left, top, right, bottom = font.getbbox(text)
    img = Image.new("L", (right - left + pad * 2, bottom - top + pad * 2), 255)
    ImageDraw.Draw(img).text((pad - left, pad - top), text, font=font, fill=0)
    return img


def to_bits(img):
    """Tight 1-bit crop: returns (width, height, MSB-first row-padded bytes)."""
    mono = img.point(lambda v: 255 if v < THRESHOLD else 0)  # 255 = ink
    bbox = mono.getbbox()
    mono = mono.crop(bbox)
    w, h = mono.size
    stride = (w + 7) // 8
    data = bytearray(stride * h)
    px = mono.load()
    for y in range(h):
        for x in range(w):
            if px[x, y]:
                data[y * stride + x // 8] |= 1 << (7 - x % 8)
    return w, h, data


def digit_set(spec, height):
    font = open_font(spec, 400)
    glyphs = {}
    for ch in "0123456789":
        # Shared baseline so every digit sits in the same vertical window.
        img = Image.new("L", (900, 700), 255)
        ImageDraw.Draw(img).text((100, 520), ch, font=font, fill=0, anchor="ls")
        glyphs[ch] = img
    # Common vertical window so every digit shares one top and one baseline.
    tops, bottoms = [], []
    for ch, img in glyphs.items():
        box = img.point(lambda v: 255 if v < THRESHOLD else 0).getbbox()
        tops.append(box[1]); bottoms.append(box[3])
    # Use the median-height "1" for the window so 0/6/8/9 overshoots are kept
    # by taking the full range, but scale so the full range equals `height`.
    top, bottom = min(tops), max(bottoms)
    scale = height / (bottom - top)
    out = []
    for ch in "0123456789":
        img = glyphs[ch]
        box = img.point(lambda v: 255 if v < THRESHOLD else 0).getbbox()
        crop = img.crop((box[0], top, box[2], bottom))
        size = (max(1, round(crop.width * scale)), height)
        small = crop.resize(size, Image.LANCZOS)
        w, h, data = to_bits(Image.merge("L", [small]) if small.mode == "L" else small)
        out.append((w, h, data))
    return out


def emit_array(name, data):
    rows = [", ".join(f"0x{b:02x}" for b in data[i:i + 16]) for i in range(0, len(data), 16)]
    return f"static const uint8_t {name}[] = {{\n  " + ",\n  ".join(rows) + "\n};\n"


TITLE_SIZES = {"Lg": 64, "Md": 52, "Sm": 40}
TITLE_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ/"


def title_fonts(spec):
    """Condensed heavy capitals for poster titles: A-Z, '/', and a blank for space."""
    font = open_font(spec, 400)
    glyph_imgs = {}
    for ch in TITLE_CHARS:
        img = Image.new("L", (900, 700), 255)
        ImageDraw.Draw(img).text((100, 520), ch, font=font, fill=0, anchor="ls")
        glyph_imgs[ch] = img
    boxes = {ch: im.point(lambda v: 255 if v < THRESHOLD else 0).getbbox() for ch, im in glyph_imgs.items()}
    top = min(b[1] for b in boxes.values()); bottom = max(b[3] for b in boxes.values())
    lines = ["struct TitleFont { PixelAsset glyph[28]; int advance[28]; int height; };", ""]
    for key, height in TITLE_SIZES.items():
        scale = height / (bottom - top)
        gap = max(3, height // 12)
        items, advs = [], []
        for i, ch in enumerate(TITLE_CHARS):
            b = boxes[ch]
            crop = glyph_imgs[ch].crop((b[0], top, b[2], bottom))
            small = crop.resize((max(1, round(crop.width * scale)), height), Image.LANCZOS)
            w, h, data = to_bits(small) if False else _bits_full(small)
            lines.append(emit_array(f"kTitle{key}{i}", data))
            items.append(f"{{{w}, {h}, kTitle{key}{i}}}")
            advs.append(str(w + gap))
        items.append("{0, 0, nullptr}")
        advs.append(str(round(height * 0.34)))
        lines.append(f"static const TitleFont kTitle{key} = {{{{{', '.join(items)}}}, {{{', '.join(advs)}}}, {height}}};\n")
    return lines


def _bits_full(img):
    """1-bit pack WITHOUT cropping (keeps the shared vertical window)."""
    mono = img.point(lambda v: 255 if v < THRESHOLD else 0)
    w, h = mono.size
    stride = (w + 7) // 8
    data = bytearray(stride * h)
    px = mono.load()
    for y in range(h):
        for x in range(w):
            if px[x, y]:
                data[y * stride + x // 8] |= 1 << (7 - x % 8)
    return w, h, data


def pack_art(art_dir):
    """Pack week-NN-*.png (ink = black) as 280 px and 150 px PixelAssets indexed by week-6."""
    files = {}
    for f in sorted(art_dir.glob("week-*-*.png")) if art_dir.exists() else []:
        files[int(f.name.split("-")[1])] = f
    if not files:
        return ["#define HOME_LAB_HAVE_ART 0", ""]
    lines = ["#define HOME_LAB_HAVE_ART 1", ""]
    big, small = [], []
    for week in range(6, 41):
        if week not in files:
            big.append("{0, 0, nullptr}"); small.append("{0, 0, nullptr}")
            continue
        img = Image.open(files[week]).convert("L")
        for tag, size, table in (("Big", img.size[0], big), ("Small", 150, small)):
            im = img if img.size == (size, size) else img.resize((size, size), Image.LANCZOS)
            ink = im.point(lambda v: 255 if v < 140 else 0)
            stride = (size + 7) // 8
            data = bytearray(stride * size)
            px = ink.load()
            for y in range(size):
                for x in range(size):
                    if px[x, y]:
                        data[y * stride + x // 8] |= 1 << (7 - x % 8)
            lines.append(emit_array(f"kArt{tag}{week}", data))
            table.append(f"{{{size}, {size}, kArt{tag}{week}}}")
    lines.append("static const PixelAsset kArtBig[35] = {" + ", ".join(big) + "};")
    lines.append("static const PixelAsset kArtSmall[35] = {" + ", ".join(small) + "};\n")
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    ap.add_argument("--digits-font", default=DEFAULT_DIGITS)
    ap.add_argument("--label-font", default=DEFAULT_LABEL)
    ap.add_argument("--art-dir", default="assets/concepts/geo-week-size/solid",
                    help="folder of week-NN-slug.png illustrations to pack (optional)")
    args = ap.parse_args()
    for spec in (args.digits_font, args.label_font):
        if not Path(spec.partition("|")[0].partition(":")[0]).exists():
            sys.exit(f"font not found: {spec}\n(pass --digits-font/--label-font; use OFL fonts for anything shipped)")

    lines = ["// Generated by make_display_assets.py - do not commit (font licensing).",
             "#pragma once", "#include <cstdint>", '#include "pixel_asset.h"', "",
             "struct NumeralFont { PixelAsset digit[10]; int advance[10]; int height; };", ""]
    for key, height in SIZES.items():
        glyphs = digit_set(args.digits_font, height)
        gap = max(4, height // 18)
        for i, (w, h, data) in enumerate(glyphs):
            lines.append(emit_array(f"k{key}Digit{i}", data))
        items = ", ".join(f"{{{w}, {h}, k{key}Digit{i}}}" for i, (w, h, _) in enumerate(glyphs))
        adv = ", ".join(str(w + gap) for w, _, _ in glyphs)
        lines.append(f"static const NumeralFont kNumeral{key} = {{{{{items}}}, {{{adv}}}, {height}}};\n")

    for key, height in PLATE_SIZES.items():
        glyphs = digit_set(PLATE_DIGITS, height)
        gap = max(3, height // 12)
        for i, (w, h, data) in enumerate(glyphs):
            lines.append(emit_array(f"k{key}Digit{i}", data))
        items = ", ".join(f"{{{w}, {h}, k{key}Digit{i}}}" for i, (w, h, _) in enumerate(glyphs))
        adv = ", ".join(str(w + gap) for w, _, _ in glyphs)
        lines.append(f"static const NumeralFont kNumeral{key} = {{{{{items}}}, {{{adv}}}, {height}}};\n")

    label_font = open_font(args.label_font, LABEL_HEIGHT * SUPER)
    img = render("孕周", label_font)
    w, h, data = to_bits(img)
    scale = LABEL_HEIGHT / h
    small = img.resize((round(img.width * scale), round(img.height * scale)), Image.LANCZOS)
    w, h, data = to_bits(small)
    lines.append(emit_array("kLabelWeekBits", data))
    lines.append(f"static const PixelAsset kLabelWeek = {{{w}, {h}, kLabelWeekBits}};\n")

    lines += title_fonts(args.digits_font)
    lines += pack_art(Path(args.art_dir))
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / "display_assets.inc").write_text("\n".join(lines), encoding="utf-8")
    # Plain PNG proof of the numerals for quick inspection.
    sheet = Image.new("L", (1800, SIZES["Xl"] + 40), 255)
    x = 20
    for w, h, data in digit_set(args.digits_font, SIZES["Xl"]):
        stride = (w + 7) // 8
        for yy in range(h):
            for xx in range(w):
                if data[yy * stride + xx // 8] & (1 << (7 - xx % 8)):
                    sheet.putpixel((x + xx, 20 + yy), 0)
        x += w + 16
    sheet.save(out / "numerals_proof.png")
    print(f"display assets -> {out}/display_assets.inc")


if __name__ == "__main__":
    main()
