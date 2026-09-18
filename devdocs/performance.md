# Performance baseline

> Reference numbers for the showreel, to compare later changes against.
> Measured on the device, not estimated.

## Baseline: 2026-09-18, ship turntable

**Result: a steady 30 FPS (33.4 ms/frame). The display's vsync sets that rate,
not the renderer.** At the heaviest pose the app uses about 21.8 ms of its
33.3 ms frame, leaving roughly 11.6 ms of headroom.

### What was running

| | |
|---|---|
| Scene | Race the Synth ship on a turntable, black backdrop (`main/ship.c`) |
| Mesh | 169 verts / 306 tris / 84 outline edges; **115–152 tris visible** after the app's back-face cull |
| Screen coverage | up to ~719 px wide (broadside) on 800×480 |
| Renderer | `SE_RENDER_ZBUFFER`, `frustum_cull` on, `depth_order` off |
| Lighting | engine `se_light`, one positional light, 75% brightness, `two_sided` |
| Backdrop | full-screen PPA FILL, overlapped with submit + prepare |
| Framebuffers | 2× 800×480 RGB565 in PSRAM |
| Code | showreel: the commit that adds this file; engine V1.5 `d9f8e5a` |

### Per-phase time, ms per frame

Each row is one 1-second window from the `ms/frame:` log line. There are eight
consecutive windows, covering a bit less than one turntable revolution.

| frame | fps | fill | submit | prep | wait | rast | blit | vsync | rest |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 33.38 | 30.0 | 0.06 | 0.57 | 0.04 | 1.77 | 15.93 | 0.47 | 14.46 | 0.09 |
| 33.51 | 29.8 | 0.06 | 0.55 | 0.04 | 1.80 | 16.69 | 0.51 | 13.78 | 0.09 |
| 33.05 | 30.3 | 0.06 | 0.58 | 0.04 | 1.77 | 12.79 | 0.51 | 17.22 | 0.09 |
| 33.11 | 30.2 | 0.06 | 0.59 | 0.04 | 1.76 |  7.77 | 0.48 | 22.32 | 0.09 |
| 33.41 | 29.9 | 0.06 | 0.58 | 0.04 | 1.77 |  7.27 | 0.48 | 23.13 | 0.09 |
| 33.86 | 29.5 | 0.06 | 0.58 | 0.04 | 1.77 | 11.74 | 0.45 | 19.14 | 0.09 |
| 33.49 | 29.9 | 0.06 | 0.57 | 0.04 | 1.78 | 17.44 | 0.43 | 13.10 | 0.09 |
| 33.22 | 30.1 | 0.06 | 0.57 | 0.04 | 1.78 | 18.91 | 0.40 | 11.37 | 0.09 |

Summary:

| phase | min | max | mean | what it is |
|---|---:|---:|---:|---|
| fill   |  0.06 |  0.06 |  0.06 | queuing the PPA FILL (the fill itself runs on hardware) |
| submit |  0.55 |  0.59 |  0.57 | model transform, back-face cull, `scene_tri` including engine lighting |
| prep   |  0.04 |  0.04 |  0.04 | `scene_prepare`: engine cull + order |
| wait   |  1.76 |  1.80 |  1.77 | waiting for the PPA FILL to finish |
| rast   |  7.27 | 18.91 | 13.57 | `scene_rasterize`, depends on pose |
| blit   |  0.40 |  0.51 |  0.47 | `bsp_display_blit` |
| vsync  | 11.37 | 23.13 | 16.82 | idle, waiting for the tearing-effect signal |
| rest   |  0.09 |  0.09 |  0.09 | input pump, `on_update`, loop overhead |
| **app work** (fill → rast) | **9.72** | **21.36** | 16.02 | |

Rasterize split for the last frame of each window (`scene_raster_stats`):
triangles took **5.3–15.2 ms** and edges **1.2–4.9 ms**. Both follow screen
coverage, not triangle count: 152 triangles at the edge-on pose took less time
than 115 at the broadside one.

Internal SRAM, taken once per window: **163 KiB free, 62 KiB largest block**.
The value was the same in every window.

### Reading it

- **Vsync caps the frame rate.** App work plus blit ranges from 10.2 to 21.8 ms,
  yet every frame is 33.3 ms. The `vsync` wait moves in exact step with `rast`
  and fills the frame out to 33.3 ms. At the lightest pose the wait is 23 ms,
  which is more than a whole 60 Hz period. So the tearing-effect signal is
  arriving at about 30 Hz: frames aren't just missing a 60 Hz window. The cause
  wasn't investigated, because 30 FPS is the target.
- **Headroom is about 11.6 ms at the heaviest pose** (33.3 − 21.8). Content can
  grow by roughly that much before the frame rate drops to the next vsync step.
- **`rast` is the only phase that varies**, and it depends on pose. It is
  fill-bound: its cost follows the pixels the ship covers. It peaks broadside,
  when the near wing is magnified by perspective.
- **The PPA overlap doesn't fully hide the fill.** The full-screen FILL takes
  about 2.4 ms. Only about 0.67 ms of CPU work (fill + submit + prep) runs
  alongside it, so each frame waits about 1.8 ms. That is still far cheaper than
  a CPU clear. If the CPU fallback ran instead, `fill` would jump from 0.06 to
  several milliseconds.
- **The engine's lighting is cheap.** All of `submit`, including one normal,
  one dot product and one `sqrtf` per face, costs 0.57 ms.
- **The blit only starts the transfer** (0.4–0.5 ms). It isn't a
  full-framebuffer copy on the CPU.

## 2026-09-18: textured hull

Same scene, but the gold hull is textured with four 64×64 metal plates in
internal SRAM (`scene_textured_tri`, engine V1.5). Twenty-one consecutive
1-second windows, covering about two turntable revolutions:

| | baseline (flat gold) | textured | |
|---|---:|---:|---|
| **fps** | 29.5–30.3 | **29.1–30.4** | the heaviest window slips below 30 |
| rast | 7.3–18.9 ms | **8.1–30.9 ms** | |
| &nbsp;&nbsp;flat tris | 5.3–15.2 ms | 0.9–3.9 ms (81–101 tris) | the lamps, poles and panel |
| &nbsp;&nbsp;**textured tris** | — | **5.5–23.2 ms** (34–69 tris) | the hull |
| &nbsp;&nbsp;edges | 1.2–4.9 ms | 3.3–4.2 ms | unchanged |
| submit | 0.55–0.59 ms | 0.70–0.94 ms | UV setup + per-face shade |
| vsync (slack) | 11.4–23.1 ms | **0.01–22.0 ms** | |
| SRAM free / largest | 163 / 62 KiB | 131 / 62 KiB | −32 KiB: the four plates |

**The heaviest pose no longer fits the frame.** App work plus blit peaks at
about 34.3 ms against the 33.3 ms vsync period, so in that window some frames
wait for the next vsync (29.1 FPS). Every other window holds 30 FPS. The
textured pass costs roughly 3–4× what the same pixels cost as a flat fill.
Each drawn pixel now pays a float divide, a texel fetch and the shade multiply
on top of the depth test and framebuffer write.

The 32 KiB drop in free SRAM is exactly the four plates, so they did land
internal. The 68 KB textured-triangle list is larger than the 62 KiB largest
free block, so it must have gone to PSRAM. That is inferred from the numbers:
the boot log line saying so wasn't captured.

### Is the perspective divide the cost? (measured: mostly no)

The textured loop divides once per drawn pixel, so the obvious suspect is
that divide. Each variant below ran for about two turntable revolutions
(21–22 one-second windows). Each ran after the app was restarted.

| variant | `rast` mean | `rast` max | textured pass, mean |
|---|---:|---:|---:|
| **one divide per pixel (kept)** | **20.52** | 30.91 | **15.25** |
| no divide: reciprocal once per column (wrong image, timing only), 2 runs | 19.02 / 18.85 | 26.31 / 25.99 | 13.73 / 13.31 |
| span-subdivided, 16 px, first version | 21.76 | 29.73 | 16.40 |
| span-subdivided, 16 px, one reciprocal + carried 16.16 fixed point | 21.09 | 29.17 | 15.83 |

- **The divide is about 10% of the textured pass**, roughly 1.5 ms. The hull
  costs about 7 ms more textured than as flat gold, so the other ~5.5 ms is
  elsewhere: float→int conversions, the texel fetch, and the shade multiply.
- **Span subdivision loses on this model.** Textured hull columns are short.
  Over 12 yaw × 3 nod poses the median column is 8 px and the mean 12, and
  75% of columns fit in one span. Each column pays an exact sample at its
  start plus one per span, so the average is one divide per 5.1 pixels, not
  per 16. The per-column and per-span setup costs more than the ~1.2 ms that
  could save.
- The divide is also cheaper than its latency suggests. It sits next to a
  PSRAM depth read, and part of it hides behind that stall.
- Span subdivision should pay off for large triangles with long columns
  (floors, walls, anything filling the screen). Revisit it for that kind of
  content.
- Noise: the two no-divide runs agree to within 0.17 ms, so the 0.6 ms gap
  between the per-pixel and span versions is about 3× that. The per-pixel
  run was measured only once.

## 2026-09-18: engine 2.0, outline off, flames, smaller ship

Three changes since the textured-hull numbers above, measured one after
another (two revolutions each):

| | textured hull | + outline off | + flames, ship at 81% span |
|---|---:|---:|---:|
| fps | 29.1–30.4 | 29.7–30.2 | 29.6–31.1 |
| `rast` mean / max | 20.52 / 30.91 ms | 17.69 / 25.83 ms | **11.43 / 15.87 ms** |
| textured pass mean | 15.25 ms | 15.42 ms | 9.98 ms (47–74 tris) |
| least vsync slack | 0.01 ms | 4.39 ms | **14.26 ms** |
| SRAM free / largest | 131 / 62 KiB | 131 / 62 KiB | 130 / 62 KiB |

- **Outline off:** the 84 edges cost 3–5 ms a frame, about the frame's
  whole remaining margin with texturing on.
- **Flames** add 12 emissive textured triangles at most (a few visible at a
  time). They make the silhouette longer, so the ship shrank from a 2.8 to a
  2.25 world-unit span to keep every pose on screen. That smaller ship, not
  the flames, is where the drop in `rast` comes from: about 65% of the
  previous screen area, so about 65% of the fill.
- The boot log confirms what was inferred earlier: the 68 KB
  textured-triangle list is in **PSRAM** (it doesn't fit the 62 KiB largest
  internal block), and all five textures (four plates and the 1 KB flame)
  are in **internal SRAM**.

### Reproducing

```sh
make install && make run
# then read the console (graceloader logs there; no USB mode switch needed).
# pyserial lives in the ESP-IDF python env, hence the source:
source "$IDF_SOURCE" >/dev/null && python3 -c "
import serial, time
s = serial.serial_for_url('rfc2217://localhost:4001', timeout=1)
t = time.time() + 12; b = b''
while time.time() < t: b += s.read(1024)
print(b.decode(errors='replace'))"
```

Every second the app logs two lines:
- the phase split (`main/profile.c`)
- FPS, renderer, raster split and SRAM (`log_frame_stats()` in `main/main.c`)

`make install` fails with `ConnectionResetError` while the app is still
running. Exit it first.
