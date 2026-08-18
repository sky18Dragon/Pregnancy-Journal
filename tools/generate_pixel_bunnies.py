#!/usr/bin/env python3
"""Generate firmware-ready 1-bit bunny assets with FFmpeg."""

from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "pixel_bunnies" / "source"
MENU_PREVIEW_DIR = ROOT / "assets" / "pixel_bunnies" / "firmware" / "80x80"
DISPLAY_PREVIEW_DIR = (
    ROOT / "assets" / "pixel_bunnies" / "firmware" / "display_animation"
)
OUTPUT_CPP = ROOT / "src" / "ui" / "assets" / "status_bunny_assets.cpp"


@dataclass(frozen=True)
class AssetSpec:
    filename: str
    symbol: str
    display_symbol: str
    enum_name: str
    crop: tuple[int, int, int, int]


ASSETS = (
    AssetSpec("focusing.png", "kFocusingData", "kFocusingDisplayData", "Focusing", (120, 120, 1014, 1014)),
    AssetSpec("in_meeting.png", "kInMeetingData", "kInMeetingDisplayData", "InMeeting", (90, 210, 1074, 820)),
    AssetSpec("welcome.png", "kWelcomeData", "kWelcomeDisplayData", "Welcome", (230, 60, 900, 1140)),
    AssetSpec("out_for_lunch.png", "kOutForLunchData", "kOutForLunchDisplayData", "OutForLunch", (210, 160, 850, 930)),
    AssetSpec("off_duty.png", "kOffDutyData", "kOffDutyDisplayData", "OffDuty", (180, 100, 900, 1040)),
    AssetSpec("custom.png", "kCustomData", "kCustomDisplayData", "Custom", (200, 90, 870, 1080)),
)

ANIMATION_ASSETS = (
    AssetSpec("focusing_alt.png", "kFocusingAltData", "kFocusingAltDisplayData", "Focusing", (120, 120, 1014, 1014)),
    AssetSpec("in_meeting_alt.png", "kInMeetingAltData", "kInMeetingAltDisplayData", "InMeeting", (80, 220, 1100, 820)),
    AssetSpec("welcome_alt.png", "kWelcomeAltData", "kWelcomeAltDisplayData", "Welcome", (230, 60, 900, 1140)),
    AssetSpec("out_for_lunch_alt_v2.png", "kOutForLunchAltData", "kOutForLunchAltDisplayData", "OutForLunch", (210, 160, 850, 930)),
    AssetSpec("off_duty_alt_v2.png", "kOffDutyAltData", "kOffDutyAltDisplayData", "OffDuty", (180, 100, 900, 1040)),
    AssetSpec("custom_alt.png", "kCustomAltData", "kCustomAltDisplayData", "Custom", (200, 90, 870, 1080)),
)

ALL_ASSETS = ASSETS + ANIMATION_ASSETS

MENU_ASSET_SIZE = 80
MENU_CONTENT_SIZE = 72
DISPLAY_ASSET_SIZE = 160
DISPLAY_CONTENT_SIZE = 144
BLACK_THRESHOLD = 220
LOCKED_GEOMETRY_STATUSES = frozenset(("OutForLunch", "OffDuty"))
MAX_BOUND_SIZE_DELTA = 10
MAX_BOUND_CENTER_DELTA = 6


def filter_graph(spec: AssetSpec, asset_size: int, content_size: int) -> str:
    """Return the shared crop, scale, pad, and monochrome conversion graph."""

    x, y, width, height = spec.crop
    return (
        f"crop={width}:{height}:{x}:{y},"
        f"scale={content_size}:{content_size}:"
        "force_original_aspect_ratio=decrease:flags=area,"
        f"pad={asset_size}:{asset_size}:(ow-iw)/2:(oh-ih)/2:color=white,"
        f"format=gray,lut=y='if(lt(val,{BLACK_THRESHOLD}),0,255)'"
    )


def render_asset(
    spec: AssetSpec,
    asset_size: int,
    content_size: int,
    preview_dir: Path,
) -> bytes:
    """Render one preview PNG and return its grayscale pixel bytes."""

    source = SOURCE_DIR / spec.filename
    preview = preview_dir / spec.filename
    graph = filter_graph(spec, asset_size, content_size)
    preview_dir.mkdir(parents=True, exist_ok=True)

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
    expected_size = asset_size * asset_size
    if len(result.stdout) != expected_size:
        raise RuntimeError(
            f"{spec.filename}: expected {expected_size} bytes, got {len(result.stdout)}"
        )
    return result.stdout


def pack_pixels(grayscale: bytes, asset_size: int) -> bytes:
    """Pack black pixels as MSB-first bits, one row at a time."""

    packed = bytearray()
    for y in range(asset_size):
        row = grayscale[y * asset_size:(y + 1) * asset_size]
        for x in range(0, asset_size, 8):
            value = 0
            for bit, pixel in enumerate(row[x:x + 8]):
                if pixel < 128:
                    value |= 1 << (7 - bit)
            packed.append(value)
    return bytes(packed)


def pixel_bounds(grayscale: bytes, asset_size: int) -> tuple[int, int, int, int]:
    """Return the inclusive bounds of all black pixels in one rendered asset."""

    black_pixels = [
        (index % asset_size, index // asset_size)
        for index, pixel in enumerate(grayscale)
        if pixel < 128
    ]
    if not black_pixels:
        raise RuntimeError("Rendered asset contains no black pixels")
    xs, ys = zip(*black_pixels)
    return min(xs), min(ys), max(xs), max(ys)


def validate_locked_animation_geometry(
    display_pixels: dict[str, bytes],
) -> None:
    """Keep fixed-scene animation frames aligned in size and center."""

    animation_specs = {spec.enum_name: spec for spec in ANIMATION_ASSETS}
    for primary in ASSETS:
        if primary.enum_name not in LOCKED_GEOMETRY_STATUSES:
            continue
        alternate = animation_specs[primary.enum_name]
        primary_bounds = pixel_bounds(
            display_pixels[primary.display_symbol], DISPLAY_ASSET_SIZE
        )
        alternate_bounds = pixel_bounds(
            display_pixels[alternate.display_symbol], DISPLAY_ASSET_SIZE
        )
        primary_width = primary_bounds[2] - primary_bounds[0] + 1
        primary_height = primary_bounds[3] - primary_bounds[1] + 1
        alternate_width = alternate_bounds[2] - alternate_bounds[0] + 1
        alternate_height = alternate_bounds[3] - alternate_bounds[1] + 1
        primary_center = (
            primary_bounds[0] + primary_bounds[2],
            primary_bounds[1] + primary_bounds[3],
        )
        alternate_center = (
            alternate_bounds[0] + alternate_bounds[2],
            alternate_bounds[1] + alternate_bounds[3],
        )
        size_delta = (
            abs(primary_width - alternate_width),
            abs(primary_height - alternate_height),
        )
        center_delta = (
            abs(primary_center[0] - alternate_center[0]) / 2,
            abs(primary_center[1] - alternate_center[1]) / 2,
        )
        if (
            max(size_delta) > MAX_BOUND_SIZE_DELTA
            or max(center_delta) > MAX_BOUND_CENTER_DELTA
        ):
            raise RuntimeError(
                f"{primary.enum_name}: animation geometry shifted; "
                f"primary={primary_bounds}, alternate={alternate_bounds}, "
                f"size_delta={size_delta}, center_delta={center_delta}"
            )


def format_array(symbol: str, data: bytes) -> str:
    """Format packed bytes as a readable C++ constant array."""

    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset:offset + 12]
        lines.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return f"constexpr uint8_t {symbol}[] = {{\n" + "\n".join(lines) + "\n};\n"


def write_cpp(
    menu_assets: dict[str, bytes],
    display_assets: dict[str, bytes],
) -> None:
    """Write menu and high-resolution display assets plus their lookups."""

    sections = [
        "#include \"status_bunny_assets.h\"\n\n",
        "namespace {\n\n",
    ]
    for spec in ASSETS:
        sections.append(format_array(spec.symbol, menu_assets[spec.symbol]))
        sections.append("\n")
    for spec in ALL_ASSETS:
        sections.append(
            format_array(
                spec.display_symbol,
                display_assets[spec.display_symbol],
            )
        )
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
                f"{{{MENU_ASSET_SIZE}, {MENU_ASSET_SIZE}, {spec.symbol}}};\n",
                "        return asset;\n",
                "    }\n",
            ]
        )
    sections.extend(
        [
            "    }\n",
            f"    static constexpr PixelAsset fallback = "
            f"{{{MENU_ASSET_SIZE}, {MENU_ASSET_SIZE}, kCustomData}};\n",
            "    return fallback;\n",
            "}\n\n",
            "const PixelAsset &status_bunny_animation_asset(\n",
            "    StatusBunnyAssetId id, StatusBunnyAnimationFrame frame)\n",
            "{\n",
            "    if (frame == StatusBunnyAnimationFrame::Primary) {\n",
            "        switch (id) {\n",
        ]
    )
    for spec in ASSETS:
        sections.extend(
            [
                f"        case StatusBunnyAssetId::{spec.enum_name}: {{\n",
                f"            static constexpr PixelAsset asset = "
                f"{{{DISPLAY_ASSET_SIZE}, {DISPLAY_ASSET_SIZE}, "
                f"{spec.display_symbol}}};\n",
                "            return asset;\n",
                "        }\n",
            ]
        )
    sections.extend(
        [
            "        }\n",
            "    }\n",
            "    switch (id) {\n",
        ]
    )
    for spec in ANIMATION_ASSETS:
        sections.extend(
            [
                f"    case StatusBunnyAssetId::{spec.enum_name}: {{\n",
                f"        static constexpr PixelAsset asset = "
                f"{{{DISPLAY_ASSET_SIZE}, {DISPLAY_ASSET_SIZE}, "
                f"{spec.display_symbol}}};\n",
                "        return asset;\n",
                "    }\n",
            ]
        )
    sections.extend(
        [
            "    }\n",
            f"    static constexpr PixelAsset fallback = "
            f"{{{DISPLAY_ASSET_SIZE}, {DISPLAY_ASSET_SIZE}, "
            "kCustomAltDisplayData};\n",
            "    return fallback;\n",
            "}\n",
        ]
    )
    OUTPUT_CPP.write_text("".join(sections), encoding="utf-8")


def main() -> None:
    menu_assets = {
        spec.symbol: pack_pixels(
            render_asset(
                spec,
                MENU_ASSET_SIZE,
                MENU_CONTENT_SIZE,
                MENU_PREVIEW_DIR,
            ),
            MENU_ASSET_SIZE,
        )
        for spec in ASSETS
    }
    for spec in ANIMATION_ASSETS:
        render_asset(
            spec,
            MENU_ASSET_SIZE,
            MENU_CONTENT_SIZE,
            MENU_PREVIEW_DIR,
        )
    display_pixels = {
        spec.display_symbol: render_asset(
            spec,
            DISPLAY_ASSET_SIZE,
            DISPLAY_CONTENT_SIZE,
            DISPLAY_PREVIEW_DIR,
        )
        for spec in ALL_ASSETS
    }
    validate_locked_animation_geometry(display_pixels)
    display_assets = {
        symbol: pack_pixels(pixels, DISPLAY_ASSET_SIZE)
        for symbol, pixels in display_pixels.items()
    }
    write_cpp(menu_assets, display_assets)
    print(
        f"Generated {len(menu_assets)} menu assets and "
        f"{len(display_assets)} display assets in {OUTPUT_CPP}"
    )


if __name__ == "__main__":
    main()
