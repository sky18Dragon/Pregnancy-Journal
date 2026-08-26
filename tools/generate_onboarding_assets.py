#!/usr/bin/env python3

"""Generate six portrait monochrome tutorial assets from approved pages."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PREVIEW_DIR = ROOT / "assets/onboarding/previews"
HEADER = ROOT / "src/ui/assets/onboarding_assets.h"
SOURCE_CPP = ROOT / "src/ui/assets/onboarding_assets.cpp"

SCREEN_WIDTH = 480
SCREEN_HEIGHT = 800
BLACK_THRESHOLD = 208
LOVE_ARROW_WIDTH = 47
LOVE_ARROW_HEIGHT = 22
LOVE_ARROW_SOURCE = (
    ROOT / "assets/onboarding/redraws/tutorial-love-arrow-47x22-v2.png"
)

FINAL_PAGE_SOURCES = (
    ROOT / "assets/onboarding/redraws/tutorial-page-1-scene-480x800-v4.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-2-480x800-v4.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-3-scene-480x800-v3.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-4-apps-480x800-v2.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-5-launcher-480x800-v6.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-6-actions-480x800-v1.png",
)


def render_asset(source: Path, width: int, height: int) -> bytes:
    if not source.is_file():
        raise FileNotFoundError(source)
    command = [
        "ffmpeg",
        "-v",
        "error",
        "-i",
        str(source),
        "-vf",
        f"scale={width}:{height}:flags=neighbor,format=gray",
        "-f",
        "rawvideo",
        "-pix_fmt",
        "gray",
        "-",
    ]
    rendered = subprocess.check_output(command)
    if len(rendered) != width * height:
        raise RuntimeError("Unexpected FFmpeg output size")
    return rendered


def threshold_pixels(grayscale: bytes) -> bytes:
    return bytes(0 if value < BLACK_THRESHOLD else 255 for value in grayscale)


def pack_monochrome(monochrome: bytes, width: int, height: int) -> bytes:
    packed = bytearray()
    for row in range(height):
        row_start = row * width
        for column in range(0, width, 8):
            value = 0
            for bit, pixel in enumerate(
                monochrome[row_start + column : row_start + column + 8]
            ):
                if pixel == 0:
                    value |= 1 << (7 - bit)
            packed.append(value)
    return bytes(packed)


def write_pgm(path: Path, monochrome: bytes) -> None:
    path.write_bytes(
        f"P5\n{SCREEN_WIDTH} {SCREEN_HEIGHT}\n255\n".encode("ascii")
        + monochrome
    )


def write_png(pgm_path: Path, png_path: Path) -> None:
    subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-y",
            "-i",
            str(pgm_path),
            str(png_path),
        ],
        check=True,
    )


def format_array(name: str, data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 16):
        chunk = data[offset : offset + 16]
        lines.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return f"constexpr uint8_t {name}[] = {{\n" + "\n".join(lines) + "\n};\n"


def write_cpp(pages: list[bytes], love_arrow: bytes) -> None:
    page_count = len(pages)
    HEADER.write_text(
        f"""#pragma once

#include <cstdint>

#include \"pixel_asset.h\"

constexpr uint8_t kOnboardingAssetCount = {page_count}U;

// Returns one approved full-screen tutorial bitmap.
// 返回一张已确认的全屏教程位图。
const PixelAsset &onboarding_asset(uint8_t page_index);

// Returns the hand-drawn LOVE callout arrow.
// 返回LOVE说明使用的手绘箭头。
const PixelAsset &onboarding_love_arrow_asset();
""",
        encoding="utf-8",
    )

    arrays = "\n".join(
        format_array(f"kOnboardingPage{index + 1}Data", data)
        for index, data in enumerate(pages)
    )
    arrays += "\n" + format_array("kOnboardingLoveArrowData", love_arrow)
    assets = "\n".join(
        f"constexpr PixelAsset kOnboardingPage{index + 1} = "
        f"{{{SCREEN_WIDTH}, {SCREEN_HEIGHT}, "
        f"kOnboardingPage{index + 1}Data}};"
        for index in range(len(pages))
    )
    assets += (
        "\nconstexpr PixelAsset kOnboardingLoveArrow = "
        f"{{{LOVE_ARROW_WIDTH}, {LOVE_ARROW_HEIGHT}, "
        "kOnboardingLoveArrowData};"
    )
    cases = "\n".join(
        f"    case {index}U:\n        return kOnboardingPage{index + 1};"
        for index in range(len(pages))
    )
    SOURCE_CPP.write_text(
        f"""#include \"onboarding_assets.h\"

namespace {{

{arrays}
{assets}

}}  // namespace

const PixelAsset &onboarding_asset(uint8_t page_index)
{{
    switch (page_index) {{
{cases}
    default:
        return kOnboardingPage{page_count};
    }}
}}

const PixelAsset &onboarding_love_arrow_asset()
{{
    return kOnboardingLoveArrow;
}}
""",
        encoding="utf-8",
    )


def main() -> None:
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    packed_pages = []
    with tempfile.TemporaryDirectory(prefix="sticky-onboarding-") as directory:
        temporary_dir = Path(directory)
        for index, source in enumerate(FINAL_PAGE_SOURCES, start=1):
            monochrome = threshold_pixels(
                render_asset(source, SCREEN_WIDTH, SCREEN_HEIGHT)
            )
            pgm_path = temporary_dir / f"tutorial-page-{index}.pgm"
            png_path = PREVIEW_DIR / f"tutorial-page-{index}.png"
            write_pgm(pgm_path, monochrome)
            write_png(pgm_path, png_path)
            packed_pages.append(
                pack_monochrome(monochrome, SCREEN_WIDTH, SCREEN_HEIGHT)
            )
    love_arrow = threshold_pixels(
        render_asset(LOVE_ARROW_SOURCE, LOVE_ARROW_WIDTH, LOVE_ARROW_HEIGHT)
    )
    write_cpp(
        packed_pages,
        pack_monochrome(love_arrow, LOVE_ARROW_WIDTH, LOVE_ARROW_HEIGHT),
    )


if __name__ == "__main__":
    main()
