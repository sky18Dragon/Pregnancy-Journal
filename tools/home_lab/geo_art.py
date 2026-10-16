#!/usr/bin/env python3
"""Geometric week-size illustrations (Bauhaus / modern-poster register).

Each object is described once as clean geometry (ellipses, lenses, splines,
capsules) with semantic roles, then rendered in two 1-bit styles:

  solid  black silhouettes, white gaps between overlapping parts, details cut
         out of the black as negative space (stencil / linocut feel)
  line   uniform-weight outlines with black leaves/caps as the only fills

Crisp, constant-width curves; no hand jitter, no faces. Everything is code, so
the set is consistent and regenerable. To replace one object with hand-made or
ImageGen art, drop a pure black/white PNG with the same name into the style
folder (PROMPT.md beside them has a matching prompt).

Usage: geo_art.py <out_dir> [--only 21,24]    (writes <out_dir>/<style>/ for solid line badge echo sticker shadow stripe)
"""
import argparse
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont, ImageOps

N = 280
K = 4
BLACK, WHITE = 0, 255
OUT_W = 7.0     # outline width in `line` style
CUT_W = 5.0     # cut-out line width in `solid` style
HALO = 3.5      # white gap around overlapping black shapes
GAP, RING = 7.0, 4.0   # echo / sticker border: gap to the shape, ring thickness
SHADOW = 13.0   # offset of the `shadow` style

STYLES = ("solid", "line", "badge", "echo", "sticker", "shadow", "stripe", "hand", "riso")
BASE = {"badge": "solid", "echo": "line", "sticker": "solid", "shadow": "line", "stripe": "solid", "riso": "solid"}


# ---- geometry helpers ---------------------------------------------------------

def spline(pts, closed=True, step=0.9):
    n = len(pts)
    out = []
    segs = n if closed else n - 1
    for i in range(segs):
        if closed:
            p0, p1, p2, p3 = pts[(i - 1) % n], pts[i], pts[(i + 1) % n], pts[(i + 2) % n]
        else:
            p0 = pts[max(i - 1, 0)]; p1 = pts[i]; p2 = pts[i + 1]; p3 = pts[min(i + 2, n - 1)]
        m = max(2, int(math.hypot(p2[0] - p1[0], p2[1] - p1[1]) / step))
        for j in range(m):
            t = j / m
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[a]) + (-p0[a] + p2[a]) * t +
                                    (2 * p0[a] - 5 * p1[a] + 4 * p2[a] - p3[a]) * t2 +
                                    (-p0[a] + 3 * p1[a] - 3 * p2[a] + p3[a]) * t3) for a in (0, 1)))
    if not closed:
        out.append(tuple(pts[-1]))
    return out


def rot(pts, cx, cy, deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in pts]


def ell(cx, cy, rx, ry, deg=0, n=120):
    pts = [(cx + rx * math.cos(2 * math.pi * i / n), cy + ry * math.sin(2 * math.pi * i / n)) for i in range(n)]
    return rot(pts, cx, cy, deg) if deg else pts


def arc(cx, cy, rx, ry, a0, a1, n=40):
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def bez(p0, c, p1, n=40):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * c[0] + t * t * p1[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * c[1] + t * t * p1[1]) for t in (i / n for i in range(n + 1))]


def lens(x0, y0, x1, y1, w, bend=0.0):
    dx, dy = x1 - x0, y1 - y0
    ln = math.hypot(dx, dy) or 1
    nx, ny = -dy / ln, dx / ln
    mx, my = (x0 + x1) / 2, (y0 + y1) / 2
    a = bez((x0, y0), (mx + nx * (2 * w + bend), my + ny * (2 * w + bend)), (x1, y1))
    b = bez((x0, y0), (mx - nx * (2 * w - bend), my - ny * (2 * w - bend)), (x1, y1))
    return a + b[::-1][1:-1]


def capsule(x0, y0, x1, y1, r):
    ang = math.degrees(math.atan2(y1 - y0, x1 - x0))
    return arc(x1, y1, r, r, ang - 90, ang + 90, 24) + arc(x0, y0, r, r, ang + 90, ang + 270, 24)


def star(cx, cy, ro, ri, n=5, deg=-90):
    pts = []
    for i in range(n * 2):
        a = math.radians(deg + i * 180 / n)
        r = ro if i % 2 == 0 else ri
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def sm(pts):
    return spline(pts, True)


# ---- scene --------------------------------------------------------------------

class Art:
    def __init__(self):
        self.ops = []

    def body(self, pts): self.ops.append(("body", pts))
    def part(self, pts): self.ops.append(("part", pts))
    def pline(self, pts, w=7.0): self.ops.append(("pline", spline(pts, False) if len(pts) > 2 else pts, w))
    def cut(self, pts, w=CUT_W, clip=None, closed=False): self.ops.append(("cut", pts, w, clip, closed))
    def cutfill(self, pts): self.ops.append(("cutfill", pts))
    def dot(self, x, y, r): self.ops.append(("dot", x, y, r))
    def inkdot(self, x, y, r): self.ops.append(("inkdot", x, y, r))
    def inkline(self, pts, w=5.0, clip=None): self.ops.append(("inkline", pts, w, clip))
    def rim(self, pts, w=7.0): self.ops.append(("rim", pts, w))
    def hl(self, cx, cy, rx, ry, a0=205, a1=255): self.ops.append(("hl", arc(cx, cy, rx * 0.72, ry * 0.72, a0, a1)))


class Canvas:
    def __init__(self):
        self.im = Image.new("L", (N * K, N * K), WHITE)
        self.d = ImageDraw.Draw(self.im)

    def disc(self, x, y, r, c):
        self.d.ellipse([(x - r) * K, (y - r) * K, (x + r) * K, (y + r) * K], fill=c)

    def stroke(self, pts, w, c, closed=False):
        pts = list(pts) + ([pts[0]] if closed else [])
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 0.6))
            for i in range(n + 1):
                t = i / n
                self.disc(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, w / 2, c)

    def fill(self, pts, c):
        self.d.polygon([(x * K, y * K) for x, y in pts], fill=c)

    def clipped(self, clip, fn):
        real_im, real_d = self.im, self.d
        self.im = Image.new("L", real_im.size, WHITE)
        self.d = ImageDraw.Draw(self.im)
        fn()
        layer = self.im
        self.im, self.d = real_im, real_d
        mask = Image.new("L", layer.size, 0)
        ImageDraw.Draw(mask).polygon([(x * K, y * K) for x, y in clip], fill=255)
        self.im.paste(ImageChops.darker(self.im, layer), mask=mask)
        # clipped white cuts must also lighten, so recomposite them
        return layer, mask


def render(art, style):
    final = style
    style = BASE.get(style, style)
    cv = Canvas()
    shapes = [op for op in art.ops if op[0] in ("body", "part")]
    plines = [op for op in art.ops if op[0] == "pline"]
    if final in ("echo", "sticker"):
        # one border around the whole silhouette: wide black pass, then a narrower white pass
        for op in shapes:
            cv.stroke(op[1], 2 * (GAP + RING), BLACK, closed=True)
        for op in plines:
            cv.stroke(op[1], op[2] + 2 * (GAP + RING), BLACK)
        for op in shapes:
            cv.stroke(op[1], 2 * GAP, WHITE, closed=True)
        for op in plines:
            cv.stroke(op[1], op[2] + 2 * GAP, WHITE)
    if final == "shadow":
        for op in shapes:
            cv.stroke([(x + SHADOW, y + SHADOW) for x, y in op[1]], OUT_W, BLACK, closed=True)
    deferred = []
    for op in art.ops:
        kind = op[0]
        if final == "riso" and kind in ("part", "pline"):
            deferred.append(op)
            continue
        if kind == "body":
            pts = op[1]
            if style == "solid":
                cv.stroke(pts, HALO * 2, WHITE, closed=True)
                cv.fill(pts, BLACK)
            else:
                cv.fill(pts, WHITE)
                cv.stroke(pts, OUT_W, BLACK, closed=True)
        elif kind == "part":
            cv.stroke(op[1], HALO * 2, WHITE, closed=True)
            cv.fill(op[1], BLACK)
        elif kind == "pline":
            cv.stroke(op[1], op[2] + HALO * 2, WHITE)
            cv.stroke(op[1], op[2], BLACK)
        elif kind == "cut":
            _, pts, w, clip, closed = op
            color, width = (WHITE, w) if style == "solid" else (BLACK, w * 0.8)
            _draw_clipped(cv, clip, lambda: cv.stroke(pts, width, color, closed), color == WHITE)
        elif kind == "cutfill":
            if style == "solid":
                cv.fill(op[1], WHITE)
            else:
                cv.stroke(op[1], OUT_W * 0.7, BLACK, closed=True)
        elif kind == "dot":
            cv.disc(op[1], op[2], op[3] if style == "solid" else op[3] * 0.85, WHITE if style == "solid" else BLACK)
        elif kind == "inkdot":
            cv.disc(op[1], op[2], op[3], BLACK)
        elif kind == "inkline":
            _, pts, w, clip = op
            _draw_clipped(cv, clip, lambda: cv.stroke(pts, w, BLACK), False)
        elif kind == "rim":
            cv.stroke(op[1], op[2], BLACK, closed=True)
        elif kind == "hl" and style == "solid":
            cv.stroke(op[1], CUT_W, WHITE)
    # riso overprint: leaves/stems are a second plate; where they overlap the body the ink flips to paper
    for op in deferred:
        m = Canvas()
        if op[0] == "part":
            m.fill(op[1], BLACK)
        else:
            m.stroke(op[1], op[2], BLACK)
        cv.im.paste(ImageOps.invert(cv.im), mask=ImageOps.invert(m.im))
        cv.d = ImageDraw.Draw(cv.im)
    # centre + scale to fill the tile
    box = ImageOps.invert(cv.im).point(lambda v: 255 if v > 90 else 0).getbbox()
    crop = cv.im.crop(box)
    fit = (N - 28) * K
    s = min(fit / crop.width, fit / crop.height)
    crop = crop.resize((max(1, round(crop.width * s)), max(1, round(crop.height * s))), Image.LANCZOS)
    tile = Image.new("L", (N * K, N * K), WHITE)
    tile.paste(crop, ((N * K - crop.width) // 2, (N * K - crop.height) // 2))
    if final == "badge":
        tile = _badge(tile)
    elif final == "stripe":
        tile = _stripe(tile)
    return tile.resize((N, N), Image.LANCZOS).point(lambda v: 0 if v < 150 else 255).convert("1")


def _badge(tile):
    """White object cut out of a solid black disc."""
    inv = ImageOps.invert(tile)
    size = N * K
    small = inv.resize((round(size * 0.70), round(size * 0.70)), Image.LANCZOS)
    out = Image.new("L", (size, size), BLACK)
    out.paste(small, ((size - small.width) // 2, (size - small.height) // 2))
    disc = Image.new("L", (size, size), 0)
    ImageDraw.Draw(disc).ellipse([size * 0.04, size * 0.04, size * 0.96, size * 0.96], fill=255)
    return Image.composite(out, Image.new("L", (size, size), WHITE), disc)


def _stripe(tile):
    """Solid silhouette rendered as horizontal bars with a solid rim."""
    size = N * K
    black = tile.point(lambda v: 255 if v < 128 else 0)
    inner = black.filter(ImageFilter.MinFilter(2 * int(2.4 * K) + 1))
    rim = ImageChops.subtract(black, inner)
    bars = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(bars)
    period = 7 * K
    for y in range(0, size, period):
        d.rectangle([0, y, size, y + int(period * 0.52)], fill=255)
    ink = ImageChops.lighter(rim, ImageChops.multiply(black, bars.point(lambda v: 255 if v else 0)))
    return ImageOps.invert(ink)


def _draw_clipped(cv, clip, fn, lighten):
    if clip is None:
        fn(); return
    real_im, real_d = cv.im, cv.d
    cv.im = Image.new("L", real_im.size, BLACK if lighten else WHITE)
    cv.d = ImageDraw.Draw(cv.im)
    # white cuts: draw on a black layer and lighten inside the mask; black lines: darken
    if lighten:
        cv.d.rectangle([0, 0, cv.im.width, cv.im.height], fill=BLACK)
    fn()
    layer = cv.im
    cv.im, cv.d = real_im, real_d
    mask = Image.new("L", layer.size, 0)
    ImageDraw.Draw(mask).polygon([(x * K, y * K) for x, y in clip], fill=255)
    cv.im.paste(ImageChops.lighter(cv.im, layer) if lighten else ImageChops.darker(cv.im, layer), mask=mask)


# ---- hand-drawn style ----------------------------------------------------------
# Same geometry, redrawn like a playful pen illustration: a smooth low-amplitude
# wobble field (so overlapping shapes stay registered), pressure-varying round
# line, an off-register black shadow fill behind each body, a face with one of
# six expressions, little arms and legs, and a few sparkles around.

import zlib

NO_LIMBS = {"carrot", "banana", "zucchini", "corn", "leek", "celery", "romaine",
            "swiss-chard", "cherry", "coffee-bean", "raspberry", "cauliflower"}
HAND_W = 6.5


def _poly_area(pts):
    return abs(sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]))) / 2


class Hand:
    def __init__(self, seed):
        import random
        self.r = random.Random(seed)
        self.ph = [self.r.uniform(0, 6.283) for _ in range(8)]
        self.cv = Canvas()

    def warp(self, p):
        x, y = p
        a = 1.7
        return (x + a * (0.6 * math.sin(0.045 * y + 0.02 * x + self.ph[0]) + 0.4 * math.sin(0.11 * x + self.ph[1])),
                y + a * (0.6 * math.sin(0.040 * x + 0.02 * y + self.ph[2]) + 0.4 * math.sin(0.10 * y + self.ph[3])))

    def w(self, pts):
        return [self.warp(p) for p in pts]

    def pen(self, pts, w, closed=False, taper=False):
        pts = list(pts)
        if closed:   # pen overshoot: carry on a little past the start, slightly off
            k = max(3, len(pts) // 18)
            pts = pts + [(x + 1.3, y + 0.9) for x, y in pts[:k]]
        arc, total = [0.0], 0.0
        for a, b in zip(pts, pts[1:]):
            total += math.hypot(b[0] - a[0], b[1] - a[1]); arc.append(total)
        for i in range(len(pts) - 1):
            (x0, y0), (x1, y1) = pts[i], pts[i + 1]
            n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 0.6))
            for j in range(n + 1):
                t = j / n
                l = arc[i] + (arc[i + 1] - arc[i]) * t
                f = 0.84 + 0.30 * math.sin(l * 0.05 + self.ph[4])
                if taper and total > 10:
                    f *= 0.5 + 0.5 * min(1.0, min(l, total - l) / 8.0)
                self.cv.disc(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, w * f / 2, BLACK)

    def face(self, cx, cy, sc, kind, color=BLACK):
        cv = self.cv
        paper = WHITE if color == BLACK else BLACK
        if color == BLACK:   # clear whatever texture lies under the face
            cv.fill(ell(cx, cy + 6 * sc, 30 * sc, 24 * sc), WHITE)
        for sx in (-1, 1):
            ex = cx + sx * 15 * sc
            if kind in (2, 5):
                a0, a1 = (200, 340) if kind == 2 else (20, 160)
                cv.stroke(arc(ex, cy + (3 if kind == 2 else -2) * sc, 7 * sc, 6 * sc, a0, a1, 14), 3.6 * sc, color)
            else:
                cv.disc(ex, cy, 5.0 * sc, color)
                cv.disc(ex - 1.5 * sc, cy - 1.8 * sc, 1.6 * sc, paper)
        my = cy + 12 * sc
        if kind == 1:      # open grin
            cv.fill(arc(cx, my - 2 * sc, 11 * sc, 11 * sc, 0, 180, 18), color)
            cv.disc(cx, my + 5 * sc, 3.6 * sc, paper)
        elif kind == 3:    # surprised "o"
            cv.disc(cx, my + 3 * sc, 5.6 * sc, color); cv.disc(cx, my + 3 * sc, 2.6 * sc, paper)
        else:
            cv.stroke(arc(cx, my - 4 * sc, 9 * sc, 8 * sc, 25, 155, 14), 3.6 * sc, color)
            if kind == 4:  # tongue
                cv.fill(arc(cx + 2 * sc, my + 3 * sc, 4.5 * sc, 6 * sc, 0, 180, 10), color)
        if color == BLACK:
            for sx in (-1, 1):
                for k in range(2):
                    bx = cx + sx * (28 + k * 4.5) * sc
                    cv.stroke([(bx - 2 * sc, my + 1 * sc), (bx + 1.5 * sc, my - 4 * sc)], 2.6 * sc, BLACK)

    def mark(self, kind, x, y, sc):
        cv = self.cv
        if kind == 0:      # sparkle ticks
            for a in (-60, -15, 30):
                ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
                cv.stroke([(x + ca * 6 * sc, y + sa * 6 * sc), (x + ca * 17 * sc, y + sa * 17 * sc)], 3.6, BLACK)
        elif kind == 1:    # four-point star
            cv.fill(star(x, y, 13 * sc, 3.2 * sc, 4, -90), BLACK)
        elif kind == 2:    # heart
            pts = [(x, y + 12 * sc), (x - 14 * sc, y - 2 * sc), (x - 8 * sc, y - 12 * sc), (x, y - 5 * sc),
                   (x + 8 * sc, y - 12 * sc), (x + 14 * sc, y - 2 * sc)]
            cv.fill(spline(pts, True), BLACK)
        elif kind == 3:    # three dots
            for dx, dy in ((-9, 3), (0, -4), (9, 3)):
                cv.disc(x + dx * sc, y + dy * sc, 3.4, BLACK)
        else:              # little spiral
            sp_pts = [(x + (2 + 1.6 * t) * 1.0 * math.cos(t), y + (2 + 1.6 * t) * math.sin(t)) for t in [i * 0.4 for i in range(20)]]
            cv.stroke(sp_pts, 3.2, BLACK)


def render_hand(art, slug, week):
    h = Hand(zlib.crc32(slug.encode()))
    cv = h.cv
    bodies = [op[1] for op in art.ops if op[0] == "body"]
    big = max(bodies, key=_poly_area) if bodies else None
    for op in art.ops:
        kind = op[0]
        if kind == "body":
            pts = h.w(op[1])
            cv.fill([(x + 7, y + 8) for x, y in pts], BLACK)      # off-register shadow fill
            cv.fill(pts, WHITE)
            h.pen(pts, HAND_W, closed=True)
        elif kind == "part":
            pts = h.w(op[1])
            cv.stroke(pts, 7, WHITE, closed=True)
            cv.fill(pts, BLACK)
            h.pen(pts, 3.2, closed=True)
        elif kind == "pline":
            pts = h.w(op[1])
            h.pen(pts, op[2] * 0.85)
        elif kind == "cut":
            _, pts, w, clip, closed = op
            f = lambda: h.pen(h.w(pts), max(3.6, w * 0.85), closed=closed, taper=True)
            _draw_clipped(cv, h.w(clip) if clip else None, f, False)
        elif kind == "cutfill":
            h.pen(h.w(op[1]), 4.4, closed=True)
        elif kind == "dot":
            x, y = h.warp((op[1], op[2])); cv.disc(x, y, op[3] * 0.95, BLACK)
        elif kind == "inkdot":
            x, y = h.warp((op[1], op[2])); cv.disc(x, y, op[3], BLACK)
        elif kind == "inkline":
            _, pts, w, clip = op
            _draw_clipped(cv, h.w(clip) if clip else None, lambda: h.pen(h.w(pts), w * 0.9, taper=True), False)
        # rim / hl are not used in the hand style
    if big is not None:
        wb = h.w(big)
        xs, ys = [p[0] for p in wb], [p[1] for p in wb]
        cx, cy = sum(xs) / len(xs), sum(ys) / len(ys)
        sc = max(0.6, min(1.25, math.sqrt(_poly_area(wb)) / 95))
        kind = (week * 5 + 2) % 6
        face_dot = next((op for op in art.ops if op[0] == "inkdot" and op[3] > 20), None)
        no_face = slug in ("coconut",)
        if face_dot is not None:
            fx, fy = h.warp((face_dot[1], face_dot[2]))
            h.face(fx, fy, 0.62, kind % 4, WHITE)
        elif slug == "cherry":      # a face on each cherry
            for body in sorted(bodies, key=_poly_area, reverse=True)[:2]:
                wb2 = h.w(body)
                h.face(sum(p[0] for p in wb2) / len(wb2), sum(p[1] for p in wb2) / len(wb2) + 4, 0.62, kind)
        elif len(bodies) > 3:       # clusters: one face in the middle of the whole thing
            allx = [p[0] for b in bodies for p in h.w(b)]; ally = [p[1] for b in bodies for p in h.w(b)]
            h.face((min(allx) + max(allx)) / 2, (min(ally) + max(ally)) / 2 + 6, 0.85, kind)
        elif not no_face:
            fy = cy + (0.0 if slug not in ("watermelon",) else 26)
            h.face(cx, fy, sc, kind)
        if slug not in NO_LIMBS:
            ya = cy + 10 * sc
            near = [p for p in wb if abs(p[1] - ya) < 7]
            if near:
                xl, xr = min(p[0] for p in near), max(p[0] for p in near)
                up = week % 2 == 0
                left = [(xl + 1, ya), (xl - 15, ya - (4 if up else -4)), (xl - 24, ya - (20 if up else -6))]
                right = [(xr - 1, ya), (xr + 15, ya - 4), (xr + 25, ya - 18)] if up else \
                        [(xr - 1, ya), (xr + 15, ya + 4), (xr + 24, ya + 8)]
                for arm in (left, right):
                    pts = spline(arm, False)
                    h.pen(pts, 5.6, taper=False)
                    cv.disc(arm[-1][0], arm[-1][1], 5.2, BLACK)
            for sx in (-1, 1):
                x0 = cx + sx * 16 * sc
                cand = [p for p in wb if abs(p[0] - x0) < 5]
                if cand:
                    yb = max(p[1] for p in cand)
                    h.pen([(x0, yb - 2), (x0, yb + 14)], 5.6)
                    h.pen([(x0, yb + 14), (x0 + sx * 11, yb + 15)], 5.6)
    # centre + scale the figure, leaving corners free for the sparkles
    box = ImageOps.invert(cv.im).point(lambda v: 255 if v > 90 else 0).getbbox()
    crop = cv.im.crop(box)
    fit = 206 * K
    sf = min(fit / crop.width, fit / crop.height)
    crop = crop.resize((max(1, round(crop.width * sf)), max(1, round(crop.height * sf))), Image.LANCZOS)
    tile = Image.new("L", (N * K, N * K), WHITE)
    tile.paste(crop, ((N * K - crop.width) // 2, (N * K - crop.height) // 2 + 4 * K))
    cv.im, cv.d = tile, ImageDraw.Draw(tile)
    corners = [(58, 62), (222, 60), (60, 222), (222, 224)]
    h.r.shuffle(corners)
    kinds = [0, 1, 2, 3, 4]
    h.r.shuffle(kinds)
    for (x, y), k in zip(corners[:3], kinds[:3]):
        h.mark(k, x, y, 0.9)
    return tile.resize((N, N), Image.LANCZOS).point(lambda v: 0 if v < 150 else 255).convert("1")


# ---- objects (280 space; auto-fitted afterwards) ---------------------------------

def blueberry():
    a = Art(); a.body(ell(140, 152, 68, 64)); a.part(star(140, 98, 20, 9)); a.hl(140, 152, 68, 64)
    return a

def coffee_bean():
    a = Art(); b = ell(140, 150, 84, 56, -30); a.body(b)
    a.cut(spline([(84, 196), (120, 164), (150, 140), (176, 116), (200, 90)], False), 7); return a

def raspberry():
    a = Art()
    cs = [(140, 176), (104, 170), (176, 170), (88, 140), (122, 140), (158, 140), (192, 140),
          (104, 110), (140, 108), (176, 110), (122, 82), (158, 82)]
    for x, y in sorted(cs, key=lambda c: -c[1]):
        a.body(ell(x, y, 21, 21))
        a.dot(x - 6, y - 6, 3.2)
    for x1, y1 in [(108, 52), (172, 52), (140, 42)]:
        a.part(lens(140, 80, x1, y1, 7))
    return a

def cherry():
    a = Art()
    a.pline([(92, 170), (112, 112), (150, 56)], 5); a.pline([(190, 164), (178, 104), (150, 56)], 5)
    a.part(lens(150, 56, 214, 36, 14))
    a.body(ell(92, 198, 46, 44)); a.body(ell(190, 190, 44, 42))
    a.hl(92, 198, 46, 44); a.hl(190, 190, 44, 42); return a

def strawberry():
    a = Art()
    b = sm([(140, 100), (190, 96), (214, 128), (196, 178), (140, 246), (84, 178), (66, 128), (90, 96)]); a.body(b)
    for x, y in [(106, 130), (140, 122), (174, 130), (118, 160), (162, 160), (140, 156), (126, 192), (154, 192), (140, 214), (92, 150), (188, 150)]:
        a.dot(x, y, 3.8)
    for x1, y1 in [(88, 84), (112, 70), (140, 62), (168, 70), (192, 84)]:
        a.part(lens(140, 104, x1, y1, 8))
    a.pline([(140, 100), (140, 72)], 6); return a

def apricot():
    a = Art(); a.part(lens(146, 98, 204, 70, 13)); a.body(ell(140, 160, 74, 68))
    a.cut(arc(60, 160, 82, 72, -28, 28, 24) if False else spline([(130, 96), (142, 140), (136, 190), (122, 226)], False), 5)
    a.hl(140, 160, 74, 68); return a

def kiwi():
    a = Art(); a.body(ell(140, 150, 66, 84)); a.cutfill(ell(140, 150, 53, 71))
    for i in range(14):
        t = 2 * math.pi * i / 14
        a.inkdot(140 + 30 * math.cos(t), 150 + 42 * math.sin(t), 3.6)
    a.cutfill(ell(140, 150, 14, 20)); a.inkdot(140, 150, 5); return a

def lemon():
    a = Art(); a.part(lens(150, 108, 204, 74, 13)); a.body(lens(56, 186, 226, 112, 62, bend=-6))
    a.hl(140, 152, 84, 56, 190, 240); a.dot(168, 150, 3.4); a.dot(188, 138, 3.4); a.dot(120, 168, 3.4); return a

def peach():
    a = Art(); a.part(lens(146, 98, 206, 66, 13))
    a.body(sm([(140, 98), (184, 98), (214, 140), (198, 202), (140, 240), (82, 202), (66, 140), (96, 98)]))
    a.cut(spline([(140, 100), (130, 146), (138, 196), (150, 230)], False), 5); a.hl(140, 164, 74, 70); return a

def apple():
    a = Art()
    a.body(sm([(140, 108), (170, 92), (206, 108), (218, 154), (198, 208), (168, 238), (140, 228), (112, 238), (82, 208), (62, 154), (74, 108), (110, 92)]))
    a.pline([(140, 108), (146, 72)], 7); a.part(lens(148, 82, 206, 56, 13)); a.hl(140, 166, 76, 72); return a

def avocado():
    a = Art()
    a.body(sm([(140, 48), (170, 86), (192, 140), (196, 198), (170, 238), (140, 246), (110, 238), (84, 198), (88, 140), (110, 86)]))
    a.cutfill(sm([(140, 78), (160, 108), (176, 148), (178, 196), (160, 224), (140, 230), (120, 224), (102, 196), (104, 148), (120, 108)]))
    a.inkdot(140, 184, 36); a.dot(128, 170, 5); return a

def pomegranate():
    a = Art(); a.body(ell(140, 162, 74, 70))
    a.part([(108, 100), (112, 62), (126, 82), (140, 54), (154, 82), (168, 62), (172, 100)])
    a.hl(140, 162, 74, 70); a.dot(120, 140, 3.6); a.dot(176, 190, 3.6); a.dot(108, 196, 3.6); return a

def bell_pepper():
    a = Art()
    a.body(sm([(100, 108), (140, 98), (180, 108), (210, 150), (204, 208), (176, 238), (140, 224), (104, 238), (76, 208), (70, 150)]))
    a.cut(spline([(120, 108), (110, 160), (118, 220)], False), 4.5); a.cut(spline([(160, 108), (170, 160), (162, 220)], False), 4.5)
    a.part([(124, 100), (130, 76), (152, 72), (158, 100)]); a.pline([(142, 74), (148, 56), (164, 50)], 6); return a

def mango():
    a = Art(); a.part(lens(170, 100, 218, 66, 13))
    a.body(sm([(58, 168), (84, 120), (148, 94), (206, 112), (230, 160), (200, 210), (140, 228), (88, 214)]))
    a.hl(142, 160, 84, 62, 200, 250); return a

def banana():
    a = Art()
    a.body(sm([(58, 100), (74, 170), (130, 214), (194, 210), (236, 158), (242, 108), (222, 134), (190, 174), (140, 184), (98, 152)]))
    a.part(sm([(50, 94), (68, 96), (66, 116), (56, 116)])); a.part(sm([(234, 98), (250, 104), (246, 122), (236, 120)]))
    a.cut(spline([(88, 144), (132, 190), (188, 186), (232, 126)], False), 4); return a

def carrot():
    a = Art()
    for x1, y1, w in [(98, 40, 12), (140, 24, 11), (182, 40, 12)]:
        a.part(lens(140, 96, x1, y1, w))
    a.body(sm([(98, 104), (140, 92), (182, 104), (174, 170), (148, 252), (132, 252), (106, 170)]))
    for y, x0, x1 in [(130, 112, 138), (160, 140, 168), (190, 126, 150), (218, 138, 156)]:
        a.cut([(x0, y), (x1, y + 2)], 5)
    return a

def coconut():
    a = Art(); a.body(ell(140, 154, 76, 74)); a.dot(122, 130, 8); a.dot(158, 130, 8); a.dot(140, 156, 8); a.hl(140, 154, 76, 74); return a

def grapefruit():
    a = Art(); a.body(ell(140, 150, 80, 80)); a.cutfill(ell(140, 150, 66, 66))
    for i in range(10):
        t = 2 * math.pi * i / 10
        a.inkline([(140 + 9 * math.cos(t), 150 + 9 * math.sin(t)), (140 + 60 * math.cos(t), 150 + 60 * math.sin(t))], 4.5)
    a.inkdot(140, 150, 7); return a

def corn():
    a = Art()
    a.part(lens(136, 246, 60, 112, 26, bend=-8)); a.part(lens(144, 246, 220, 118, 26, bend=8))
    cob = sm([(140, 38), (112, 64), (102, 128), (110, 208), (140, 238), (170, 208), (178, 128), (168, 64)]); a.body(cob)
    for y in range(66, 224, 19):
        a.cut([(96, y), (140, y + 5), (184, y)], 3.6, clip=cob)
    for x in (122, 140, 158):
        a.cut([(x, 40), (x, 240)], 3.6, clip=cob)
    a.rim(cob)
    return a

def cauliflower():
    a = Art()
    a.part(lens(72, 216, 124, 150, 18, bend=-8)); a.part(lens(208, 216, 156, 150, 18, bend=8)); a.part(lens(140, 254, 140, 188, 20))
    for x, y, r in [(92, 134, 42), (188, 134, 42), (140, 104, 48), (108, 172, 42), (172, 172, 42)]:
        a.body(ell(x, y, r, r * 0.94)); a.hl(x, y, r, r * 0.94, 200, 250)
    return a

def lettuce():
    a = Art()
    pts = [(140 + (80 + 9 * math.sin(12 * t)) * math.cos(t), 156 + (74 + 9 * math.sin(12 * t)) * math.sin(t))
           for t in (2 * math.pi * i / 240 for i in range(240))]
    a.body(pts)
    for k in (-46, -23, 0, 23, 46):
        a.cut(spline([(140, 232), (140 + k * 0.5, 190), (140 + k * 1.4, 130), (140 + k * 1.2, 90)], False), 4.5)
    return a

def zucchini():
    a = Art(); body = capsule(80, 208, 206, 100, 40); a.body(body)
    a.pline([(210, 92), (230, 70), (246, 62)], 14)
    for off in (-14, 0, 14):
        a.cut([(78 + off * 0.6, 214 - off * 0.8 + 6), (200 + off * 0.6, 106 - off * 0.8 + 6)], 4, clip=body)
    a.rim(body); return a

def eggplant():
    a = Art()
    a.body(sm([(120, 86), (160, 88), (186, 122), (206, 172), (188, 226), (140, 248), (92, 228), (76, 178), (96, 126)]))
    a.part([(100, 96), (180, 96), (194, 122), (140, 110), (86, 122)]); a.pline([(140, 96), (150, 60), (176, 46)], 8)
    a.hl(140, 176, 66, 70, 190, 240); return a

def acorn_squash():
    a = Art()
    b = sm([(140, 98), (86, 112), (64, 164), (94, 216), (140, 242), (186, 216), (216, 164), (194, 112)]); a.body(b)
    for k in (-34, -12, 12, 34):
        a.cut(spline([(140 + k * 0.4, 102), (140 + k * 1.9, 164), (140 + k * 1.2, 232)], False), 4.5, clip=b)
    a.rim(b); a.part([(124, 100), (130, 66), (152, 62), (156, 100)]); return a

def cabbage():
    a = Art(); b = ell(140, 156, 82, 78); a.body(b)
    for k in (-52, -26, 0, 26, 52):
        a.cut(spline([(140, 234), (140 + k * 0.6, 190), (140 + k * 1.4, 130), (140 + k * 1.1, 84)], False), 4.5, clip=b)
    a.rim(b); a.part(lens(140, 84, 112, 44, 11)); return a

def pineapple():
    a = Art()
    for x1, y1 in [(140, 16), (108, 30), (172, 30), (82, 54), (198, 54), (60, 84), (220, 84)][:5]:
        a.part(lens(140, 100, x1, y1, 9))
    body = ell(140, 172, 64, 80); a.body(body)
    for i in range(-8, 9):
        a.cut([(60 + i * 20, 90), (220 + i * 20, 254)], 3.6, clip=body)
        a.cut([(220 + i * 20, 90), (60 + i * 20, 254)], 3.6, clip=body)
    a.rim(body)
    return a

def jicama():
    a = Art(); a.body(ell(140, 154, 84, 64)); a.part(lens(124, 96, 106, 66, 9)); a.part(lens(156, 96, 176, 66, 9))
    a.pline([(140, 214), (144, 244), (150, 256)], 5); a.hl(140, 154, 84, 64)
    a.cut(spline([(70, 150), (140, 168), (210, 150)], False), 4); return a

def celery():
    a = Art()
    for x0, x1, r in [(112, 84, 18), (168, 196, 18), (140, 140, 21)]:
        c = capsule(x0, 232, x1, 92, r); a.body(c)
        a.cut([(x0 - r * 0.35, 224), (x1 - r * 0.35, 100)], 3.6, clip=c); a.cut([(x0 + r * 0.35, 224), (x1 + r * 0.35, 100)], 3.6, clip=c); a.rim(c)
    for x1, y1 in [(92, 38), (140, 28), (188, 38)]:
        a.part(lens(140, 94, x1, y1, 13))
    return a

def cantaloupe():
    a = Art(); b = ell(140, 158, 78, 74); a.body(b)
    for i in range(-6, 7):
        a.cut([(50 + i * 30, 70), (230 + i * 30, 250)], 3.4, clip=b)
        a.cut([(230 + i * 30, 70), (50 + i * 30, 250)], 3.4, clip=b)
    a.rim(b); a.part([(130, 88), (134, 62), (152, 62), (152, 88)]); return a

def winter_melon():
    a = Art(); a.body(ell(140, 158, 66, 88)); a.part([(132, 76), (138, 50), (154, 52), (150, 78)])
    for x, y in [(106, 118), (176, 128), (110, 196), (172, 200), (140, 106), (96, 160), (186, 164), (140, 220)]:
        a.dot(x, y, 4)
    a.hl(140, 158, 66, 88); return a

def romaine():
    a = Art()
    a.body(lens(112, 238, 76, 62, 38, bend=-6)); a.body(lens(168, 238, 204, 62, 38, bend=6)); a.body(lens(140, 246, 140, 36, 42))
    for x1 in (76, 204, 140):
        x0 = 112 if x1 == 76 else (168 if x1 == 204 else 140)
        a.cut([(x0, 232), ((x0 + x1) / 2, 150), (x1, 70)], 4.5)
    return a

def swiss_chard():
    a = Art()
    a.body(lens(130, 224, 70, 50, 38, bend=-6)); a.body(lens(150, 224, 210, 50, 38, bend=6)); a.body(lens(140, 230, 140, 30, 40))
    for x1 in (70, 210, 140):
        a.cut([(140, 226), ((140 + x1) / 2, 140), (x1, 62)], 8)
    a.body(sm([(118, 200), (162, 200), (156, 252), (124, 252)])); return a

def leek():
    a = Art()
    for x1, y1, w in [(66, 40, 22), (214, 40, 22), (112, 22, 18), (168, 22, 18)]:
        a.part(lens(140, 150, x1, y1, w))
    b = capsule(140, 236, 140, 150, 28); a.body(b)
    a.cut([(112, 150), (140, 176), (168, 150)], 4.5); a.cut([(140, 178), (140, 238)], 4.5); return a

def watermelon():
    # Slice: rind, flesh, seeds. Flat edge on top.
    a = Art()
    rind = arc(140, 96, 112, 120, 0, 180, 90)
    a.body(rind)
    a.cutfill(arc(140, 110, 92, 94, 0, 180, 90))
    for x, y, d in [(98, 146, 70), (140, 164, 90), (182, 146, 110), (118, 190, 75), (162, 190, 105), (140, 128, 90)]:
        a.inkdot(x, y, 5.5)
    return a

def pumpkin():
    a = Art()
    a.body(ell(98, 170, 48, 68)); a.body(ell(182, 170, 48, 68)); a.body(ell(140, 168, 44, 74))
    a.part([(126, 96), (130, 62), (152, 54), (154, 96)]); a.pline([(152, 68), (184, 50), (202, 70), (190, 84)], 5); return a


OBJECTS = [
    (6, "blueberry", blueberry), (7, "coffee-bean", coffee_bean), (8, "raspberry", raspberry),
    (9, "cherry", cherry), (10, "strawberry", strawberry), (11, "apricot", apricot),
    (12, "kiwi", kiwi), (13, "lemon", lemon), (14, "peach", peach), (15, "apple", apple),
    (16, "avocado", avocado), (17, "pomegranate", pomegranate), (18, "bell-pepper", bell_pepper),
    (19, "mango", mango), (20, "banana", banana), (21, "carrot", carrot), (22, "coconut", coconut),
    (23, "grapefruit", grapefruit), (24, "corn", corn), (25, "cauliflower", cauliflower),
    (26, "lettuce", lettuce), (27, "zucchini", zucchini), (28, "eggplant", eggplant),
    (29, "acorn-squash", acorn_squash), (30, "cabbage", cabbage), (31, "pineapple", pineapple),
    (32, "jicama", jicama), (33, "celery", celery), (34, "cantaloupe", cantaloupe),
    (35, "winter-melon", winter_melon), (36, "romaine", romaine), (37, "swiss-chard", swiss_chard),
    (38, "leek", leek), (39, "watermelon", watermelon), (40, "pumpkin", pumpkin),
]

NAMES_ZH = {6: "蓝莓", 7: "咖啡豆", 8: "覆盆子", 9: "樱桃", 10: "草莓", 11: "杏子", 12: "猕猴桃",
            13: "柠檬", 14: "桃子", 15: "苹果", 16: "牛油果", 17: "石榴", 18: "甜椒", 19: "芒果",
            20: "香蕉", 21: "胡萝卜", 22: "椰子", 23: "西柚", 24: "玉米", 25: "花菜", 26: "生菜",
            27: "西葫芦", 28: "茄子", 29: "金瓜", 30: "卷心菜", 31: "菠萝", 32: "凉薯", 33: "芹菜",
            34: "哈密瓜", 35: "冬瓜", 36: "罗马生菜", 37: "牛皮菜", 38: "韭葱", 39: "西瓜", 40: "南瓜"}

PROMPT = """# ImageGen prompt for replacing one illustration

Keep the file name `week-NN-slug.png` and rerun `tools/home_lab/render_lab.sh`.

Minimal geometric poster illustration of ONE {object}: a bold flat black
silhouette built from clean circles, ellipses and leaf shapes, details cut out as
white negative space (stencil / linocut feel), white gaps between overlapping
parts, constant-width crisp curves, Bauhaus / modern Japanese gallery print.
Pure black on pure white only: no gray, no gradients, no texture, no outline
halo, no text, no face, no background. Centered on a square canvas with a
generous white margin; thinnest white line at least 2% of the canvas width.
"""


def contact_sheet(tiles, path):
    cols, label_h = 7, 30
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("L", (cols * (N + 10) + 10, rows * (N + 10 + label_h) + 10), 128)
    try:
        font = ImageFont.truetype("/System/Library/Fonts/STHeiti Medium.ttc", 20)
    except OSError:
        font = ImageFont.load_default()
    draw = ImageDraw.Draw(sheet)
    for i, (week, slug, im) in enumerate(tiles):
        x, y = 10 + (i % cols) * (N + 10), 10 + (i // cols) * (N + 10 + label_h)
        sheet.paste(im.convert("L"), (x, y + label_h))
        draw.rectangle([x, y, x + N - 1, y + label_h - 1], fill=255)
        draw.text((x + 8, y + 3), f"{week}  {NAMES_ZH.get(week, slug)}", font=font, fill=0)
    sheet.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    ap.add_argument("--only", default="")
    ap.add_argument("--styles", default=",".join(STYLES))
    ap.add_argument("--compare", action="store_true", help="also write style_compare.png")
    args = ap.parse_args()
    styles = [x for x in args.styles.split(",") if x]
    out = Path(args.out_dir)
    only = {int(x) for x in args.only.split(",") if x}
    for style in styles:
        folder = out / style
        folder.mkdir(parents=True, exist_ok=True)
        tiles = []
        for week, slug, fn in OBJECTS:
            if only and week not in only:
                continue
            im = render_hand(fn(), slug, week) if style == "hand" else render(fn(), style)
            im.save(folder / f"week-{week:02d}-{slug}.png")
            tiles.append((week, slug, im))
        contact_sheet(tiles, folder / "contact_sheet.png")
        (folder / "PROMPT.md").write_text(PROMPT, encoding="utf-8")
    if args.compare:
        pick = [6, 9, 10, 16, 21, 24, 31, 39, 40]
        names = {w: fn for w, _, fn in OBJECTS}
        slug_of = {w: sl for w, sl, _ in OBJECTS}
        sheet = Image.new("L", (len(pick) * 150 + 130, len(styles) * 150 + 10), 255)
        font = ImageFont.truetype("/System/Library/Fonts/STHeiti Medium.ttc", 18)
        d = ImageDraw.Draw(sheet)
        for r, st in enumerate(styles):
            d.text((10, r * 150 + 62), st, font=font, fill=0)
            for c, w in enumerate(pick):
                im = (render_hand(names[w](), slug_of[w], w) if st == "hand" else render(names[w](), st)).convert("L").resize((140, 140), Image.LANCZOS)
                sheet.paste(im, (130 + c * 150, r * 150 + 5))
        sheet.save(out / "style_compare.png")
    print(f"{len(OBJECTS) if not only else len(only)} objects x {len(styles)} styles -> {out}")


if __name__ == "__main__":
    main()
