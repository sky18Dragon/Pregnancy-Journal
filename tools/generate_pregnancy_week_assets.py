#!/usr/bin/env python3
"""Normalize weekly ImageGen PNGs and pack them as 1-bit firmware assets."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "pregnancy" / "week-size"
NORMALIZED_DIR = SOURCE_DIR / "firmware"
OUTPUT_CPP = ROOT / "src" / "ui" / "assets" / "pregnancy_week_assets.cpp"

WIDTH = 148
HEIGHT = 148
INK_THRESHOLD = 184
CONTENT_THRESHOLD = 245
MARGIN = 8


@dataclass(frozen=True)
class AssetSpec:
    week: int
    slug: str

    @property
    def source_name(self) -> str:
        return f"week-{self.week:02d}-{self.slug}.png"

    @property
    def symbol(self) -> str:
        parts = self.slug.replace("-", " ").title().replace(" ", "")
        return f"kWeek{self.week:02d}{parts}"


ASSETS = (
    AssetSpec(6, "blueberry"), AssetSpec(7, "coffee-bean"),
    AssetSpec(8, "raspberry"), AssetSpec(9, "cherry"),
    AssetSpec(10, "strawberry"), AssetSpec(11, "apricot"),
    AssetSpec(12, "kiwi"), AssetSpec(13, "lemon"),
    AssetSpec(14, "peach"), AssetSpec(15, "apple"),
    AssetSpec(16, "avocado"), AssetSpec(17, "pomegranate"),
    AssetSpec(18, "bell-pepper"), AssetSpec(19, "mango"),
    AssetSpec(20, "banana"), AssetSpec(21, "carrot"),
    AssetSpec(22, "coconut"), AssetSpec(23, "grapefruit"),
    AssetSpec(24, "corn"), AssetSpec(25, "cauliflower"),
    AssetSpec(26, "lettuce"), AssetSpec(27, "zucchini"),
    AssetSpec(28, "eggplant"), AssetSpec(29, "acorn-squash"),
    AssetSpec(30, "cabbage"), AssetSpec(31, "pineapple"),
    AssetSpec(32, "jicama"), AssetSpec(33, "celery"),
    AssetSpec(34, "cantaloupe"), AssetSpec(35, "winter-melon"),
    AssetSpec(36, "romaine"), AssetSpec(37, "swiss-chard"),
    AssetSpec(38, "leek"), AssetSpec(39, "watermelon"),
    AssetSpec(40, "pumpkin"),
)


def normalize(path: Path) -> Image.Image:
    """Flatten, crop, resize with nearest-neighbour, and threshold one source."""

    with Image.open(path) as source:
        rgba = source.convert("RGBA")
        flattened = Image.new("RGBA", rgba.size, "white")
        flattened.alpha_composite(rgba)
        grayscale = flattened.convert("L")

    content = grayscale.point(lambda value: 0 if value < CONTENT_THRESHOLD else 255)
    bbox = ImageOps.invert(content).getbbox()
    if bbox is None:
        raise ValueError(f"no visible content in {path}")
    cropped = grayscale.crop(bbox)
    contained = ImageOps.contain(
        cropped,
        (WIDTH - MARGIN * 2, HEIGHT - MARGIN * 2),
        Image.Resampling.NEAREST,
    )
    normalized = Image.new("L", (WIDTH, HEIGHT), "white")
    normalized.paste(
        contained,
        ((WIDTH - contained.width) // 2, (HEIGHT - contained.height) // 2),
    )
    return normalized.point(lambda value: 0 if value < INK_THRESHOLD else 255)


def pack(image: Image.Image) -> bytes:
    """Pack black pixels into MSB-first row bytes used by PixelAsset."""

    pixels = image.load()
    packed = bytearray()
    for y in range(HEIGHT):
        for x in range(0, WIDTH, 8):
            value = 0
            for bit in range(8):
                if x + bit < WIDTH and pixels[x + bit, y] == 0:
                    value |= 1 << (7 - bit)
            packed.append(value)
    return bytes(packed)


def format_array(symbol: str, data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 16):
        chunk = data[offset:offset + 16]
        lines.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return f"constexpr uint8_t {symbol}Data[] = {{\n" + "\n".join(lines) + "\n};\n"


def write_cpp(packed_assets: list[tuple[AssetSpec, bytes]]) -> None:
    parts = ['#include "pregnancy_week_assets.h"\n\n', "namespace {\n\n"]
    for spec, data in packed_assets:
        parts.append(format_array(spec.symbol, data))
        parts.append(
            f"constexpr PixelAsset {spec.symbol}Asset = "
            f"{{{WIDTH}, {HEIGHT}, {spec.symbol}Data}};\n\n"
        )
    parts.extend(["}  // namespace\n\n", "const PixelAsset &pregnancy_week_size_asset(uint8_t week)\n{\n", "    switch (week) {\n"])
    for spec, _ in packed_assets:
        parts.append(f"    case {spec.week}: return {spec.symbol}Asset;\n")
    parts.extend([
        "    default:\n",
        "        return week < 6 ? kWeek06BlueberryAsset : kWeek40PumpkinAsset;\n",
        "    }\n",
        "}\n",
    ])
    OUTPUT_CPP.write_text("".join(parts), encoding="utf-8")


def main() -> None:
    NORMALIZED_DIR.mkdir(parents=True, exist_ok=True)
    packed_assets: list[tuple[AssetSpec, bytes]] = []
    for spec in ASSETS:
        path = SOURCE_DIR / spec.source_name
        if not path.exists():
            raise FileNotFoundError(path)
        image = normalize(path)
        black_pixels = sum(1 for value in image.getdata() if value == 0)
        ratio = black_pixels / (WIDTH * HEIGHT)
        if not 0.02 <= ratio <= 0.72:
            raise ValueError(f"unexpected black-pixel ratio {ratio:.3f} in {path}")
        image.save(NORMALIZED_DIR / spec.source_name, optimize=True)
        packed_assets.append((spec, pack(image)))
        print(f"week {spec.week:02d}: black={ratio:.1%} source={path.name}")
    write_cpp(packed_assets)


if __name__ == "__main__":
    main()
