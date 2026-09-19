#!/usr/bin/env python3
"""Generate the app icons, metadata/icon{16,32,64}.png.

A film reel (the showreel) with a "2" on it (SynthEngine3D 2.0): a
steel-blue reel with round cut-outs round the hub, film unspooling from
its bottom straight out to the right edge (sprocket holes, magenta and
cyan frames), and a big magenta "2" with a dark outline over the middle.
Transparent background, like the other apps' icons.

The 64 and 32 px icons are drawn at 8x and scaled down (anti-aliased);
the 16 px one is drawn on its own, simpler (no film strip, fewer holes),
because at that size the detail turns to mud. The "2" is built from
geometry (an arc, a diagonal, a bar), so no font is needed.

    python3 tools/make_icons.py            # write metadata/icon*.png
    python3 tools/make_icons.py --preview  # also build/icons_preview.png
"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "metadata"

MAGENTA = (255, 49, 241, 255)     # SynthEngine's magenta (0xFFFF31F1)
CYAN = (60, 220, 255, 255)
OUTLINE = (24, 16, 40, 255)
STEEL = (150, 164, 190, 255)
STEEL_DARK = (84, 94, 118, 255)
HUB = (60, 66, 84, 255)
FILM = (34, 30, 44, 255)
SPROCKET = (200, 205, 220, 255)
CLEAR = (0, 0, 0, 0)


def two_path(cx, cy, h):
    """The "2" as a polyline, `h` tall, centred on (cx, cy): the bowl (an
    arc from the left round over the top), a diagonal down to the lower
    left, the base bar to the right."""
    w = 0.62 * h
    r = 0.27 * h
    bx, by = cx, cy - h / 2 + r  # the bowl's centre
    pts = []
    for k in range(0, 25):
        a = math.radians(200 - k * (235 / 24))  # 200 deg round to -35 deg
        pts.append((bx + r * 1.05 * math.cos(a), by - r * math.sin(a)))
    pts.append((cx - w / 2, cy + h / 2))    # the diagonal's foot
    pts.append((cx + w / 2, cy + h / 2))    # the base
    return pts


def draw_two(d, cx, cy, h, stroke, outline):
    pts = two_path(cx, cy, h)
    d.line(pts, fill=OUTLINE, width=int(stroke + 2 * outline), joint="curve")
    for p in (pts[0], pts[-1]):  # round the ends of the outline
        rr = (stroke + 2 * outline) / 2
        d.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=OUTLINE)
    d.line(pts, fill=MAGENTA, width=int(stroke), joint="curve")
    for p in (pts[0], pts[-1]):
        rr = stroke / 2
        d.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=MAGENTA)


def reel(d, cx, cy, r, holes, hole_r, hole_ring, rim):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=STEEL_DARK)
    d.ellipse([cx - r + rim, cy - r + rim, cx + r - rim, cy + r - rim], fill=STEEL)
    for k in range(holes):
        a = math.radians(90 + k * 360 / holes)
        hx, hy = cx + hole_ring * math.cos(a), cy - hole_ring * math.sin(a)
        d.ellipse([hx - hole_r, hy - hole_r, hx + hole_r, hy + hole_r], fill=CLEAR)
    hr = 0.22 * r
    d.ellipse([cx - hr, cy - hr, cx + hr, cy + hr], fill=HUB)


def film_strip(img, cx, cy, r, S):
    """The film unspooling from the bottom of the reel, straight out to
    the right edge (tangent to the reel, so it reads as film leaving it):
    sprocket holes along both edges, coloured frames between them."""
    width = 0.36 * r
    y0, y1 = cy + r - width, cy + r
    d = ImageDraw.Draw(img)
    d.rectangle([cx, y0, S, y1], fill=FILM)
    hole, step = 0.06 * r, 0.14 * r
    x = cx + 0.03 * r
    while x + hole <= S:
        for y in (y0 + 0.04 * r, y1 - 0.04 * r - hole):
            d.rectangle([x, y, x + hole, y + hole], fill=SPROCKET)
        x += step
    colours = [MAGENTA, CYAN]
    fx, fw, k = cx + 0.02 * r, 0.26 * r, 0
    while fx < S:
        c = colours[k % 2]
        d.rectangle([fx, y0 + 0.12 * r, fx + fw, y1 - 0.12 * r], fill=c)
        fx += fw + 0.06 * r
        k += 1


def big_icon(size):
    """64 / 32 px: drawn at 8x, scaled down."""
    S = size * 8
    img = Image.new("RGBA", (S, S), CLEAR)
    cx, cy, r = 0.42 * S, 0.43 * S, 0.41 * S
    film_strip(img, cx, cy, r, S)
    d = ImageDraw.Draw(img)
    reel(d, cx, cy, r, holes=6, hole_r=0.17 * r, hole_ring=0.6 * r, rim=0.08 * r)
    draw_two(d, cx, cy, 0.95 * r, stroke=0.17 * r, outline=0.06 * r)
    return img.resize((size, size), Image.LANCZOS)


def small_icon():
    """16 px: the reel and the "2", nothing else; drawn at 8x as well, but
    with bolder proportions so the numeral stays legible."""
    S = 16 * 8
    img = Image.new("RGBA", (S, S), CLEAR)
    d = ImageDraw.Draw(img)
    cx, cy, r = S / 2, S / 2, 0.48 * S
    reel(d, cx, cy, r, holes=4, hole_r=0.14 * r, hole_ring=0.66 * r, rim=0.1 * r)
    draw_two(d, cx, cy, 1.0 * r, stroke=0.24 * r, outline=0.08 * r)
    return img.resize((16, 16), Image.LANCZOS)


def main():
    icons = {16: small_icon(), 32: big_icon(32), 64: big_icon(64)}
    for n, im in icons.items():
        im.save(OUT / f"icon{n}.png", optimize=True)
        print(f"wrote {OUT / f'icon{n}.png'}")
    if "--preview" in sys.argv:
        # Each icon 8x (nearest), on dark and on light, side by side.
        tiles = []
        for bg in ((40, 40, 48, 255), (230, 230, 236, 255)):
            for n, im in icons.items():
                big = im.resize((n * 8, n * 8), Image.NEAREST)
                tile = Image.new("RGBA", (512 + 16, 512 + 16), bg)
                tile.alpha_composite(big, (8, 8))
                tiles.append(tile)
        sheet = Image.new("RGBA", (len(tiles) * 528, 528))
        for i, t in enumerate(tiles):
            sheet.paste(t, (i * 528, 0))
        path = OUT.parent / "build" / "icons_preview.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(path)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
