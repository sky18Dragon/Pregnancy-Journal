#!/usr/bin/env python3
"""Generate firmware-ready 1-bit pet animation assets with FFmpeg."""

from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "pixel_bunnies" / "source"
PREVIEW_DIR = ROOT / "assets" / "pixel_bunnies" / "firmware" / "pet_animation"
OUTPUT_CPP = ROOT / "src" / "ui" / "assets" / "pet_animation_assets.cpp"


@dataclass(frozen=True)
class AssetSpec:
    filename: str
    preview_name: str
    symbol: str
    enum_name: str
    crop: tuple[int, int, int, int]


ASSETS = (
    AssetSpec("welcome.png", "pet_wave.png", "kWaveData", "Wave", (230, 60, 900, 1140)),
    AssetSpec("pet_crouch.png", "pet_crouch.png", "kCrouchData", "Crouch", (160, 270, 930, 820)),
    AssetSpec("pet_jump.png", "pet_jump.png", "kJumpData", "Jump", (190, 120, 900, 1010)),
    AssetSpec("pet_celebrate.png", "pet_celebrate.png", "kCelebrateData", "Celebrate", (170, 60, 950, 1130)),
    AssetSpec("pet_walk_left.png", "pet_walk_left.png", "kWalkLeftData", "WalkLeft", (240, 70, 780, 1120)),
)

ASSET_SIZE = 96
CONTENT_SIZE = 90
BLACK_THRESHOLD = 220


def filter_graph(spec: AssetSpec) -> str:
    """Return the crop, scale, pad, and monochrome conversion graph."""

    x, y, width, height = spec.crop
    return (
        f"crop={width}:{height}:{x}:{y},"
        f"scale={CONTENT_SIZE}:{CONTENT_SIZE}:"
        "force_original_aspect_ratio=decrease:flags=area,"
        f"pad={ASSET_SIZE}:{ASSET_SIZE}:(ow-iw)/2:(oh-ih)/2:color=white,"
        f"format=gray,lut=y='if(lt(val,{BLACK_THRESHOLD}),0,255)'"
    )


def render_asset(spec: AssetSpec) -> bytes:
    """Render one preview PNG and return its 96x96 grayscale pixels."""

    source = SOURCE_DIR / spec.filename
    preview = PREVIEW_DIR / spec.preview_name
    graph = filter_graph(spec)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

    subprocess.run(
        [
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(source),
            "-vf", graph, "-frames:v", "1", "-y", str(preview),
        ],
        check=True,
    )
    result = subprocess.run(
        [
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-i", str(source),
            "-vf", graph, "-frames:v", "1", "-f", "rawvideo",
            "-pix_fmt", "gray", "-",
        ],
        check=True,
        stdout=subprocess.PIPE,
    )
    expected_size = ASSET_SIZE * ASSET_SIZE
    if len(result.stdout) != expected_size:
        raise RuntimeError(
            f"{spec.filename}: expected {expected_size} bytes, got {len(result.stdout)}"
        )
    return result.stdout


def pack_pixels(grayscale: bytes) -> bytes:
    """Pack black pixels as MSB-first bits, one row at a time."""

    packed = bytearray()
    for y in range(ASSET_SIZE):
        row = grayscale[y * ASSET_SIZE:(y + 1) * ASSET_SIZE]
        for x in range(0, ASSET_SIZE, 8):
            value = 0
            for bit, pixel in enumerate(row[x:x + 8]):
                if pixel < 128:
                    value |= 1 << (7 - bit)
            packed.append(value)
    return bytes(packed)


def format_array(symbol: str, data: bytes) -> str:
    """Format packed bytes as a readable C++ constant array."""

    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset:offset + 12]
        lines.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return f"constexpr uint8_t {symbol}[] = {{\n" + "\n".join(lines) + "\n};\n"


def write_cpp(packed_assets: dict[str, bytes]) -> None:
    """Write generated arrays and the public pose-to-asset lookup."""

    sections = [
        "#include \"pet_animation_assets.h\"\n\n",
        "namespace {\n\n",
    ]
    for spec in ASSETS:
        sections.append(format_array(spec.symbol, packed_assets[spec.symbol]))
        sections.append("\n")
    sections.extend(
        [
            "}  // namespace\n\n",
            "const PixelAsset &pet_animation_asset(PetAnimationPose pose)\n",
            "{\n",
            "    switch (pose) {\n",
        ]
    )
    for spec in ASSETS:
        sections.extend(
            [
                f"    case PetAnimationPose::{spec.enum_name}: {{\n",
                f"        static constexpr PixelAsset asset = "
                f"{{{ASSET_SIZE}, {ASSET_SIZE}, {spec.symbol}}};\n",
                "        return asset;\n",
                "    }\n",
            ]
        )
    sections.extend(
        [
            "    }\n",
            f"    static constexpr PixelAsset fallback = "
            f"{{{ASSET_SIZE}, {ASSET_SIZE}, kWaveData}};\n",
            "    return fallback;\n",
            "}\n",
        ]
    )
    OUTPUT_CPP.write_text("".join(sections), encoding="utf-8")


def main() -> None:
    packed_assets = {
        spec.symbol: pack_pixels(render_asset(spec)) for spec in ASSETS
    }
    write_cpp(packed_assets)
    print(f"Generated {len(ASSETS)} assets in {OUTPUT_CPP}")


if __name__ == "__main__":
    main()
