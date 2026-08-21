#!/usr/bin/env python3
"""Pack normalized launcher sticker PNGs into firmware C++ arrays."""

from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "app_launcher" / "firmware"
OUTPUT_CPP = ROOT / "src" / "ui" / "assets" / "app_launcher_assets.cpp"


@dataclass(frozen=True)
class AssetSpec:
    source_name: str
    enum_name: str
    symbol_prefix: str


ASSETS = (
    AssetSpec("pet.png", "Pet", "kPet"),
    AssetSpec("focus.png", "Focus", "kFocus"),
    AssetSpec("status.png", "Status", "kStatus"),
    AssetSpec("answers.png", "Answers", "kAnswers"),
)

WIDTH = 176
HEIGHT = 176
BLACK_MAX = 84
GRAY_MAX = 220


def read_grayscale(path: Path) -> bytes:
    """Decode one normalized PNG into row-major grayscale pixels."""

    result = subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(path),
            "-vf",
            f"scale={WIDTH}:{HEIGHT}:flags=neighbor,format=gray",
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
    expected_size = WIDTH * HEIGHT
    if len(result.stdout) != expected_size:
        raise RuntimeError(
            f"{path.name}: expected {expected_size} bytes, "
            f"got {len(result.stdout)}"
        )
    return result.stdout


def pack_layer(grayscale: bytes, minimum: int, maximum: int) -> bytes:
    """Pack pixels in one inclusive range as MSB-first row bits."""

    packed = bytearray()
    for y in range(HEIGHT):
        row = grayscale[y * WIDTH:(y + 1) * WIDTH]
        for x in range(0, WIDTH, 8):
            value = 0
            for bit, pixel in enumerate(row[x:x + 8]):
                if minimum <= pixel <= maximum:
                    value |= 1 << (7 - bit)
            packed.append(value)
    return bytes(packed)


def format_array(symbol: str, data: bytes) -> str:
    """Format packed bytes as one readable C++ array."""

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


def write_cpp(layers: dict[str, tuple[bytes, bytes]]) -> None:
    """Write the packed layers and public lookup function."""

    sections = ['#include "app_launcher_assets.h"\n\n', "namespace {\n\n"]
    for spec in ASSETS:
        gray, black = layers[spec.enum_name]
        sections.append(format_array(f"{spec.symbol_prefix}GrayData", gray))
        sections.append("\n")
        sections.append(format_array(f"{spec.symbol_prefix}BlackData", black))
        sections.append("\n")
        sections.append(
            "constexpr AppLauncherStickerAsset "
            f"{spec.symbol_prefix}Asset = {{\n"
            f"    {{{WIDTH}, {HEIGHT}, {spec.symbol_prefix}GrayData}},\n"
            f"    {{{WIDTH}, {HEIGHT}, {spec.symbol_prefix}BlackData}},\n"
            "};\n\n"
        )
    sections.extend(
        [
            "}  // namespace\n\n",
            "const AppLauncherStickerAsset &app_launcher_sticker_asset(\n",
            "    AppLauncherAssetId id)\n",
            "{\n",
            "    switch (id) {\n",
        ]
    )
    for spec in ASSETS:
        sections.extend(
            [
                f"    case AppLauncherAssetId::{spec.enum_name}:\n",
                f"        return {spec.symbol_prefix}Asset;\n",
            ]
        )
    sections.extend(["    }\n", "    return kPetAsset;\n", "}\n"])
    OUTPUT_CPP.write_text("".join(sections), encoding="utf-8")


def main() -> None:
    """Generate both firmware layers for every launcher sticker."""

    layers: dict[str, tuple[bytes, bytes]] = {}
    for spec in ASSETS:
        grayscale = read_grayscale(SOURCE_DIR / spec.source_name)
        gray = pack_layer(grayscale, BLACK_MAX + 1, GRAY_MAX)
        black = pack_layer(grayscale, 0, BLACK_MAX)
        layers[spec.enum_name] = (gray, black)
    write_cpp(layers)


if __name__ == "__main__":
    main()
