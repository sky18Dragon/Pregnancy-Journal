#!/usr/bin/env python3
"""Turn ui_preview .mono/.meta output into PNGs.

For each page it writes:
  <name>.png          exact 1-bit panel image (what the SSD1677 receives)
  <name>.hits.png     same image with every hit rectangle outlined and named
and a contact sheet (index.html) listing all pages with lint warnings.
"""
import html
import sys
from pathlib import Path

from PIL import Image, ImageDraw

MIN_TOUCH = 48  # ui_tokens::kMinimumTouchHeight


def parse_meta(path):
    meta = {"hits": []}
    for line in path.read_text().splitlines():
        parts = line.split()
        if parts[0] == "size":
            meta["w"], meta["h"] = int(parts[1]), int(parts[2])
        elif parts[0] in ("gray_dark", "gray_light"):
            meta[parts[0]] = int(parts[1])
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
    return warnings


def main():
    out = Path(sys.argv[1])
    rows = []
    for mono in sorted(out.glob("*.mono")):
        warns = render(mono)
        rows.append((mono.stem, warns))
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
