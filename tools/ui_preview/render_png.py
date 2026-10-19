#!/usr/bin/env python3
"""Turn ui_preview .mono/.meta output into PNGs.

For each page it writes:
  <name>.png          exact 1-bit panel image (what the SSD1677 receives)
  <name>.hits.png     same image with every hit rectangle outlined and named
  <name>.pressed.png  every hit target in its tap-feedback (pressed) state
and a contact sheet (index.html) listing all pages with lint warnings.
"""
import html
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw

MIN_TOUCH = 48  # ui_tokens::kMinimumTouchHeight
CAPTION_MIN = 16  # ui_tokens::kSecondaryBody: nothing smaller may be drawn
# 16-17 px text is a caption tier: allowed for subtitles/footers/chart ticks only,
# never for anything the user must read to act (review those by eye).


def parse_meta(path):
    meta = {"hits": []}
    for line in path.read_text().splitlines():
        parts = line.split()
        if parts[0] == "size":
            meta["w"], meta["h"] = int(parts[1]), int(parts[2])
        elif parts[0] in ("gray_dark", "gray_light"):
            meta[parts[0]] = int(parts[1])
        elif parts[0] == "truncated":
            meta.setdefault("truncated", []).append(line[len("truncated "):])
        elif parts[0] == "missing_glyph":
            meta.setdefault("missing", []).append(
                f"MISSING GLYPH {parts[1]}: {' '.join(parts[2:])}")
        elif parts[0] == "text":
            size, x, y, width = map(int, parts[1:5])
            meta.setdefault("texts", []).append((size, x, y, width, " ".join(parts[5:])))
        elif parts[0] == "hit":
            meta["hits"].append((parts[1], *map(int, parts[2:6])))
    return meta


def render(mono_path):
    meta = parse_meta(mono_path.with_suffix(".meta"))
    w, h = meta["w"], meta["h"]
    img = Image.frombytes("L", (w, h), mono_path.read_bytes()).convert("1")
    img.save(mono_path.with_suffix(".png"))

    over = Image.frombytes("L", (w, h), mono_path.read_bytes()).convert("RGB")
    draw = ImageDraw.Draw(over, "RGBA")
    warnings = []
    for name, x, y, rw, rh in meta["hits"]:
        small = rh < MIN_TOUCH or rw < MIN_TOUCH
        color = (220, 0, 0, 255) if small else (0, 90, 220, 255)
        draw.rectangle([x, y, x + rw - 1, y + rh - 1], outline=color, width=2,
                       fill=color[:3] + (28,))
        draw.text((x + 4, y + 3), name, fill=color)
        if small:
            warnings.append(f"hit {name} {rw}x{rh} < {MIN_TOUCH}px")
    hits = meta["hits"]
    for i, (na, ax, ay, aw, ah) in enumerate(hits):
        for nb, bx, by, bw, bh in hits[i + 1:]:
            if na != nb and ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah:
                warnings.append(f"hit regions overlap: {na} / {nb}")
    over.save(mono_path.with_suffix(".hits.png"))
    if meta.get("gray_dark") or meta.get("gray_light"):
        warnings.append(
            f"gray pixels before threshold: dark={meta.get('gray_dark', 0)} "
            f"light={meta.get('gray_light', 0)}")
    texts = meta.get("texts", [])
    fonts = [f"TRUNCATED (cut by line limit): {t}" for t in meta.get("truncated", [])]
    fonts += meta.get("missing", [])
    for size, x, y, tw, text in texts:
        if size < CAPTION_MIN:
            fonts.append(f"FORBIDDEN {size}px: {text}")
        elif size < 18:
            fonts.append(f"caption {size}px: {text}")
        if x < 0 or y < 0 or x + tw > w or y + size > h:
            fonts.append(f"outside canvas ({x},{y},{tw}x{size}): {text}")
    for i, (sa, xa, ya, wa, ta) in enumerate(texts):
        for sb, xb, yb, wb, tb in texts[i + 1:]:
            if xa < xb + wb and xb < xa + wa and ya < yb + sb and yb < ya + sa:
                fonts.append(f"text overlap: '{ta}' / '{tb}'")
    # A label whose centre lies inside a hit target must fit inside it.
    for size, x, y, tw, text in texts:
        cx, cy = x + tw // 2, y + size // 2
        for name, hx, hy, hw, hh in meta["hits"]:
            centred = abs(cx - (hx + hw // 2)) <= 6
            if hx <= cx < hx + hw and hy <= cy < hy + hh and size >= 16 and centred:
                if x < hx + 4 or x + tw > hx + hw - 4:
                    fonts.append(f"label overflows target {name} ({hw}px): {text}")
    pressed = mono_path.with_suffix(".pressed.raw")
    if pressed.exists():
        Image.frombytes("L", (w, h), pressed.read_bytes()).convert("1").save(
            mono_path.with_suffix(".pressed.png"))
        pressed.unlink()
    return warnings, fonts


def main():
    out = Path(sys.argv[1])
    rows = []
    fontlint = {}
    lint_by_stem = {}
    for mono in sorted(out.glob("*.mono")):
        warns, fonts = render(mono)
        rows.append((mono.stem, warns))
        lint_by_stem[mono.stem] = {"warnings": warns, "fonts": fonts}
        page = mono.stem.rsplit("_", 2)[0]
        for item in fonts:
            fontlint.setdefault(page, {}).setdefault(item, []).append(
                mono.stem[len(page) + 1:])
    lines = []
    for page in sorted(fontlint):
        lines.append(f"## {page}")
        for item, where in sorted(fontlint[page].items()):
            lines.append(f"  [{','.join(sorted(set(where)))}] {item}")
    (out / "fontlint.txt").write_text("\n".join(lines) + "\n")
    (out / "lint.json").write_text(json.dumps(lint_by_stem, ensure_ascii=False))
    forbidden = sum(1 for p in fontlint.values() for i in p if i.startswith("FORBIDDEN"))
    caption = sum(1 for p in fontlint.values() for i in p if i.startswith("caption"))
    other = sum(1 for p in fontlint.values() for i in p
                if not i.startswith(("FORBIDDEN", "caption")))
    print(f"font lint: {forbidden} below {CAPTION_MIN}px (must be 0), {caption} caption-tier "
          f"({CAPTION_MIN}-17px, review), {other} layout (overlap/outside) -> {out}/fontlint.txt")
    cells = []
    for stem, warns in rows:
        w = "".join(f"<li>{html.escape(x)}</li>" for x in warns)
        cells.append(
            f"<figure><figcaption>{html.escape(stem)}</figcaption>"
            f"<a href='{stem}.hits.png'><img src='{stem}.png' width='400'></a>"
            f"<ul>{w}</ul></figure>")
    (out / "index.html").write_text(
        "<meta charset='utf-8'><title>UI preview</title>"
        "<style>body{font:13px sans-serif;display:flex;flex-wrap:wrap;gap:16px;padding:16px}"
        "img{border:1px solid #888}ul{margin:2px;padding-left:16px;color:#b00}"
        "figure{margin:0}</style>" + "".join(cells))
    flagged = sum(1 for _, w in rows if w)
    print(f"{len(rows)} pages, {flagged} with warnings -> {out}/index.html")


if __name__ == "__main__":
    main()
