#!/usr/bin/env python3
"""Normalize and pack the approved Pomodoro rabbit artwork."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "pomodoro" / "source" / "tomato_bunny_half.png"
PREVIEW = ROOT / "assets" / "pomodoro" / "firmware" / "tomato_bunny.png"
OUTPUT_CPP = ROOT / "src" / "ui" / "assets" / "pomodoro_assets.cpp"
WIDTH = 64
HEIGHT = 64
BLACK_MAX = 150
CONTENT_SIZE = 62


def render_grayscale() -> Image.Image:
    """Crop, scale, and threshold the approved half-body character art."""

    source = ImageOps.autocontrast(Image.open(SOURCE).convert("L"))
    foreground = source.point(lambda pixel: 255 if pixel < 230 else 0)
    bounds = foreground.getbbox()
    if bounds is None:
        raise ValueError(f"No foreground pixels found in {SOURCE}")

    cropped = source.crop(bounds)
    cropped.thumbnail(
        (CONTENT_SIZE, CONTENT_SIZE),
        Image.Resampling.LANCZOS,
    )

    # The firmware asset is pure 1-bit art with transparent white pixels.
    # 固件素材使用纯1位黑白图，白色像素在绘制时保持透明。
    normalized = Image.new("L", (WIDTH, HEIGHT), 255)
    x = (WIDTH - cropped.width) // 2
    y = (HEIGHT - cropped.height) // 2
    normalized.paste(cropped, (x, y))
    return normalized.point(lambda pixel: 0 if pixel <= BLACK_MAX else 255)


def pack_bitmap(grayscale: Image.Image) -> bytes:
    """Pack black pixels as MSB-first row bits for PixelAsset."""

    packed = bytearray()
    for y in range(HEIGHT):
        row = [grayscale.getpixel((x, y)) for x in range(WIDTH)]
        for x in range(0, WIDTH, 8):
            value = 0
            for bit, pixel in enumerate(row[x:x + 8]):
                if pixel <= BLACK_MAX:
                    value |= 1 << (7 - bit)
            packed.append(value)
    return bytes(packed)


def format_array(data: bytes) -> str:
    """Format packed bytes as a readable C++ array."""

    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset:offset + 12]
        lines.append(
            "    " + ", ".join(f"0x{value:02X}" for value in chunk) + ","
        )
    return "\n".join(lines)


def write_preview(grayscale: Image.Image) -> None:
    """Save the same normalized frame used by the firmware packer."""

    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    grayscale.save(PREVIEW)


def write_cpp(data: bytes) -> None:
    """Write the generated firmware asset implementation."""

    OUTPUT_CPP.write_text(
        '#include "pomodoro_assets.h"\n\n'
        "namespace {\n\n"
        "constexpr uint8_t kTomatoBunnyData[] = {\n"
        f"{format_array(data)}\n"
        "};\n\n"
        "constexpr PixelAsset kTomatoBunnyAsset = {\n"
        f"    {WIDTH}, {HEIGHT}, kTomatoBunnyData,\n"
        "};\n\n"
        "}  // namespace\n\n"
        "const PixelAsset &pomodoro_tomato_bunny_asset()\n"
        "{\n"
        "    return kTomatoBunnyAsset;\n"
        "}\n",
        encoding="utf-8",
    )


def main() -> None:
    """Regenerate the preview and packed firmware asset together."""

    grayscale = render_grayscale()
    write_preview(grayscale)
    write_cpp(pack_bitmap(grayscale))


if __name__ == "__main__":
    main()
