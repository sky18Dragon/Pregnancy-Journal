#!/usr/bin/env python3
"""Generate firmware-ready 1-bit bunny assets with FFmpeg."""

from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "pixel_bunnies" / "source"
PREVIEW_DIR = ROOT / "assets" / "pixel_bunnies" / "firmware" / "80x80"
OUTPUT_CPP = ROOT / "src" / "ui" / "assets" / "status_bunny_assets.cpp"


@dataclass(frozen=True)
class AssetSpec:
    filename: str
    symbol: str
    enum_name: str
    crop: tuple[int, int, int, int]


ASSETS = (
    AssetSpec("focusing.png", "kFocusingData", "Focusing", (120, 120, 1014, 1014)),
    AssetSpec("in_meeting.png", "kInMeetingData", "InMeeting", (90, 210, 1074, 820)),
    AssetSpec("welcome.png", "kWelcomeData", "Welcome", (230, 60, 900, 1140)),
    AssetSpec("out_for_lunch.png", "kOutForLunchData", "OutForLunch", (210, 160, 850, 930)),
    AssetSpec("off_duty.png", "kOffDutyData", "OffDuty", (180, 100, 900, 1040)),
    AssetSpec("custom.png", "kCustomData", "Custom", (200, 90, 870, 1080)),
)

ANIMATION_ASSETS = (
    AssetSpec("focusing_alt.png", "kFocusingAltData", "Focusing", (120, 120, 1014, 1014)),
    AssetSpec("in_meeting_alt.png", "kInMeetingAltData", "InMeeting", (80, 220, 1100, 820)),
    AssetSpec("welcome_alt.png", "kWelcomeAltData", "Welcome", (230, 60, 900, 1140)),
    AssetSpec("out_for_lunch_alt.png", "kOutForLunchAltData", "OutForLunch", (180, 130, 900, 980)),
    AssetSpec("off_duty_alt.png", "kOffDutyAltData", "OffDuty", (180, 100, 900, 1040)),
    AssetSpec("custom_alt.png", "kCustomAltData", "Custom", (200, 90, 870, 1080)),
)

ALL_ASSETS = ASSETS + ANIMATION_ASSETS

ASSET_SIZE = 80
CONTENT_SIZE = 72
BLACK_THRESHOLD = 220


def filter_graph(spec: AssetSpec) -> str:
    """Return the shared crop, scale, pad, and monochrome conversion graph."""

    x, y, width, height = spec.crop
    return (
        f"crop={width}:{height}:{x}:{y},"
        f"scale={CONTENT_SIZE}:{CONTENT_SIZE}:"
        "force_original_aspect_ratio=decrease:flags=area,"
        f"pad={ASSET_SIZE}:{ASSET_SIZE}:(ow-iw)/2:(oh-ih)/2:color=white,"
        f"format=gray,lut=y='if(lt(val,{BLACK_THRESHOLD}),0,255)'"
    )


def render_asset(spec: AssetSpec) -> bytes:
    """Render one preview PNG and return its 80x80 grayscale pixel bytes."""

    source = SOURCE_DIR / spec.filename
    preview = PREVIEW_DIR / spec.filename
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
    """Pack black pixels as MSB-first bits, one 80-pixel row at a time."""

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
    """Write the generated arrays and the public status-to-asset lookup."""

    sections = [
        "#include \"status_bunny_assets.h\"\n\n",
        "namespace {\n\n",
    ]
    for spec in ALL_ASSETS:
        sections.append(format_array(spec.symbol, packed_assets[spec.symbol]))
        sections.append("\n")
    sections.extend(
        [
            "}  // namespace\n\n",
            "const PixelAsset &status_bunny_asset(StatusBunnyAssetId id)\n",
            "{\n",
            "    switch (id) {\n",
        ]
    )
    for spec in ASSETS:
        sections.extend(
            [
                f"    case StatusBunnyAssetId::{spec.enum_name}: {{\n",
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
            f"{{{ASSET_SIZE}, {ASSET_SIZE}, kCustomData}};\n",
            "    return fallback;\n",
            "}\n\n",
            "const PixelAsset &status_bunny_animation_asset(\n",
            "    StatusBunnyAssetId id, StatusBunnyAnimationFrame frame)\n",
            "{\n",
            "    if (frame == StatusBunnyAnimationFrame::Primary) {\n",
            "        return status_bunny_asset(id);\n",
            "    }\n",
            "    switch (id) {\n",
        ]
    )
    for spec in ANIMATION_ASSETS:
        sections.extend(
            [
                f"    case StatusBunnyAssetId::{spec.enum_name}: {{\n",
                f"        static constexpr PixelAsset asset = "
                f"{{{ASSET_SIZE}, {ASSET_SIZE}, {spec.symbol}}};\n",
                "        return asset;\n",
                "    }\n",
            ]
        )
    sections.extend(
        [
            "    }\n",
            "    return status_bunny_asset(id);\n",
            "}\n",
        ]
    )
    OUTPUT_CPP.write_text("".join(sections), encoding="utf-8")


def main() -> None:
    packed_assets = {
        spec.symbol: pack_pixels(render_asset(spec)) for spec in ALL_ASSETS
    }
    write_cpp(packed_assets)
    print(f"Generated {len(ALL_ASSETS)} assets in {OUTPUT_CPP}")


if __name__ == "__main__":
    main()
