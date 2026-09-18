#!/usr/bin/env python3
"""Generate the showreel's bare-metal plate textures into textures/.

Procedural and seeded, so the PNGs are reproducible from this script
instead of being opaque binaries: change a number here, re-run, commit
both. Every texture tiles seamlessly -- all noise is built periodic
(FFT smoothing wraps around) and every feature is drawn modulo the
texture size -- because the ship repeats them across large faces.

Plates are 64x64: the engine needs power-of-two edges (it wraps with a
mask), and at 2 bytes a texel in RGB565 a plate costs 8 KB of internal
SRAM. The flame is 64x8 (1 KB).

    python3 tools/make_textures.py            # write textures/*.png
    python3 tools/make_textures.py --preview  # also write a 4x contact sheet
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image

N = 64
OUT = Path(__file__).resolve().parent.parent / "textures"
rng = np.random.default_rng(0x5E3D)


def periodic_noise(sx, sy, gen=None):
    """Tileable noise: white noise low-passed with a Gaussian in the FFT
    domain (convolution there is circular, so the result wraps). sx/sy
    are the blur radii in texels; unequal radii give streaks. `gen` is
    the random generator (default: the shared one of the hull plates)."""
    white = (gen or rng).standard_normal((N, N))
    fy = np.fft.fftfreq(N)[:, None]
    fx = np.fft.fftfreq(N)[None, :]
    g = np.exp(-2 * (np.pi ** 2) * ((fx * sx) ** 2 + (fy * sy) ** 2))
    out = np.real(np.fft.ifft2(np.fft.fft2(white) * g))
    return out / (np.abs(out).max() + 1e-9)          # roughly -1..1


def to_rgb(lum, tint):
    """Luminance offsets around a base colour -> clipped uint8 RGB."""
    base = np.array(tint, dtype=float)[None, None, :]
    return np.clip(base + lum[:, :, None], 0, 255).astype(np.uint8)


def seam(lum, dark=-48, light=22):
    """A panel seam along row 0 / column 0, lit from the top-left: a dark
    groove with a bright lip below / right of it. Repeating the texture
    turns that into a plate grid."""
    lum[0, :] += dark
    lum[:, 0] += dark
    lum[1, 1:] += light
    lum[1:, 1] += light


def rivet(lum, x, y):
    """A 3x3 domed rivet head: bright top-left, shadow bottom-right."""
    for dy in range(-1, 2):
        for dx in range(-1, 2):
            lum[(y + dy) % N, (x + dx) % N] += 26 - 16 * (dx + dy)
    lum[(y + 2) % N, (x + 2) % N] -= 34


def riveted():
    """Fuselage: mid steel, soft mottling, one plate per tile with a
    rivet line inset along its two seams."""
    lum = 9 * periodic_noise(6, 6) + 4 * periodic_noise(1.2, 1.2)
    seam(lum)
    for k in range(4, N, 8):
        rivet(lum, k, 4)
        rivet(lum, 4, k)
    return to_rgb(lum, (148, 151, 156))


def brushed():
    """Wings: bright brushed steel -- long horizontal grain -- with a
    single seam, so the wing reads as a few large sheets."""
    lum = 15 * periodic_noise(14, 0.45) + 5 * periodic_noise(3, 0.8)
    seam(lum, dark=-40, light=18)
    return to_rgb(lum, (176, 179, 184))


def gunmetal():
    """Pods: dark blue-grey, vertical cooling grooves every 8 texels and
    a few bright scratches."""
    lum = 7 * periodic_noise(5, 5)
    for x in range(0, N, 8):
        lum[:, x] -= 30
        lum[:, (x + 1) % N] += 14
    for _ in range(6):
        x0, y0 = rng.integers(0, N, 2)
        length = rng.integers(8, 22)
        dx = rng.choice([-1, 1])
        for i in range(length):
            lum[(y0 + i) % N, (x0 + dx * i) % N] += 30
    return to_rgb(lum, (92, 98, 110))


def tread():
    """Undersides: diamond tread plate -- raised lozenges on a 16-texel
    grid, alternating 45-degree orientation checkerboard-style, each lit
    from the top-left."""
    lum = 6 * periodic_noise(4, 4)
    yy, xx = np.mgrid[0:N, 0:N]
    cell = 16
    for cy in range(0, N, cell):
        for cx in range(0, N, cell):
            flip = ((cx // cell) + (cy // cell)) % 2
            mx, my = cx + cell // 2, cy + cell // 2
            # Distance to the cell centre, wrapped so edge lozenges tile.
            ddx = (xx - mx + N // 2) % N - N // 2
            ddy = (yy - my + N // 2) % N - N // 2
            a = (ddx + ddy) if not flip else (ddx - ddy)   # along the lozenge
            b = (ddx - ddy) if not flip else (ddx + ddy)   # across it
            body = (np.abs(a) <= 7) & (np.abs(b) <= 1.5)
            lum[body] += 22
            # Lit edge on the upper-left side, shadow on the lower-right.
            edge = (np.abs(a) <= 7) & (np.abs(b) > 1.5) & (np.abs(b) <= 2.6)
            upper = (ddy + ddx) < 0 if flip else (ddy - ddx) < 0
            lum[edge & upper] += 12
            lum[edge & ~upper] -= 26
    return to_rgb(lum, (122, 124, 129))


def flame():
    """Engine flame, 64x8: u (x) runs along the flame from the nozzle
    (x = 0) to the tip, v (y) around it. A white-hot core fading through
    blue to a deep blue tip, with faint lengthwise streaks that fade in
    after the core so the white stays clean. Drawn emissive, so these
    are the exact on-screen colours. Not tiled along u: the flame maps it
    once, stopping short of the right edge so the tip never wraps back
    to white."""
    w, h = 64, 8
    stops = [(0.00, (235, 250, 255)), (0.15, (150, 212, 255)), (0.40, (60, 132, 255)),
             (0.75, (26, 62, 205)), (1.00, (10, 26, 112))]
    xs = np.linspace(0.0, 1.0, w)
    ramp = np.zeros((w, 3))
    for c in range(3):
        ramp[:, c] = np.interp(xs, [p for p, _ in stops], [col[c] for _, col in stops])
    streak = np.array([1.00, 0.90, 1.06, 0.94, 1.00, 0.88, 1.05, 0.95])[:, None, None]
    fade = np.clip((xs - 0.12) / 0.3, 0.0, 1.0)[None, :, None]   # no streaks in the core
    img = ramp[None, :, :] * (1.0 + (streak - 1.0) * fade)
    return np.clip(img, 0, 255).astype(np.uint8)


# --- Station and marauder textures -----------------------------------
#
# Their own generator, so adding them left the hull plates above (which
# share `rng`, in order) byte-identical.
rng2 = np.random.default_rng(0x57A7)


def panel_grid(lum, step, dark=-34, light=14):
    """Panel seams every `step` texels (tileable when step divides N)."""
    for k in range(0, N, step):
        lum[k, :] += dark
        lum[:, k] += dark
        lum[(k + 1) % N, :] += light
        lum[:, (k + 1) % N] += light


def station_hull():
    """Station hub and faces: the 2001 station's off-white, large panels
    (two per tile each way), soft mottling and a few small dark vents."""
    lum = 5 * periodic_noise(7, 7, rng2) + 2 * periodic_noise(1.5, 1.5, rng2)
    panel_grid(lum, 32)
    for _ in range(5):
        x0, y0 = rng2.integers(2, N - 8, 2)
        w, h = rng2.integers(3, 7), rng2.integers(2, 4)
        lum[y0:y0 + h, x0:x0 + w] -= 70
        lum[y0 + h, x0:x0 + w] += 20      # lit lower lip
    return to_rgb(lum, (200, 202, 204))


def station_ring():
    """Ring walls: the hull panelling with a band of lit windows across
    the middle -- read as habitation. (Lit only by the scene light, not
    emissive: on the night side the windows go dark with the wall.)"""
    lum = 5 * periodic_noise(7, 7, rng2)
    panel_grid(lum, 32)
    img = to_rgb(lum, (198, 200, 202)).astype(int)
    band = slice(26, 38)
    img[band, :, :] = (img[band, :, :] * 0.35).astype(int)      # dark window band
    for x in range(2, N, 8):                                     # 8 windows per tile
        on = rng2.random() < 0.8
        col = (255, 226, 150) if on else (60, 66, 80)
        img[29:35, x:x + 4, :] = col
    return np.clip(img, 0, 255).astype(np.uint8)


def marauder_wear():
    """Shared by both marauder liveries, so the two ships are visibly the
    same type: panel seams, grime (a luminance field) and a mask of where
    the paint is scratched or chipped down to bare metal."""
    grime = 16 * periodic_noise(5, 5, rng2) + 6 * periodic_noise(1.2, 1.2, rng2)
    panel_grid(grime, 16, dark=-40, light=10)
    bare = np.zeros((N, N), bool)
    for _ in range(9):                                   # scratches
        x0, y0 = rng2.integers(0, N, 2)
        length = rng2.integers(6, 20)
        dx = rng2.choice([-1, 1])
        for i in range(length):
            bare[(y0 + i // 2) % N, (x0 + dx * i) % N] = True
    chips = periodic_noise(2.2, 2.2, rng2) > 0.55        # chipped patches
    return grime, bare | chips


_WEAR = None


def marauder(paint):
    global _WEAR
    if _WEAR is None:
        _WEAR = marauder_wear()
    grime, bare = _WEAR
    img = to_rgb(grime, paint).astype(int)
    metal = to_rgb(grime * 0.6, (128, 130, 134)).astype(int)
    img[bare] = metal[bare]
    return np.clip(img, 0, 255).astype(np.uint8)


def marauder_green():
    """Marauder #1: old, dirty green paint."""
    return marauder((70, 94, 56))


def marauder_yellow():
    """Marauder #2: darkened, sooty yellow."""
    return marauder((150, 124, 38))


def flame_red():
    """Marauder engine flame, 64x8: the counterpart of flame(), running
    white-hot -> orange -> deep red."""
    w, h = 64, 8
    stops = [(0.00, (255, 246, 222)), (0.15, (255, 196, 96)), (0.40, (255, 112, 32)),
             (0.75, (196, 38, 16)), (1.00, (92, 12, 8))]
    xs = np.linspace(0.0, 1.0, w)
    ramp = np.zeros((w, 3))
    for c in range(3):
        ramp[:, c] = np.interp(xs, [p for p, _ in stops], [col[c] for _, col in stops])
    streak = np.array([1.00, 0.90, 1.06, 0.94, 1.00, 0.88, 1.05, 0.95])[:, None, None]
    fade = np.clip((xs - 0.12) / 0.3, 0.0, 1.0)[None, :, None]
    img = ramp[None, :, :] * (1.0 + (streak - 1.0) * fade)
    return np.clip(img, 0, 255).astype(np.uint8)


TEXTURES = {
    "plate_riveted.png": riveted,
    "plate_brushed.png": brushed,
    "plate_gunmetal.png": gunmetal,
    "plate_tread.png": tread,
    "flame.png": flame,
    "station_hull.png": station_hull,
    "station_ring.png": station_ring,
    "marauder_green.png": marauder_green,
    "marauder_yellow.png": marauder_yellow,
    "flame_red.png": flame_red,
}


def main():
    OUT.mkdir(exist_ok=True)
    tiles = []
    for name, fn in TEXTURES.items():
        img = fn()
        Image.fromarray(img, "RGB").save(OUT / name, optimize=True)
        tiles.append(img)
        print(f"wrote {OUT / name}")
    if "--preview" in sys.argv:
        # Each texture repeated 2x2 (so seams and wrap-around are visible)
        # and scaled 4x, side by side.
        # Each texture 2x2, padded to the tallest so they sit side by side.
        tall = max(t.shape[0] for t in tiles) * 2
        blocks = [np.tile(t, (2, 2, 1)) for t in tiles]
        blocks = [np.pad(b, ((0, tall - b.shape[0]), (0, 4), (0, 0))) for b in blocks]
        row = np.concatenate(blocks, axis=1)
        prev = Image.fromarray(row, "RGB").resize((row.shape[1] * 4, row.shape[0] * 4), Image.NEAREST)
        path = Path(sys.argv[sys.argv.index("--preview") + 1]) if len(sys.argv) > sys.argv.index("--preview") + 1 \
            else OUT.parent / "build" / "textures_preview.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        prev.save(path)
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
