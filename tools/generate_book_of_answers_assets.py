#!/usr/bin/env python3
"""Generate firmware-ready Book of Answers assets with FFmpeg."""

from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "book_of_answers" / "source"
PREVIEW_DIR = ROOT / "assets" / "book_of_answers" / "firmware"
OUTPUT_CPP = ROOT / "src" / "ui" / "assets" / "book_of_answers_assets.cpp"


@dataclass(frozen=True)
class AssetSpec:
    source_name: str
    preview_name: str
    symbol: str
    enum_name: str
    crop: tuple[int, int, int, int]
    output_size: tuple[int, int]


ASSETS = (
    AssetSpec(
        "home.png",
        "home.png",
        "kHomeData",
        "Home",
        (160, 410, 650, 830),
        (300, 385),
    ),
    AssetSpec(
        "shake_left.png",
        "shake_left.png",
        "kShakeLeftData",
        "ShakeLeft",
        (130, 430, 700, 830),
        (320, 380),
    ),
    AssetSpec(
        "shake_right.png",
        "shake_right.png",
        "kShakeRightData",
        "ShakeRight",
        (130, 430, 700, 830),
        (320, 380),
    ),
    AssetSpec(
        "thinking.png",
        "thinking.png",
        "kThinkingData",
        "Thinking",
        (160, 450, 650, 810),
        (300, 374),
    ),
    AssetSpec(
        "revealing.png",
        "revealing.png",
        "kRevealingData",
        "Revealing",
        (160, 450, 650, 810),
        (300, 374),
    ),
    AssetSpec(
        "message_result.png",
        "message_result.png",
        "kMessageResultData",
        "MessageResult",
        (160, 625, 650, 675),
        (310, 322),
    ),
    AssetSpec(
        "crystal_result.png",
        "crystal_peek.png",
        "kCrystalPeekData",
        "CrystalPeek",
        (655, 820, 316, 460),
        (140, 204),
    ),
)

BLACK_THRESHOLD = 220


def filter_graph(spec: AssetSpec) -> str:
    """Return the crop, scale, pad, and monochrome conversion graph."""

    x, y, width, height = spec.crop
    output_width, output_height = spec.output_size
    return (
        f"crop={width}:{height}:{x}:{y},"
        f"scale={output_width}:{output_height}:flags=area,"
        f"format=gray,lut=y='if(lt(val,{BLACK_THRESHOLD}),0,255)'"
    )


def render_asset(spec: AssetSpec) -> bytes:
    """Render one preview PNG and return its grayscale pixels."""

    source = SOURCE_DIR / spec.source_name
    preview = PREVIEW_DIR / spec.preview_name
    graph = filter_graph(spec)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

    subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(source),
            "-vf",
            graph,
            "-frames:v",
            "1",
            "-y",
            str(preview),
        ],
        check=True,
    )
    result = subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(source),
            "-vf",
            graph,
            "-frames:v",
            "1",
            "-f",
            "rawvideo",
            "-pix_fmt",
            "gray",
            "-",
        ],
        check=True,
        stdout=subprocess.PIPE,
    )
    output_width, output_height = spec.output_size
    expected_size = output_width * output_height
    if len(result.stdout) != expected_size:
        raise RuntimeError(
            f"{spec.source_name}: expected {expected_size} bytes, "
            f"got {len(result.stdout)}"
        )
    return result.stdout


def pack_pixels(grayscale: bytes, width: int, height: int) -> bytes:
    """Pack black pixels as MSB-first bits, padding each row to full bytes."""

    packed = bytearray()
    for y in range(height):
        row = grayscale[y * width:(y + 1) * width]
        for x in range(0, width, 8):
            value = 0
            for bit, pixel in enumerate(row[x:x + 8]):
                if pixel < 128:
                    value |= 1 << (7 - bit)
            packed.append(value)
    return bytes(packed)


def format_array(symbol: str, data: bytes) -> str:
    """Format packed bytes as one readable C++ constant array."""

    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset:offset + 12]
        lines.append(
            "    " + ", ".join(f"0x{value:02X}" for value in chunk) + ","
        )
    return (
        f"constexpr uint8_t {symbol}[] = {{\n"
        + "\n".join(lines)
        + "\n};\n"
    )


def write_cpp(packed_assets: dict[str, bytes]) -> None:
    """Write packed arrays and the public asset lookup function."""

    sections = [
        '#include "book_of_answers_assets.h"\n\n',
        "namespace {\n\n",
    ]
    for spec in ASSETS:
        sections.append(format_array(spec.symbol, packed_assets[spec.symbol]))
        sections.append("\n")
    sections.extend(
        [
            "}  // namespace\n\n",
            "const PixelAsset &book_of_answers_asset(BookOfAnswersAssetId id)\n",
            "{\n",
            "    switch (id) {\n",
        ]
    )
    for spec in ASSETS:
        width, height = spec.output_size
        sections.extend(
            [
                f"    case BookOfAnswersAssetId::{spec.enum_name}: {{\n",
                "        static constexpr PixelAsset asset = "
                f"{{{width}, {height}, {spec.symbol}}};\n",
                "        return asset;\n",
                "    }\n",
            ]
        )
    sections.extend(
        [
            "    }\n",
            "    static constexpr PixelAsset fallback = "
            "{300, 385, kHomeData};\n",
            "    return fallback;\n",
            "}\n",
        ]
    )
    OUTPUT_CPP.write_text("".join(sections), encoding="utf-8")


def main() -> None:
    """Generate every preview and packed firmware asset."""

    packed_assets = {}
    for spec in ASSETS:
        grayscale = render_asset(spec)
        width, height = spec.output_size
        packed_assets[spec.symbol] = pack_pixels(grayscale, width, height)
    write_cpp(packed_assets)
    print(f"Generated {len(ASSETS)} assets in {OUTPUT_CPP}")


if __name__ == "__main__":
    main()
