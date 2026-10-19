#!/usr/bin/env python3
"""Turn engraving-style fruit/veg illustrations into 1-bit stipple assets.

Two ways in:
  --sheet  FILE   a contact sheet laid out like the owner's reference (8 columns x
                  5 rows, weeks 1-40 left-to-right, top-to-bottom); each cell's
                  object is cut out (week number and caption blanked) and converted.
  --dir    DIR    a folder of week-NN-name.png illustrations (e.g. from ImageGen);
                  each is converted the same way.

Output: <out_dir>/week-NN-slug.png (280x280, black = ink) + contact_sheet.png +
labels.tsv (week, english name, chinese name) for the lab layout.

Render modes
  bold      (default) Gaussian denoise, sharpen, Atkinson stipple, plus a solid 3 px outline.
            Built for e-paper: the outline and heavy tone give the picture the same
            weight as the text beside it, where a pure stipple looks hazy.
  atkinson  crisp stipple only, no denoise/outline
  fs        Floyd-Steinberg (finer but mushy on e-paper)
  bayer, noise  ordered dithers

LICENSING / RULES: sheet-derived art is the owner's own reference, used for
host-only layout prototyping and written under build/ (never committed). Dithered
fills conflict with the CLAUDE.md "no dither / no hatch shading" rule; they need
an explicit owner decision (or a reviewed 4-level-gray page).
"""
import argparse
import math
import re
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont, ImageOps

SIZE = 280

# (english, chinese) for the reference sheet, weeks 1..40.
REF_NAMES = [
    ("POPPY SEED", "罂粟籽"), ("SESAME SEED", "芝麻"), ("BLUEBERRY", "蓝莓"), ("RASPBERRY", "覆盆子"),
    ("PEA", "豌豆"), ("LENTIL", "扁豆"), ("GRAPE", "葡萄"), ("KIDNEY BEAN", "芸豆"),
    ("CHERRY", "樱桃"), ("STRAWBERRY", "草莓"), ("LIME", "青柠"), ("PLUM", "梅子"),
    ("LEMON", "柠檬"), ("AVOCADO", "牛油果"), ("APPLE", "苹果"), ("ORANGE", "橙子"),
    ("POMEGRANATE", "石榴"), ("MANGO", "芒果"), ("GRAPEFRUIT", "西柚"), ("BANANA", "香蕉"),
    ("CARROT", "胡萝卜"), ("CORN", "玉米"), ("BELL PEPPER", "甜椒"), ("EGGPLANT", "茄子"),
    ("RUTABAGA", "芜菁"), ("CAULIFLOWER", "花菜"), ("CABBAGE", "卷心菜"), ("BUTTERNUT SQUASH", "南瓜"),
    ("COCONUT", "椰子"), ("HONEYDEW", "甜瓜"), ("PINEAPPLE", "菠萝"), ("JICAMA", "凉薯"),
    ("ROMAINE LETTUCE", "生菜"), ("CANTALOUPE", "哈密瓜"), ("WINTER MELON", "冬瓜"), ("NAPA CABBAGE", "大白菜"),
    ("WATERMELON", "西瓜"), ("PUMPKIN", "南瓜"), ("SPAGHETTI SQUASH", "金丝瓜"), ("JACKFRUIT", "木菠萝"),
]

# The firmware CJK font does not contain every character of the true names above.
# Until the font generator is run for them, previews use stand-ins built only from
# glyphs the font already has (week -> stand-in). Real names go in REF_NAMES.
PREVIEW_ZH = {6: "豆子", 7: "提子", 8: "大豆", 11: "小柠檬", 12: "小桃子", 16: "圆果", 25: "大头菜",
              32: "圆萝卜", 36: "菜心", 39: "菜瓜", 40: "大菠萝"}

COL_X = [9, 200, 391, 583, 774, 966, 1157, 1349]
ROW_Y = [10, 206, 410, 616, 828]       # measured: the sheet's rows are not equally tall
ROW_H = [187, 196, 199, 207, 192]
CELL_W = 179


def slug(name):
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def erase_text_components(g, paper, zone):
    """Paint out dark components that lie wholly inside `zone` (label glyphs).

    A component touching any edge of the zone continues into the object, so it is
    kept: that is what stops tips, stems and bases from being sliced off.
    """
    x0, y0, x1, y1 = zone
    px = g.load()
    dark = lambda x, y: px[x, y] < 205
    seen = set()
    d = ImageDraw.Draw(g)
    for sy in range(y0, y1):
        for sx in range(x0, x1):
            if (sx, sy) in seen or not dark(sx, sy):
                continue
            stack, comp, touches = [(sx, sy)], [], False
            seen.add((sx, sy))
            while stack:
                x, y = stack.pop()
                comp.append((x, y))
                if x <= x0 or y <= y0 or x >= x1 - 1 or y >= y1 - 1:
                    touches = True
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1), (x + 1, y + 1), (x - 1, y - 1), (x + 1, y - 1), (x - 1, y + 1)):
                    if x0 <= nx < x1 and y0 <= ny < y1 and (nx, ny) not in seen and dark(nx, ny):
                        seen.add((nx, ny))
                        stack.append((nx, ny))
            if not touches and len(comp) < 900:
                for x, y in comp:
                    d.rectangle([x - 3, y - 3, x + 3, y + 3], fill=paper)


# Pale, rounded objects whose edge fades into the paper: close their silhouette with
# a convex hull so a thin engraved contour still describes them.
HULL_WEEKS = {13, 39}   # pale highlight reaches the edge: keep a light stipple inside the hull
# Pale objects melt into the paper; a gamma > 1 darkens their tones so the stipple
# still describes the whole silhouette.
GAMMA_OVERRIDE = {13: 1.5, 28: 1.25, 30: 1.25, 34: 1.2, 39: 1.6}
PAPER = 228


def fill_holes(mask):
    inv = mask.copy()
    ImageDraw.floodfill(inv, (0, 0), 128)
    return inv.point(lambda v: 0 if v == 128 else 255)


def convex_hull(points):
    pts = sorted(set(points))
    if len(pts) < 3:
        return pts
    cross = lambda o, a, b: (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def cut_object(cell, week):
    """(grayscale object on paper-white, outline mask or None), cropped to the object."""
    g = cell.convert("L")
    erase_text_components(g, PAPER, (1, 1, 84, 92))                                  # "N WEEKS"
    erase_text_components(g, PAPER, (1, g.height - 48, g.width - 1, g.height - 1))   # caption
    mask = g.point(lambda v: 255 if v < 200 else 0).filter(ImageFilter.MedianFilter(3))
    box = mask.getbbox()
    if box is None:
        return None
    outline = None
    if week in HULL_WEEKS:   # pale object: a convex hull gives it a faint engraved contour
        m2 = fill_holes(mask.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.MinFilter(3)))
        px = m2.load()
        pts = [(x, y) for y in range(0, m2.height, 2) for x in range(0, m2.width, 2) if px[x, y]]
        hull = convex_hull(pts)
        if len(hull) >= 3:
            outline = Image.new("L", mask.size, 0)
            ImageDraw.Draw(outline).polygon(hull, fill=255)
    pad = 3
    box = (max(0, box[0] - pad), max(0, box[1] - pad), min(g.width, box[2] + pad), min(g.height, box[3] + pad))
    obj = g.crop(box).point(lambda v: max(0, min(255, int((v - 18) * 255 / (PAPER - 6 - 18)))))
    return obj, (outline.crop(box) if outline is not None else None)


def crop_to_object(img, pad=3):
    """Crop a white-background illustration to the ink's bounding box (full-size sources only)."""
    box = img.point(lambda v: 255 if v < 200 else 0).getbbox()
    if box is None:
        raise ValueError("illustration is blank")
    return img.crop((max(0, box[0] - pad), max(0, box[1] - pad), min(img.width, box[2] + pad), min(img.height, box[3] + pad)))


def fit_square(obj, outline, size=SIZE, margin=14):
    scale = min((size - 2 * margin) / obj.width, (size - 2 * margin) / obj.height)
    dims = (max(1, round(obj.width * scale)), max(1, round(obj.height * scale)))
    big = obj.resize(dims, Image.LANCZOS).filter(ImageFilter.UnsharpMask(radius=1.6, percent=90, threshold=2))
    tile = Image.new("L", (size, size), 255)
    pos = ((size - dims[0]) // 2, (size - dims[1]) // 2)
    tile.paste(big, pos)
    mtile = None
    if outline is not None:
        mtile = Image.new("L", (size, size), 0)
        mtile.paste(outline.resize(dims, Image.LANCZOS).point(lambda v: 255 if v > 100 else 0), pos)
    return tile, mtile


BAYER8 = [
    [0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26],
    [12, 44, 4, 36, 14, 46, 6, 38], [60, 28, 52, 20, 62, 30, 54, 22],
    [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
    [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21],
]


def atkinson(tile):
    """Atkinson error diffusion: only 3/4 of the error is spread, so highlights and
    shadows stay clean and the stipple keeps real structure instead of grey fog."""
    w, h = tile.size
    px = [[float(tile.getpixel((x, y))) for x in range(w)] for y in range(h)]
    out = Image.new("L", (w, h), 255)
    op = out.load()
    for y in range(h):
        for x in range(w):
            old = px[y][x]
            new = 255.0 if old >= 128 else 0.0
            op[x, y] = int(new)
            err = (old - new) / 8.0
            for dx, dy in ((1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h:
                    px[ny][nx] += err
    return out.convert("1")


def dither(tile, mode, gamma=1.0):
    t = tile.point(lambda v: int(255 * ((v / 255) ** gamma)))
    if mode == "atkinson":
        return atkinson(t)
    if mode == "fs":
        return t.convert("1", dither=Image.FLOYDSTEINBERG)
    out = Image.new("1", t.size, 1)
    px, op = t.load(), out.load()
    for y in range(t.height):
        for x in range(t.width):
            if mode == "bayer":
                thr = (BAYER8[y % 8][x % 8] + 0.5) / 64
            else:  # interleaved gradient noise: stipple without a visible grid
                thr = (52.9829189 * ((0.06711056 * x + 0.00583715 * y) % 1)) % 1
            op[x, y] = 255 if px[x, y] / 255 > thr else 0
    return out


def s_curve(tile, amount):
    lut = []
    for x in range(256):
        t = x / 255.0
        smooth = t * t * (3 - 2 * t)
        lut.append(int(round(255 * ((1 - amount) * t + amount * smooth))))
    return tile.point(lut)


def silhouette(tile, hull):
    """Closed object mask: the hull for pale objects, otherwise everything off-white."""
    if hull is not None:
        return hull
    m = tile.point(lambda v: 255 if v < 246 else 0)
    m = m.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5))
    return fill_holes(m)   # highlights inside the object must not get their own outline


def bold(tile, hull):
    t = ImageOps.autocontrast(tile, cutoff=1)
    # The source carries AI grain. A round Gaussian removes it without the square
    # artefacts a median filter leaves in highlights; the unsharp pass then restores edges.
    t = t.filter(ImageFilter.GaussianBlur(1.4))
    t = t.filter(ImageFilter.UnsharpMask(radius=2.2, percent=260, threshold=2))
    t = s_curve(t, 0.6)
    t = t.point(lambda v: int(255 * ((v / 255.0) ** 0.85)))
    if hull is not None:   # near-white highlight inside the silhouette: keep a light stipple
        t = Image.composite(t.point(lambda v: min(v, 224)), t, hull)
    mono = atkinson(t).convert("L")
    mask = silhouette(t, hull)
    inner = mask.filter(ImageFilter.MinFilter(2 * 3 + 1))
    outline = ImageOps.invert(ImageChops.subtract(mask, inner))       # 3 px solid outline
    return ImageChops.darker(mono, outline).point(lambda v: 0 if v < 128 else 255).convert("1")


def to_asset(tile, hull, mode, gamma):
    if mode == "bold":
        return bold(tile, hull)
    tile = tile.filter(ImageFilter.UnsharpMask(radius=1.4, percent=140, threshold=1))  # edges first: dithering softens them
    tile = ImageOps.autocontrast(tile, cutoff=1)
    if hull is not None:
        # A near-white highlight inside the silhouette would read as a bite taken out
        # of the object; keep a light stipple there so the whole shape stays visible.
        tile = Image.composite(tile.point(lambda v: min(v, 224)), tile, hull)
    mono = dither(tile, mode, gamma).convert("L")
    return mono.point(lambda v: 0 if v < 128 else 255).convert("1")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    ap.add_argument("--sheet")
    ap.add_argument("--dir")
    ap.add_argument("--mode", default="bold", choices=("bold", "atkinson", "fs", "bayer", "noise"))
    ap.add_argument("--gamma", type=float, default=1.0, help="<1 lightens (fewer dots)")
    ap.add_argument("--only", default="")
    ap.add_argument("--size", type=int, default=SIZE, help="output square size in px")
    args = ap.parse_args()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    only = {int(x) for x in args.only.split(",") if x}
    sources = {}
    if args.sheet:
        sheet = Image.open(args.sheet).convert("RGB")
        for i in range(40):
            r, c = divmod(i, 8)
            cell = sheet.crop((COL_X[c], ROW_Y[r], COL_X[c] + CELL_W, ROW_Y[r] + ROW_H[r]))
            cut = cut_object(cell, i + 1)
            if cut is not None:
                sources[i + 1] = cut
    if args.dir:
        for f in sorted(Path(args.dir).glob("week-*.png")):
            w = int(f.name.split("-")[1])
            sources[w] = (crop_to_object(Image.open(f).convert("L")), None)
    tiles = []
    labels = []
    for week in sorted(sources):
        if (only and week not in only) or week < 6:
            continue
        en, zh = REF_NAMES[week - 1]
        tile, mtile = fit_square(*sources[week], size=args.size)
        asset = to_asset(tile, mtile, args.mode, args.gamma if args.dir else GAMMA_OVERRIDE.get(week, args.gamma))   # overrides were tuned for the reference sheet
        asset.save(out / f"week-{week:02d}-{slug(en)}.png")
        labels.append(f"{week}\t{en}\t{PREVIEW_ZH.get(week, zh)}")
        tiles.append((week, en, asset))
    (out / "labels.tsv").write_text("\n".join(labels) + "\n", encoding="utf-8")
    cols = 7
    rows = (len(tiles) + cols - 1) // cols
    sz = args.size
    sheet_out = Image.new("L", (cols * (sz + 8) + 8, rows * (sz + 8) + 8), 140)
    for i, (_, _, im) in enumerate(tiles):
        sheet_out.paste(im.convert("L"), (8 + (i % cols) * (sz + 8), 8 + (i // cols) * (sz + 8)))
    sheet_out.save(out / "contact_sheet.png")
    print(f"{len(tiles)} stipple assets ({args.mode}, gamma {args.gamma}) -> {out}")


if __name__ == "__main__":
    main()
