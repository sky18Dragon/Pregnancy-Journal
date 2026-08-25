#!/usr/bin/env python3

"""Generate eight portrait monochrome tutorial assets from approved pages."""

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

FINAL_PAGE_SOURCES = (
    ROOT / "assets/onboarding/redraws/tutorial-page-1-480x800-v3.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-2-480x800-v3.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-3-480x800-v1.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-4-480x800-v3.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-5-480x800-v2.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-6-480x800-v1.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-7-480x800-v8.png",
    ROOT / "assets/onboarding/redraws/tutorial-page-8-480x800-v8.png",
)


def render_page(source: Path) -> bytes:
    if not source.is_file():
        raise FileNotFoundError(source)
    command = [
        "ffmpeg",
        "-v",
        "error",
        "-i",
        str(source),
        "-vf",
        f"scale={SCREEN_WIDTH}:{SCREEN_HEIGHT}:flags=neighbor,format=gray",
        "-f",
        "rawvideo",
        "-pix_fmt",
        "gray",
        "-",
    ]
    rendered = subprocess.check_output(command)
    if len(rendered) != SCREEN_WIDTH * SCREEN_HEIGHT:
        raise RuntimeError("Unexpected FFmpeg output size")
    return rendered


def threshold_pixels(grayscale: bytes) -> bytes:
    return bytes(0 if value < BLACK_THRESHOLD else 255 for value in grayscale)


def pack_monochrome(monochrome: bytes) -> bytes:
    packed = bytearray()
    for offset in range(0, len(monochrome), 8):
        value = 0
        for bit, pixel in enumerate(monochrome[offset : offset + 8]):
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


def write_cpp(pages: list[bytes]) -> None:
    HEADER.write_text(
        """#pragma once

#include <cstdint>

#include \"pixel_asset.h\"

constexpr uint8_t kOnboardingAssetCount = 8U;

// Returns one approved full-screen tutorial bitmap.
// 返回一张已确认的全屏教程位图。
const PixelAsset &onboarding_asset(uint8_t page_index);
""",
        encoding="utf-8",
    )

    arrays = "\n".join(
        format_array(f"kOnboardingPage{index + 1}Data", data)
        for index, data in enumerate(pages)
    )
    assets = "\n".join(
        f"constexpr PixelAsset kOnboardingPage{index + 1} = "
        f"{{{SCREEN_WIDTH}, {SCREEN_HEIGHT}, "
        f"kOnboardingPage{index + 1}Data}};"
        for index in range(len(pages))
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
        return kOnboardingPage8;
    }}
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
            monochrome = threshold_pixels(render_page(source))
            pgm_path = temporary_dir / f"tutorial-page-{index}.pgm"
            png_path = PREVIEW_DIR / f"tutorial-page-{index}.png"
            write_pgm(pgm_path, monochrome)
            write_png(pgm_path, png_path)
            packed_pages.append(pack_monochrome(monochrome))
    write_cpp(packed_pages)


if __name__ == "__main__":
    main()
