#!/usr/bin/env python3
"""Generate 1-bit firmware assets for the desktop-pet app."""

from __future__ import annotations

import argparse
from collections import deque
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class AssetSpec:
    name: str
    source: Path
    width: int
    height: int
    filter_graph: str
    subject_mask: bool = False


def read_pgm(path: Path) -> tuple[int, int, bytes]:
    """Read the binary PGM file emitted by FFmpeg.

    读取 FFmpeg 输出的二进制 PGM 文件。
    """
    data = path.read_bytes()
    if not data.startswith(b"P5\n"):
        raise ValueError(f"Unsupported PGM file: {path}")

    offset = 3
    tokens: list[bytes] = []
    while len(tokens) < 3:
        while data[offset:offset + 1].isspace():
            offset += 1
        if data[offset:offset + 1] == b"#":
            offset = data.index(b"\n", offset) + 1
            continue
        end = offset
        while not data[end:end + 1].isspace():
            end += 1
        tokens.append(data[offset:end])
        offset = end

    while data[offset:offset + 1].isspace():
        offset += 1
    width, height, maximum = map(int, tokens)
    if maximum != 255:
        raise ValueError(f"Unsupported PGM depth: {maximum}")
    pixels = data[offset:offset + width * height]
    if len(pixels) != width * height:
        raise ValueError(f"Incomplete PGM data: {path}")
    return width, height, pixels


def pack_msb(width: int, height: int, pixels: bytes) -> bytes:
    """Pack black pixels into row-padded, MSB-first bytes.

    将黑色像素按每行补齐、高位优先的格式打包。
    """
    stride = (width + 7) // 8
    output = bytearray(stride * height)
    for y in range(height):
        for x in range(width):
            if pixels[y * width + x] >= 128:
                continue
            output[y * stride + x // 8] |= 1 << (7 - x % 8)
    return bytes(output)


def pack_subject_mask(width: int, height: int, pixels: bytes) -> bytes:
    """Fill closed white regions inside line art to form an opaque mask.

    填充线稿内部封闭的白色区域，生成不透明主体遮罩。
    """
    background = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def enqueue(x: int, y: int) -> None:
        index = y * width + x
        if background[index] or pixels[index] < 128:
            return
        background[index] = 1
        queue.append((x, y))

    for x in range(width):
        enqueue(x, 0)
        enqueue(x, height - 1)
    for y in range(height):
        enqueue(0, y)
        enqueue(width - 1, y)

    while queue:
        x, y = queue.popleft()
        if x > 0:
            enqueue(x - 1, y)
        if x + 1 < width:
            enqueue(x + 1, y)
        if y > 0:
            enqueue(x, y - 1)
        if y + 1 < height:
            enqueue(x, y + 1)

    stride = (width + 7) // 8
    output = bytearray(stride * height)
    for y in range(height):
        for x in range(width):
            if background[y * width + x]:
                continue
            output[y * stride + x // 8] |= 1 << (7 - x % 8)
    return bytes(output)


def render_asset(ffmpeg: str,
                 spec: AssetSpec,
                 output: Path) -> tuple[bytes, bytes | None]:
    """Resize and threshold one source image with FFmpeg.

    使用 FFmpeg 对单张源图进行裁切、缩放和黑白化。
    """
    with tempfile.TemporaryDirectory(prefix="sticky-pet-") as temp_dir:
        pgm_path = Path(temp_dir) / f"{spec.name}.pgm"
        filter_graph = (
            f"{spec.filter_graph},scale={spec.width}:{spec.height}:flags=lanczos,"
            "format=gray,lut=y='if(lt(val,150),0,255)'"
        )
        subprocess.run(
            [
                ffmpeg,
                "-hide_banner",
                "-loglevel",
                "error",
                "-y",
                "-i",
                str(spec.source),
                "-vf",
                filter_graph,
                "-frames:v",
                "1",
                str(pgm_path),
            ],
            check=True,
        )
        width, height, pixels = read_pgm(pgm_path)
        packed = pack_msb(width, height, pixels)
        mask = pack_subject_mask(width, height, pixels) if spec.subject_mask else None

        subprocess.run(
            [
                ffmpeg,
                "-hide_banner",
                "-loglevel",
                "error",
                "-y",
                "-i",
                str(pgm_path),
                str(output),
            ],
            check=True,
        )
        return packed, mask


def format_bytes(data: bytes) -> str:
    rows = []
    for index in range(0, len(data), 12):
        chunk = data[index:index + 12]
        rows.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return "\n".join(rows)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()

    source_dir = args.root / "assets" / "desktop_pet" / "source"
    concept_dir = args.root / "assets" / "desktop_pet" / "concepts"
    preview_dir = args.root / "assets" / "desktop_pet" / "firmware"
    preview_dir.mkdir(parents=True, exist_ok=True)

    specs = [
        AssetSpec("room", source_dir / "room_background.png", 440, 340, "crop=1350:1040:0:62"),
        AssetSpec("sleep_scene_a", source_dir / "sleep_scene_sheet_v1.png", 440, 340, "crop=768:650:0:185"),
        AssetSpec("sleep_scene_b", source_dir / "sleep_scene_sheet_v1.png", 440, 340, "crop=768:650:768:185"),
        AssetSpec("egg_intact", source_dir / "egg_hatching_sheet.png", 280, 280, "crop=512:512:13:0", True),
        AssetSpec("egg_wobble_left", source_dir / "egg_hatching_sheet.png", 280, 280, "crop=512:512:459:0", True),
        AssetSpec("egg_wobble_right", source_dir / "egg_hatching_sheet.png", 280, 280, "crop=512:512:16:512", True),
        AssetSpec("egg_crack_one", source_dir / "egg_hatching_sheet.png", 280, 280, "crop=512:512:466:512", True),
        AssetSpec("egg_crack_two", source_dir / "egg_hatching_sheet.png", 280, 280, "crop=512:512:16:1024", True),
        AssetSpec("egg_open", source_dir / "egg_hatching_sheet.png", 280, 280, "crop=576:576:437:960", True),
        AssetSpec("egg_nest_intact", source_dir / "egg_nest_sheet_v1.png", 380, 220, "crop=550:320:60:200"),
        AssetSpec("egg_nest_chip", source_dir / "egg_nest_sheet_v1.png", 380, 220, "crop=550:320:647:200"),
        AssetSpec("egg_nest_cracked", source_dir / "egg_nest_sheet_v1.png", 380, 220, "crop=550:320:60:700"),
        AssetSpec("egg_nest_hatched", source_dir / "egg_nest_sheet_v1.png", 380, 220, "crop=550:320:647:700"),
        AssetSpec("idle", source_dir / "hatchling_idle.png", 220, 220, "crop=920:920:167:167", True),
        AssetSpec("idle_blink", source_dir / "hatchling_blink.png", 220, 220, "crop=920:920:167:167", True),
        AssetSpec("idle_ear_twitch", source_dir / "hatchling_ear_twitch.png", 220, 220, "crop=1000:1000:127:127", True),
        AssetSpec("idle_look_around", source_dir / "hatchling_look.png", 220, 220, "crop=1000:1000:127:127", True),
        AssetSpec("idle_stretch", source_dir / "hatchling_stretch.png", 220, 220, "crop=1150:1150:52:52", True),
        AssetSpec("idle_hungry", source_dir / "hatchling_hungry.png", 220, 220, "crop=1000:1000:127:127", True),
        AssetSpec("idle_tired", source_dir / "hatchling_tired.png", 220, 220, "crop=1000:1000:127:127", True),
        AssetSpec("feed", source_dir / "hatchling_feed.png", 220, 220, "crop=920:920:167:167", True),
        AssetSpec("pet", source_dir / "hatchling_pet.png", 220, 220, "crop=920:920:167:167", True),
        AssetSpec("play", source_dir / "hatchling_play.png", 220, 220, "crop=1000:1000:127:127", True),
        AssetSpec("child_idle", source_dir / "child_idle.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("child_idle_blink", source_dir / "child_blink.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("child_idle_ear_twitch", source_dir / "child_ear_twitch.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("child_idle_look_around", source_dir / "child_look.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("child_idle_stretch", source_dir / "child_stretch.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("child_idle_hungry", source_dir / "child_hungry.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("child_idle_tired", source_dir / "child_tired.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("child_feed", source_dir / "child_feed.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("child_pet", source_dir / "child_pet_v2.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("child_play", source_dir / "child_play.png", 240, 240, "crop=1150:1150:52:52", True),
        AssetSpec("youth_foodie_idle", source_dir / "youth_foodie_idle.png", 250, 250, "crop=1150:1150:52:52", True),
        AssetSpec("youth_foodie_signature", source_dir / "youth_foodie_actions.png", 250, 250, "crop=627:627:0:0", True),
        AssetSpec("youth_foodie_feed", source_dir / "youth_foodie_actions.png", 250, 250, "crop=627:627:627:0", True),
        AssetSpec("youth_foodie_pet", source_dir / "youth_foodie_actions.png", 250, 250, "crop=627:627:0:627", True),
        AssetSpec("youth_foodie_play", source_dir / "youth_foodie_actions.png", 250, 250, "crop=627:627:627:627", True),
        AssetSpec("youth_affectionate_idle", source_dir / "youth_affectionate_idle.png", 250, 250, "crop=1150:1150:52:52", True),
        AssetSpec("youth_affectionate_signature", source_dir / "youth_affectionate_actions.png", 250, 250, "crop=627:627:0:0", True),
        AssetSpec("youth_affectionate_feed", source_dir / "youth_affectionate_actions.png", 250, 250, "crop=627:627:627:0", True),
        AssetSpec("youth_affectionate_pet", source_dir / "youth_affectionate_actions.png", 250, 250, "crop=627:627:0:627", True),
        AssetSpec("youth_affectionate_play", source_dir / "youth_affectionate_actions.png", 250, 250, "crop=627:627:627:627", True),
        AssetSpec("youth_active_idle", source_dir / "youth_active_idle.png", 250, 250, "crop=1150:1150:52:52", True),
        AssetSpec("youth_active_signature", source_dir / "youth_active_actions.png", 250, 250, "crop=627:627:0:0", True),
        AssetSpec("youth_active_feed", source_dir / "youth_active_actions.png", 250, 250, "crop=627:627:627:0", True),
        AssetSpec("youth_active_pet", source_dir / "youth_active_actions.png", 250, 250, "crop=627:627:0:627", True),
        AssetSpec("youth_active_play", source_dir / "youth_active_actions.png", 250, 250, "crop=627:627:627:627", True),
        AssetSpec("adult_foodie_idle", source_dir / "adult_foodie_idle.png", 270, 270, "crop=1150:1150:52:52", True),
        AssetSpec("adult_foodie_signature", source_dir / "adult_foodie_actions.png", 270, 270, "crop=627:600:0:0,pad=627:627:0:0:color=white", True),
        AssetSpec("adult_foodie_feed", source_dir / "adult_foodie_actions.png", 270, 270, "crop=627:627:627:0", True),
        AssetSpec("adult_foodie_pet", source_dir / "adult_foodie_actions.png", 270, 270, "crop=627:627:0:627", True),
        AssetSpec("adult_foodie_play", source_dir / "adult_foodie_actions.png", 270, 270, "crop=627:627:627:627", True),
        AssetSpec("adult_affectionate_idle", source_dir / "adult_affectionate_idle.png", 270, 270, "crop=1150:1150:52:52", True),
        AssetSpec("adult_affectionate_signature", source_dir / "adult_affectionate_actions.png", 270, 270, "crop=627:600:0:0,pad=627:627:0:0:color=white", True),
        AssetSpec("adult_affectionate_feed", source_dir / "adult_affectionate_actions.png", 270, 270, "crop=627:627:627:0", True),
        AssetSpec("adult_affectionate_pet", source_dir / "adult_affectionate_actions.png", 270, 270, "crop=627:627:0:627", True),
        AssetSpec("adult_affectionate_play", source_dir / "adult_affectionate_actions.png", 270, 270, "crop=627:627:627:627", True),
        AssetSpec("adult_active_idle", source_dir / "adult_active_idle.png", 270, 270, "crop=1150:1150:52:52", True),
        AssetSpec("adult_active_signature", source_dir / "adult_active_actions.png", 270, 270, "crop=627:600:0:0,pad=627:627:0:0:color=white", True),
        AssetSpec("adult_active_feed", source_dir / "adult_active_actions.png", 270, 270, "crop=627:627:627:0", True),
        AssetSpec("adult_active_pet", source_dir / "adult_active_actions.png", 270, 270, "crop=627:627:0:627", True),
        AssetSpec("adult_active_play", source_dir / "adult_active_actions.png", 270, 270, "crop=627:627:627:627", True),
        AssetSpec("feed_icon", concept_dir / "home_selected_480x800.png", 82, 72, "crop=110:95:25:620"),
        AssetSpec("pet_icon", concept_dir / "home_selected_480x800.png", 82, 72, "crop=100:95:195:620"),
        AssetSpec("talk_icon", source_dir / "talk_icon_v1.png", 82, 72, "crop=1000:880:127:180"),
        AssetSpec("play_icon", concept_dir / "home_selected_480x800.png", 82, 72, "crop=100:95:350:620"),
        AssetSpec("love_icon", concept_dir / "home_selected_480x800.png", 64, 30, "crop=64:42:312:66"),
    ]

    rendered: list[tuple[AssetSpec, bytes, bytes | None]] = []
    for spec in specs:
        preview_path = preview_dir / f"{spec.name}.png"
        packed, mask = render_asset(args.ffmpeg, spec, preview_path)
        rendered.append((spec, packed, mask))
        print(f"Generated {spec.name}: {spec.width}x{spec.height}")

    header_path = args.root / "src" / "ui" / "assets" / "desktop_pet_assets.h"
    source_path = args.root / "src" / "ui" / "assets" / "desktop_pet_assets.cpp"
    header_path.write_text(
        """#pragma once\n\n#include \"pixel_asset.h\"\n\nenum class DesktopPetAssetId : uint8_t {\n    Room,\n    SleepSceneA,\n    SleepSceneB,\n    Idle,\n    IdleMask,\n    IdleBlink,\n    IdleBlinkMask,\n    IdleEarTwitch,\n    IdleEarTwitchMask,\n    IdleLookAround,\n    IdleLookAroundMask,\n    IdleStretch,\n    IdleStretchMask,\n    IdleHungry,\n    IdleHungryMask,\n    IdleTired,\n    IdleTiredMask,\n    Feed,\n    FeedMask,\n    Pet,\n    PetMask,\n    Play,\n    PlayMask,\n    ChildIdle,\n    ChildIdleMask,\n    ChildIdleBlink,\n    ChildIdleBlinkMask,\n    ChildIdleEarTwitch,\n    ChildIdleEarTwitchMask,\n    ChildIdleLookAround,\n    ChildIdleLookAroundMask,\n    ChildIdleStretch,\n    ChildIdleStretchMask,\n    ChildIdleHungry,\n    ChildIdleHungryMask,\n    ChildIdleTired,\n    ChildIdleTiredMask,\n    ChildFeed,\n    ChildFeedMask,\n    ChildPet,\n    ChildPetMask,\n    ChildPlay,\n    ChildPlayMask,\n    FeedIcon,\n    PetIcon,\n    TalkIcon,\n    PlayIcon,\n    LoveIcon,\n};\n\n// Returns one generated desktop-pet bitmap.\n// 返回一张已生成的桌宠位图。\nconst PixelAsset &desktop_pet_asset(DesktopPetAssetId id);\n""",
        encoding="utf-8",
    )
    egg_enum_lines: list[str] = []
    for spec in specs:
        if not spec.name.startswith("egg_"):
            continue
        enum_name = "".join(part.capitalize() for part in spec.name.split("_"))
        egg_enum_lines.append(f"    {enum_name},")
        if spec.subject_mask:
            egg_enum_lines.append(f"    {enum_name}Mask,")
    header_path.write_text(
        header_path.read_text(encoding="utf-8").replace(
            "    Idle,",
            "\n".join(egg_enum_lines) + "\n    Idle,",
        ),
        encoding="utf-8",
    )
    personality_enum_lines: list[str] = []
    for spec in specs:
        if not spec.name.startswith(("youth_", "adult_")):
            continue
        enum_name = "".join(part.capitalize() for part in spec.name.split("_"))
        personality_enum_lines.append(f"    {enum_name},")
        if spec.subject_mask:
            personality_enum_lines.append(f"    {enum_name}Mask,")
    header_path.write_text(
        header_path.read_text(encoding="utf-8").replace(
            "    FeedIcon,",
            "\n".join(personality_enum_lines) + "\n    FeedIcon,",
        ),
        encoding="utf-8",
    )

    arrays = []
    declarations = []
    for spec, packed, mask in rendered:
        symbol = "k" + "".join(part.capitalize() for part in spec.name.split("_"))
        arrays.append(f"const uint8_t {symbol}Data[] = {{\n{format_bytes(packed)}\n}};")
        declarations.append(
            f"const PixelAsset {symbol} = {{{spec.width}, {spec.height}, {symbol}Data}};"
        )
        if mask is not None:
            arrays.append(f"const uint8_t {symbol}MaskData[] = {{\n{format_bytes(mask)}\n}};")
            declarations.append(
                f"const PixelAsset {symbol}Mask = {{{spec.width}, {spec.height}, {symbol}MaskData}};"
            )

    switch_lines = []
    for spec, _, mask in rendered:
        enum_name = "".join(part.capitalize() for part in spec.name.split("_"))
        symbol = "k" + enum_name
        switch_lines.append(f"    case DesktopPetAssetId::{enum_name}:\n        return {symbol};")
        if mask is not None:
            switch_lines.append(
                f"    case DesktopPetAssetId::{enum_name}Mask:\n        return {symbol}Mask;"
            )

    source_path.write_text(
        "#include \"desktop_pet_assets.h\"\n\nnamespace {\n\n"
        + "\n\n".join(arrays)
        + "\n\n"
        + "\n".join(declarations)
        + "\n\n}  // namespace\n\n"
        + "const PixelAsset &desktop_pet_asset(DesktopPetAssetId id)\n{\n"
        + "    switch (id) {\n"
        + "\n".join(switch_lines)
        + "\n    }\n    return kIdle;\n}\n",
        encoding="utf-8",
    )
    print(f"Wrote {header_path}")
    print(f"Wrote {source_path}")


if __name__ == "__main__":
    main()
