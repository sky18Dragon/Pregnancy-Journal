#!/usr/bin/env python3
"""Estimate a bounded input-font strike from the committed generated asset.

This intentionally does not regenerate the 23 MiB C++ source. It measures the
real bitmap density of the existing 18 px and 24 px Medium strikes and scales
those values to GB2312 Level-1 (3,755 Han characters) plus printable ASCII.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


GLYPH_STRUCT_BYTES = 16  # ESP32-S3 ABI, verified from the compiled object.
GB2312_LEVEL1_CHARACTERS = 3755
PRINTABLE_ASCII_CHARACTERS = 95


def array_body(source: str, declaration: str) -> str:
    match = re.search(
        rf"constexpr\s+[^\n]+\s+{re.escape(declaration)}\[\]\s*=\s*\{{(.*?)\n\}};",
        source,
        re.DOTALL,
    )
    if match is None:
        raise ValueError(f"array not found: {declaration}")
    return match.group(1)


def strike_measurement(source: str, size: int) -> tuple[int, int]:
    glyph_body = array_body(source, f"kMedium{size}Glyphs")
    bitmap_body = array_body(source, f"kMedium{size}Bitmap")
    glyph_count = len(re.findall(r"\{0x[0-9A-F]+U,", glyph_body))
    bitmap_bytes = len(re.findall(r"0x[0-9A-F]{2}U", bitmap_body))
    return glyph_count, bitmap_bytes


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--asset",
        type=Path,
        default=Path("src/ui/assets/chinese_font_assets.cpp"),
    )
    args = parser.parse_args()
    source = args.asset.read_text(encoding="utf-8")
    target_glyphs = GB2312_LEVEL1_CHARACTERS + PRINTABLE_ASCII_CHARACTERS
    total_estimate = 0.0
    print("Chinese input font budget (Medium only)")
    print(f"target_glyphs={target_glyphs} (GB2312 Level-1 + printable ASCII)")
    for size in (18, 24):
        current_glyphs, current_bitmap_bytes = strike_measurement(source, size)
        average_bitmap_bytes = current_bitmap_bytes / current_glyphs
        estimate = target_glyphs * (GLYPH_STRUCT_BYTES + average_bitmap_bytes)
        total_estimate += estimate
        print(
            f"strike={size}px current_glyphs={current_glyphs} "
            f"bitmap_bytes={current_bitmap_bytes} "
            f"estimated_target_bytes={round(estimate)}"
        )
    print(f"estimated_total_bytes={round(total_estimate)}")
    print(f"estimated_with_20pct_margin_bytes={round(total_estimate * 1.2)}")


if __name__ == "__main__":
    main()
