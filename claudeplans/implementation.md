# Implementation plan: the SynthEngine showreel

Started as "scene system + spacestation_flyby" (Parts A–C3, steps 0–7, all done). Part C4 and steps 8–12 cover the full eleven-scene reel the user defined afterwards (D-28).

Living document: design, step-by-step status, findings and decisions.
Updated whenever a step starts or finishes, something is measured, or
something is decided. Plan approved by the user on 2026-09-18.

## Context
The showreel becomes a Frontier: Elite II–style intro ("in the spirit of": our own sequence and models, textured). The first real scene is `spacestation_flyby`:
- Our ship (the synthracer ship already in the repo) is chased by two marauders.
- It daringly threads between two spokes of a rotating 2001-style station: hub, spokes, outer ring.
- The marauders take the longer way round the ring, catch up, and shoot at it.

Decisions from the user:
- Scenes live in separate files, and assets live in separate generator modules that any scene can reuse.
- All choreography stays in the app. The engine only renders the render list each frame.
- Stars are a new engine primitive, `scene_point`, a world-space dot, so they move correctly with the camera.
- No transparency for now.
- Performance: look at it afterwards and decide.
- The engine gets real near-plane clipping. This is still unreleased **2.0 work on branch V2.0**: no version bump, and it goes into the existing 2.0 CHANGELOG entry.
- The turntable is kept as an **unused** scene (compiled, not in the playlist).
- Testing that doesn't need the user's eyes is **automated end to end**, the way `../tanmatsu-fonttest` and `../tanmatsu-idf6tests` do it:
  - the app waits for a debug command, runs the test and exits back to the launcher;
  - launcher mode switches happen automatically.
- **Engine problems → stop and ask**, never work around them (D-15).

## About this document
This file (`claudeplans/implementation.md`) is committed with the work. It contains:
- the design (Parts A–C below),
- the step-by-step plan with a status table (Part D),
- the findings log and the decisions log (Part E).

It is updated as each step starts and finishes, and whenever something is measured or decided.

## Part A: engine (synthengine3D, branch V2.0)

### A1. Near-plane clipping (`src/se_scene.c`)
**Today:** a primitive is dropped only if *all* its vertices have cz < `RENDER_NEAR_CLIP_Z` (0.5). Otherwise each behind-plane vertex is clamped to z = 0.5 in `scene_project_cam` (S:252), which distorts the shape and the UVs. This happens in `scene_tri` S:622, `scene_textured_tri` S:664 and `scene_line` S:648.

**New:**
- **Clip helper:** a static helper clips the camera-space triangle against cz ≥ near (Sutherland–Hodgman). Three vertices in give 0, 3 or 4 out, and each new vertex interpolates (cx, cy, cz, u, v) along the crossing edge. That interpolation is exact, because an edge is affine in camera space.
- **`scene_tri` / `scene_textured_tri`:**
  - Transform the vertices, then take the fast path when all three are in front. That path is unchanged, so the turntable's output stays bit-identical.
  - Otherwise clip and emit 1–2 list entries (a fan).
  - The shade and `packed` colour are computed once from the world-space vertices, as now.
  - The textured umin/vmin period shift is computed from the original UVs, before clipping.
  - Cap checks apply per emitted piece.
- **`scene_line`:** move the behind-plane endpoint onto the plane (lerp).
- **The clamp in `scene_project_cam`** stays only as a safety net.
- **Docs:** fix the "whole-triangle near-clip guard" wording in `docs/renderer.md` (~line 83) and the clamp comment in `include/se_config.h` (~246).

### A2. `scene_point` primitive
Follow the textured list as the precedent for lazy allocation, a separate stats call, an appended geometry field, and an exported raster helper.
- **Public API (`include/se_scene.h`):**
  - `void scene_point(float x, float y, float z, uint32_t argb);` next to `scene_line`. It is 1 px and unlit.
  - It is depth-tested against the z-buffer with `>=`, like lines, and never writes depth, so geometry hides stars.
  - New public struct `se_pt_t { se_vtx_t v; uint16_t packed; }`.
  - Append `pts`, `pt_n` to `se_geometry_t`.
  - `void se_scene_raster_points(se_geometry_t const*)` for custom renderers.
  - `void scene_point_stats(int* n, int64_t* us);`
- **Cap:** `SE_SCENE_POINT_CAP 1024` in `include/se_config.h`, `#ifndef`-overridable.
- **Storage:** allocated lazily on the first `scene_point`, in PSRAM (it is read sequentially once a frame; internal SRAM is scarce). Overflow drops silently, like the other lists.
- **Submit-time culling:** drop a point when cz < near or when it projects outside the current viewport (`scene_set_viewport`). One vertex makes a later cull pass pointless.
- **Wiring in `src/se_scene.c`:** reset in `scene_begin` and after rasterize. Rasterize after the line pass in both `zbuf_rasterize` (~S:872) and `raycast_rasterize` (~S:1156). Add to `se_scene_geometry()`, `scene_rasterize`'s stat capture and the null guard.
- **Docs:** `docs/renderer.md` and `docs/objects.md` get a short section on points and near clipping. `CHANGELOG.md` adds both to the 2.0 entry.

## Part B: showreel structure

### B0. Timing: show clock wrapper (`main/showtime.c/.h`)
Every animation is a **pure function of scene time t**, never accumulated per-frame dt. A dropped or slow frame then just samples the choreography later, with no drift, and a fixed-step export produces exactly the same motion.
- **`double showtime_now(void)`** is the *only* place the app reads "the current time". Nothing else calls `esp_timer_get_time()` for animation (profiling keeps its own real timer).
- **`void showtime_frame(void)`** is called once per frame at frame start (`on_update`). It latches the frame's time, so every object in a frame sees the same instant.
- **Modes:**
  - `SHOWTIME_REALTIME` (default): wall clock from `esp_timer_get_time()`, minus any excluded stall.
  - `SHOWTIME_FIXED_STEP` (for a future 30 fps MJPEG render): each `showtime_frame()` advances exactly 1/fps, whatever the wall clock does. Set with `showtime_set_fixed_step(float fps)`; `showtime_set_realtime()` switches back.
- **`void showtime_set(double t)`:** jumps show time straight to a given instant. Used by the automated `RUN shots` test to render exact scene times, and naturally by a future MJPEG export.
- **`void showtime_exclude(int64_t us)`:** main.c passes the screenshot's measured stall, so a capture freezes the show instead of jumping it forward by about 1 s.
- **reel** keeps `scene_start` in show time and passes each scene `t = showtime_now() - scene_start`. A scene is done when t ≥ its duration.
- **Consequences for scenes and assets (all deterministic in t):**
  - Paths, the camera, station spin (`phase0 + ω·t`) and the turntable yaw/nod are all evaluated at t.
  - **Chase-cam "lag":** no stateful smoothing filter. The camera is placed from the player's path evaluated at `t − lag`, plus an offset.
  - **Lasers:** a scripted fire schedule. A shot is a beam lit for 0.12 s from the gun's *current* position to its target (D-24), so there is no projectile state at all.
  - **Flame flicker:** a hash of `floor(t·rate)`, interpolated between neighbouring samples, instead of a per-frame RNG step.
- The engine's own dt (and `SE_FRAME_DT_MAX` clamp) is ignored by the reel.

### B1. Files
```
main/main.c            engine glue (unchanged roles: PPA fill, profiling, P screenshot); delegates to reel
main/reel.c/.h         scene registry + playlist, enter/advance/loop; N key = next scene
main/showtime.c/.h     show clock wrapper (B0): realtime / fixed-step, stall exclusion
main/scene.h           reel_scene_t { name, duration, init(asset_dir), shutdown, enter, submit(t) } -- no dt anywhere
main/xform.c/.h        vec3; xform_t {R[3][3], t[3], scale}; from yaw/pitch/roll and from forward/up/roll;
                       look-at -> (yaw, pitch, roll) for render_set_camera_6dof; Catmull-Rom path eval + tangent;
                       smoothstep; shared back-face test (moved out of ship.c)
main/mesh.c/.h         generic static mesh: verts + tris {a,b,c, material, uv[3]}; materials = texture | flat argb
                       (+ emissive flag); mesh_submit(mesh, xform) = transform, back-face cull, scene_tri /
                       scene_textured_tri; builders: box, capped cylinder, rectangular-section torus (ring),
                       with CCW-outward winding and planar/cylindrical UVs
main/assets/           one generator per asset, each: *_init(asset_dir) / *_shutdown / *_submit(..., t) -- stateless in time
  player_ship.c/.h     current ship.c minus the turntable: plates, UVs, flames; player_ship_submit(xform, throttle)
  flame.c/.h           the Frontier cone flame (from ship.c), reusable by any ship; takes a flame texture
                       + fallback colour, so the player's is blue and the marauders' red
  marauder.c/.h        one marauder TYPE (a design distinct from the player ship; ~40-60 tris via mesh
                       builders) with per-instance livery: marauder_submit(xform, livery, t). One shared mesh +
                       UVs; the livery only swaps the hull texture. Red engine flames.
  station.c/.h         2001 wheel: hub, 8 spokes, outer ring; spin angle; geometry queries (spoke angle,
                       radii) for choreography
  starfield.c/.h       ~500 seeded directions, varied brightness/tint; submitted as scene_point at
                       camera + dir * 1000 (infinitely distant: rotation parallax only; z within depth range)
  laser.c/.h           laser_submit_beam(muzzle, target, t, t_fire, style): a red scene_line while the shot is lit
  texcache.c/.h        textures shared between assets (loaded once, by file name)
main/scenes/
  marauder_pursuit.c   opening: the marauders in formation, close up, firing (added after the plan, D-25)
  spacestation_flyby.c
  turntable.c          the original single item, unused (not in the playlist, still compiled)
  asset_viewer.c       dev: orbits each asset in turn (not in the playlist)
main/export_mjpeg.c/.h MJPEG video export, compile option SHOWREEL_EXPORT_MJPEG (added after the plan, D-26)
main/third_party/stb_image_write.h  JPEG encoder for the export (public domain)
main/objects/ship_model.h  unchanged (vendored)
```
- **`main.c`:**
  - `on_update` calls `showtime_frame()`.
  - `on_backdrop` becomes fill, `scene_begin`, `reel_submit`, prepare. The scene sets the camera first thing in submit, since vertices are camera-transformed at submit time.
  - `on_input` gains N.
  - The global light and camera setup move into scenes: each scene sets `se_light` in `enter`.
  - The perf log line gains the scene and shot name, plus the point count.
- **Build and assets:** `CMakeLists.txt` gets the new sources. New textures go in `metadata/metadata.json` (the Makefile already globs `textures/*.png`).
- **Textures:** all stay in PSRAM, per the earlier decision.

## Part T: test automation (modelled on idf6tests / fonttest)

### T1. Graceloader: export the USB-serial/JTAG driver (user decision D-16)
graceloader's `main/symbol_export/all` lacks `usb_serial_jtag_driver_install`, `usb_serial_jtag_read_bytes` and `usb_serial_jtag_write_bytes` (plus `usb_serial_jtag_driver_uninstall`). The fonttest and idf6tests listeners depend on them, and mixing that driver with stdin loses bytes.
- **Add them:**
  - Pull in the `esp_driver_usb_serial_jtag` component if it isn't linked already.
  - Add `-Wl,--undefined=` entries in `main/CMakeLists.txt` / `exported_symbols.cmake`.
  - **Use graceloader's existing Makefile targets only** (D-18): `make sync-template` (extract → rebuild → regenerate the kbelf tables and fakelib → `--check` → `update-template`). No hand-copied files.
- **Template:** `update-template` writes the new fakelib and headers into `../tanmatsu-template-grace`. Commit and push that repo (remote `nullislandspace/tanmatsu-template-grace`).
- **Showreel side:** pull those changes in by **merging from `upstream`**. The showreel's `upstream` remote is the template, so this is `git fetch upstream && git merge upstream/main`, resolving any conflicts with the showreel's own changes. No manual copying into the showreel.
- **On the device:** install graceloader with its own `make install`.
- **Proposal, needs the user's OK at that step:** graceloader has `CONFIG_ESP_SYSTEM_PANIC_PRINT_HALT=y`, so a crashing app hangs the badge. idf6tests switched to `PANIC_PRINT_REBOOT` for unattended runs. The host runner can recover either way (hard reset, like `make monitor` / `recover.py`).

### T2. Device side (showreel)
- **`main/report.c/.h`** (from `idf6tests/main/report.c`):
  - framed records `@@SR-<KIND>@@ <compact json> @@<crc32 hex8>@@`, as a single `fwrite` plus `fflush` under a mutex;
  - a table-free CRC32 that matches `zlib.crc32`;
  - never goes through `ESP_LOG`.
- **`main/debugcon.c/.h`** (from `idf6tests/main/console.c`):
  - a listener task on the USJ driver (rx 512 / tx 1024), with a 2000 ms timed read;
  - emits a `READY` banner every 2 s (app, git hash, reset reason) until a test starts;
  - answers `PING` → `PONG` inside the task;
  - other lines go to a queue that `on_update` drains non-blockingly.
- **Commands:**
  - **`RUN perf scene=<name> [secs=N]`:** plays the scene in real time from t=0. It emits one `PERF` record per second (the phase split, tri/ttri/line/point counts with their µs, SRAM free and largest block, scene and shot) and a per-shot `SHOTPERF` aggregate, then `END`.
  - **`RUN shots scene=<name> t=<t1,t2,...>`:** scenes are pure functions of t, so this renders exactly those instants (fixed step, `showtime_set`). Each one is saved as `/sd/showreel/test/<scene>_<t>.png` via `screenshot_capture` and gets a `SHOT` record (path plus an FNV-1a hash of the framebuffer, so a regression compare needs no download), then `END`.
  - **`EXIT`:** see "After a test" below.
- **After a test:** the app emits `END`, then `BYE`, flushes, waits 300 ms (so the USB FIFO drains) and calls `bsp_device_restart_to_launcher()`. There is no idle timeout: outside a test the showreel is a normal app.
- **Identity:** `APP_GIT_HASH` (from `git describe --always --dirty` in CMake) and the engine version appear in READY and PONG. The runner refuses a device whose hash doesn't match (idf6tests lesson: a failed install once tested the launcher).

### T3. Host side (showreel `tools/`)
- **`tools/testrun.py`** (structure from `idf6tests/tools/sdtest.py`; stdlib plus pyserial, run under `source $IDF_SOURCE`):
  - **Connect:** open `$(PORT)` with `do_not_open` + `open()` in a retry loop. Send `PING` every 1 s until `READY`/`PONG`: 10 s per attempt, 90 s in total, which covers the ~10 s reconnect after app start.
  - **Read loop:** `read_line()` uses `in_waiting`-sized reads and drains buffered lines first.
  - **Checks:** records are CRC-checked. The crash regex (`Guru Meditation|abort\(\) was called|Backtrace:|...`) and the stall and overall timeouts apply.
  - **After `BYE`:** it switches the launcher to BadgeLink mode automatically, downloads `/sd/showreel/test/*.png` with `badgelink fs download` into `results/<run>/`, then deletes them on the device with `fs delete`.
  - **Output:** optional reference comparison (`tools/png_diff.py`, from fonttest's `bench_png.py`, producing a 3-panel diff PNG). It writes `results/<UTC>-<test>.{log,json}` and prints a summary.
  - **Exit codes:** 0 ok, 1 link, 2 crash, 3 bad, 4 image mismatch, 5 usage.
- **`tools/recover.py`** (from idf6tests): hard reset through the rfc2217 port, then `EXIT`.
- **`tools/badgelink_retry.sh`:** copied as-is (4 retries, to get past "Invalid sync").
- **Makefile:**
  - `mode_badgelink` becomes idempotent: probe `fs list` first, and resend `BADGELINK\n` up to 5×, probing 5× each time.
  - `install` and `run` depend on `mode_badgelink`, and every badgelink call goes through the retry wrapper.
  - New targets:
    - `testrun TEST="perf scene=x"`
    - `cycle` = build → install → run → testrun
    - `recover`
    - `testrefs` / `testcompare`: capture or compare reference shots in `tests/refs/`, with the manifest committed.
  - `results/` is added to `.gitignore`.
- **What stays manual:** judging looks (composition, colours, whether a shot "reads"). Claude reviews the downloaded PNGs itself first and only asks the user for artistic judgement.

## Part C: the `spacestation_flyby` scene
**World (1 unit ≈ player wingspan):**
- Station at the origin with its axis along z, turning slowly (~0.15 rad/s).
- Hub radius 3. Ring major radius 24, cross-section about 3×2.5. Eight spokes.
- Marauders ~0.9 span.
- Light: a far "sun" (positional at ~1000 units, so effectively directional), brightness 0.75.

**Paths:** Catmull-Rom control points in world space, driven by scene time.
- **Player:** crosses the wheel plane at r ≈ 13, between two spokes.
- **Marauders:** swing outside the ring (r > 28), then converge behind the player.
- **Orientation:** forward = path tangent. Roll comes from turn rate, plus a scripted jink / barrel roll in the last shot.
- **Gap timing:** the station's initial spin phase is solved at `enter` so a gap is centred on the player's crossing point at t_cross. This is the "daring" moment and needs no collision logic.

**Shots (20 s), as built:**
1. **0–5 s, establish:** static camera behind and above the three ships (eye (−25, 40, 150)) as they fly away from it towards the turning wheel.
2. **5–9.2 s, chase:** on the player's own path, 0.25 s behind and 0.9 above it. The camera threads the same gap at t ≈ 8.5, so the spokes sweep right past the lens.
3. **9.2–12.6 s, exit:** static camera beyond the wheel (eye (14, 20, −80)) looking back: the player comes out of the gap towards the lens, green goes over the top of the ring, yellow round its left.
4. **12.6–20 s, reverse chase:** camera 0.28 s ahead of the player, offset (1.3, 0.7), looking back at it. The marauders close to ~6 units, the player barrel-rolls (15.0–16.4 s), red laser beams flash past (they miss).

Sun: far point light at (−500, 350, −60), 80%, so shots looking along and against the flight are both side-lit (F-20, 6.5).

**Marauders:** two ships of the same type, differing only in livery:
- **#1:** old, dirty **green** paint.
- **#2:** darkened **yellow**.

Both have **red** engine flames and fire **red** lasers: beams (unlit `scene_line`s) lit for 0.12 s from the gun to the target (D-24).

**New textures** (`tools/make_textures.py`, seeded, 64×64 unless noted):
- `station_hull.png`
- `station_ring.png`: panels with window rows.
- `marauder_green.png` and `marauder_yellow.png`: the same panel/grime pattern, so the two liveries match in structure, with worn, scratched and chipped paint over bare metal. Green is dirty and faded; yellow is darkened and sooty.
- `flame_red.png`: 64×8, white-hot to orange to deep red, the counterpart of `flame.png`.

The spokes reuse `plate_gunmetal.png`.

## Part C2: the `marauder_pursuit` scene (added, D-25)
The opening (6 s, first in the playlist): the two marauders in a tight formation (±0.75 units, the right one 0.6 behind) cruising along −z at 14 u/s. Each weaves sideways and up/down, banks into it and rocks its wings, with its own frequencies. The camera rides with the formation 0.90 right of, 0.42 above and 1.08 behind the right ship (pulled back 20% after F-22, so no part of the ship comes nearer than 0.63, clear of the 0.5 near plane), looking ~50° across the flight line at the pair. That is more than half the horizontal field of view (83°, so 41.6° either side of centre), so the beams, aimed at the unseen player 60 units ahead, run off the screen edge. The right ship is up to ~370 px wide.

## Part C3: MJPEG video export (added, D-26)
Compile option `SHOWREEL_EXPORT_MJPEG` (CMake `option()`, OFF by default); `make export` builds it in `build-export/`, installs and starts it:
- The show clock goes to fixed step at 30 fps from t = 0, and the reel restarts.
- Each playlist scene plays once. Every finished frame is converted from the RGB565 framebuffer to RGB888 (logical orientation) and JPEG-encoded at quality 85 (`stb_image_write`), then appended as a `00dc` chunk.
- At the end the AVI header totals and the `idx1` index are written and the app returns to the launcher.

Output: `/sd/showreel/showreel.avi`. Convert with ffmpeg: `ffmpeg -i showreel.avi -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -movflags +faststart showreel.mp4`.

## Part C4: the full reel, eleven scenes (added, D-28)

The user defined the complete sequence on 2026-09-18. Two of the scenes already exist (`marauder_pursuit` and `spacestation_flyby`); nine are new. The reel tells one story: the hero lands on an industrial planet, the marauders come for it, it escapes into space, past the station, and warps out. In another system it ambushes its pursuers from behind an asteroid, destroys one and drives off the other.

### C4.1 Running order

Durations are first estimates, to be tuned per scene; total ≈ 1:50.

| # | Scene (file / `name`) | Status | Content | ~s |
|---|---|---|---|---|
| 1 | `title` | new | 3D title "Borderworlds:" / "Superior", drifting in, canted into the screen | 7 |
| 2 | `planet_landing` | new | The hero ship lands on a planet; industrial buildings behind it, flare stacks burning | 12 |
| 3 | `marauder_approach` | new | Medium close-up: the marauders fly towards that planet | 6 |
| 4 | `pad_strafe` | new | Medium close-up: the marauders fire on the landed hero ship | 7 |
| 5 | `emergency_takeoff` | new | The hero ship takes off in a hurry and climbs between the incoming marauders, firing blue lasers | 8 |
| 6 | `marauder_pursuit` | exists | The marauders close up, firing red lasers (Part C2) | 6 |
| 7 | `spacestation_flyby` | exists | The station pass (Part C) | 20 |
| 8 | `warp_out` | new | The hero ship warps away; the marauders fly on for a few seconds, then warp too | 8 |
| 9 | `asteroid_ambush` | new | Another system: the hero comes out of warp and hides behind an asteroid; the marauders arrive and pass it; the hero comes out behind them, gives chase and fires blue lasers | 16 |
| 10 | `marauder_downfall` | new | Close-up of the marauders under blue fire; the yellow one explodes; a couple of seconds later the green one warps away | 9 |
| 11 | `hero_rolls` | new | Close-up of the hero ship flying fast, a couple of barrel rolls, then straight on | 8 |

The playlist becomes all eleven in this order. The reel still loops, and `N` still skips. The turntable and the asset viewer stay unused dev scenes.

### C4.2 Scene designs

Each scene is a pure function of scene time (B0). New scenes follow the lessons so far: `make scenecheck` (C4.4) keeps the near-plane margin on ships (F-22), and so do the cap and clearance checks; shots are framed on the host replica before the device is touched.

**1 · `title`.** Starfield only (black backdrop).
- Two lines of extruded block letters: "Borderworlds:" and "Superior" (C4.3, `title_text`). "Superior" is set in larger letters so that both lines are the same length (D-29).
- Each line lies in a plane yawed into the screen: its left end near the left screen edge (~5%), its right end further away, ending at ~70% of the screen width. Both lines start and end at the same screen x. The yaw and depth are solved on the host so the projected ends land there.
- Line 1 drifts down from above the top of the screen; line 2 drifts up from below. Both ease out (smoothstep) into place around 3 s, hold, and a slow camera dolly keeps the hold alive. Staggered by ~0.5 s so they don't mirror each other exactly.
- Textures (D-29): "Borderworlds:" wears the hero ship's plates: `plate_brushed` on the letter faces, `plate_riveted` on the sides. "Superior" wears the green marauder's livery: `marauder_green` on the faces, `plate_gunmetal` on the sides. The light comes from the upper left, so the extrusion reads as 3D.

**2 · `planet_landing`.** On the planet's surface.
- Set (shared with scenes 4 and 5, C4.3 `planet_base`): a landing pad; behind it industrial buildings (halls, tanks, pipe racks, chimneys) and two or three flare stacks with flames on top, flickering; a distant ridge line. The sky and the ground out to the horizon are solid colours painted by the PPA (C4.3 `backdrop`, D-29); only the ground around the pad (the apron) is textured geometry.
- The hero ship descends from above/behind the camera, decelerates on a path, levels out, the flames throttle down, it touches down on the pad and the engines die.
- Shots: a wide establishing shot of the base with the ship coming in; a lower, closer angle for the touchdown.

**3 · `marauder_approach`.** In space, above the planet.
- The planet as a large textured sphere in the background (C4.3 `planet`), the marauders in formation in the foreground, heading towards it.
- Medium close-up: both ships well inside the frame (unlike the pursuit's fill-the-screen framing), the camera tracking alongside and slightly ahead.

**4 · `pad_strafe`.** Back at the base.
- The marauders make a low strafing pass over the pad, firing red beams at the landed hero ship. The beams hit the ground around it: impact bursts (C4.3 `impact`). The ship is not hit.
- Medium close-up on the marauders as they come in, then the pass over the pad.

**5 · `emergency_takeoff`.** At the base.
- The hero ship lifts off hard: vertical first, flames at full throttle, nose pitching up into a steep climb.
- The marauders come round for a second pass, head-on. The hero ship climbs between them, the two passing either side, and fires blue beams as they cross (C4.3: player guns, blue laser style).
- Camera: low on the ground looking up for the lift-off, then a shot that holds the moment the three ships cross.

**6 · `marauder_pursuit`**, **7 · `spacestation_flyby`**: as built.

**8 · `warp_out`.** Space; the station small in the background.
- The hero ship flies away from the camera and warps out (C4.3 `warp`): a stretch along its flight line, then a flash, and it is gone.
- The marauders fly on for ~3 s, then warp after it, one after the other.
- Camera: behind the marauders, looking past them at the hero ship.

**9 · `asteroid_ambush`.** Another solar system: a different starfield (other seed and band), a different sun position and brightness, a banded gas giant in the distance (`planet` with another texture), a large asteroid (C4.3 `asteroid`) and a few small ones.
- The hero ship warps in (the warp effect in reverse), turns and slows into cover behind the asteroid.
- A few seconds later the marauders warp in, fly on and pass the asteroid.
- The hero ship comes out from behind it, falls in behind them and opens fire with blue beams.
- Shots: wide on the asteroid for the arrival and hiding; the marauders passing close to the camera; the hero emerging behind them.

**10 · `marauder_downfall`.** Same system.
- Framing like the pursuit (close, beside the formation), with blue beams arriving from behind and out of frame; some miss, some hit the yellow ship (impact bursts on its hull).
- The yellow marauder explodes (C4.3 `explosion`): the ship breaks into its parts, which fly apart tumbling, a fireball swells and dies, and sparks scatter.
- The green one flies on for a couple of seconds, then warps away.

**11 · `hero_rolls`.** Close-up of the hero ship flying fast.
- Space dust (C4.3 `space_dust`) streams past, so the speed shows; stars alone can't show speed.
- It flies straight, then does two barrel rolls, then flies straight on.
- Camera: close beside and slightly ahead, riding along. The ship's nearest point stays clear of the near plane.

### C4.3 New assets and shared pieces

Each goes in its own generator, reusable by any scene (D-6). Meshes live in engine-free `*_mesh.c` files, so `make meshcheck` covers them (D-20).
- **`title_text`**: 3D block letters for the glyphs needed (B d e i l o p r S s u w and ":"). **Proposal P-1:** each glyph is a set of straight, chamfered strokes on a small grid. Each stroke extrudes into a closed prism, so there is no polygon triangulation and holes (o, e, d, p, B) come for free, with a chunky retro look. `title_text_build(mesh, "Borderworlds:", height, depth)` returns the line's width for layout. Two materials per letter: face (front and back) and sides, so each line can take its own texture pair (D-29). Accepted (D-29).
- **`planet_base`**: the base set of scenes 2, 4 and 5. It includes the landing pad, industrial buildings (boxes and cylinders), flare stacks and a textured apron of ground around the pad. The apron is subdivided so near clipping and culling work on small pieces. Beyond it, the ground is the PPA-painted backdrop. It also includes a ridge line on the horizon, and a pad position/orientation for choreography.
- **Flare stack flames:** the existing `flame` asset, pointing up, with an orange style.
- **`planet`**: a UV sphere with an equirectangular texture. The earthy one for scene 3; a banded gas giant for scene 9. Kept to ≤ 24×12 segments, because the textured list holds 1024 triangles (F-3). To evaluate in the same step: a large, far planet barely changes on screen, so it could instead be drawn once into a PPA layer and blitted with a colour key under the geometry each frame (`se_ppa_blend_key`), in the same spirit as the PPA backdrop. Measure both and decide.
- **`asteroid`**: an icosphere (subdivided twice), with its vertices displaced along the radius by value noise, and a rock texture. Seeded, so small ones differ from the big one. It stays closed, so meshcheck applies.
- **`warp`**: the warp-out and warp-in effect, a pure function of time since the warp. **Proposal P-2:** over ~0.4 s the ship stretches along its flight line (×6) and thins (×0.3) while speeding away. At the end a white flash (a burst of short radial `scene_line`s and points, ~0.25 s) marks where it vanished. Warp-in runs the same thing backwards. Accepted (D-29). The stretch needs a non-uniform scale in `xform_t`, and a general 3×3 is enough (`mesh_submit` only transforms points; faces and lighting use world vertices).
- **`explosion`**: **Proposal P-3:** the marauder mesh records its parts (fuselage, wings, fins, nacelles, guns: 10 closed parts), and an exploding ship submits each part with its own velocity and tumble. A fireball goes with it: a flame-textured icosphere, emissive, swelling and shrinking with flicker. Sparks are points and short lines. No transparency needed. Accepted (D-29).
- **`impact`**: a laser hit, i.e. a burst of short orange-white lines and points for ~0.2 s, on the ground or a hull.
- **Player guns and blue lasers**: `player_ship_gun(x, side)` (muzzle positions on the vendored model, to be chosen) and `LASER_STYLE_PLAYER` (blue, e.g. `0xFF40A0FF`).
- **`space_dust`**: points in a box around the camera, wrapped around it (position modulo the box). This gives parallax and a sense of speed. Optionally drawn as short lines along the velocity, as speed streaks.
- **`backdrop`: PPA-painted sky and ground (D-29).** Ported from Stunt Racer's `main/backdrop.c` (`../tanmatsu-stuntracer-grace`). It is app code, not engine, and uses only public engine calls (`render_camera`, `render_project`, `se_ppa_fill`, `se_ppa_wait_job`, `direct_565_vrun`).
  - Two far probe points at eye height give the horizon (the ground plane's vanishing line) as a screen line.
  - The PPA fills everything above the lowest point of that line with the sky colour and everything below with the ground colour, while the geometry is prepared.
  - Before any framebuffer write, the CPU waits for the fills, then paints only the ground wedge a rolled horizon cuts out, which is empty when the camera is level. Stunt Racer's finding is kept: no CPU framebuffer work while a fill is in flight, because cache lines straddle the seam.
  - A scene declares its backdrop in `scene_def_t`: black (the current single fill, for the space scenes) or sky/ground colours (scenes 2, 4, 5). It replaces the fixed `BACKDROP_ARGB` and the single `JOB_CLEAR` fill in `main.c`.
  - Buildings and the ship stand on geometry drawn over it, so the backdrop needs no depth.
  - Accepted instead of P-4's horizon haze band; no sky dome.
- **Shared formation code:** the pursuit's formation, weave and bank maths moves from `marauder_pursuit.c` into `scenes/formation.c`, for scenes 3, 8, 10 and the pursuit itself.
- **New textures** (`make_textures.py`, own RNG so the existing PNGs stay byte-identical): `ground` (the apron), `pad` (concrete with markings), `industrial_wall`, `rock`, `planet_terran` (128×64), `planet_gas` (128×64).

### C4.4 Tooling: `make scenecheck` (host)

The lesson of F-22: the marauder wing crossed the near plane, which only showed on the device. A host build of the real scene code, linked against a stub engine, checks every scene at 30 fps before any device run:
- **Near plane:** the minimum camera-space depth of each object submitted through `mesh_submit`, with the frames where it drops below the near plane plus a margin (0.6). Some objects are allowed to cross on purpose (the spokes in the flyby chase, debris flying at the lens); scenes list those.
- **Caps:** triangle, textured triangle, line and point counts per frame, against the engine caps (F-3), with the peak per shot.
- **Clearances:** the closest approach between named objects (ships, station, asteroid, ground), so nothing flies through anything.
- **Framing:** optionally, the projected screen bounds of named objects per shot (the title layout of scene 1 uses this).

This needs a host-side stand-in for the engine header (`scene_tri`, `scene_textured_tri`, `scene_line`, `scene_point`, camera, light, textures), in `tools/host/`. That stub records; it does not render.

### C4.5 Risks and open points
- **Performance:** the planet scenes draw buildings and a textured apron; the rest of the ground and the sky are PPA fills (D-29), which cost the CPU almost nothing. The flyby's chase shot already shows that a screen full of close textured surfaces drops to 23–25 fps (F-20). Measured per scene (`perf`), decided later (D-7).
- **Textured cap:** the planet sphere, the base set and two marauders together can approach 1024 textured triangles. `scenecheck` shows the peaks. Raising `SE_SCENE_TEXTURED_TRI_CAP` would be an engine change, so ask first (D-15).
- **Engine:** nothing in this plan needs a new engine feature. If something does turn out to need one (for example a coloured light for the second solar system; `se_light_t` has brightness only), stop and ask (D-15).
- **Confirmed by the user (D-29):** P-1, P-2 and P-3 as proposed; P-4 replaced by the PPA backdrop; "Superior" larger, so both title lines are the same length.

## Part D: step-by-step plan with status tracking

**Status values:** `todo` · `in progress` · `done` · `blocked (why)` · `skipped (why)`. Each step's Notes column records its result, with links to Findings (F-n) and Decisions (D-n).

**Automated device loop (from step 3 on):** `make cycle TEST="…"` does build → BadgeLink mode → install → run → connect → RUN → collect → the app exits → BadgeLink mode → download → compare. No user action is needed; only artistic calls go to the user.

| # | Step | Status | Notes |
|---|---|---|---|
| **0** | **Tracking** | | |
| 0.1 | Create `claudeplans/implementation.md` from the approved plan | done | 2026-09-18 |
| **1** | **Graceloader: export the USJ driver (T1)** | | |
| 1.1 | Add the `usb_serial_jtag_driver_*` / `_read_bytes` / `_write_bytes` exports (component dependency + `--undefined`) | done | 2026-09-18: 8 symbols in `exported_symbols.cmake`; `esp_driver_usb_serial_jtag` added to `HEADER_COMPONENTS` in `tools/update-template.sh` (F-14) |
| 1.2 | Ask the user about panic → reboot (proposal); apply it if approved | done | User: keep HALT (D-19) |
| 1.3 | `make sync-template` in graceloader (existing targets only); its `--check` step passes; the template repo gets the new fakelib and headers | done | `make sync-template` rc 0, `--check` OK 5346; template: fakelib + 4 `driver/usb_serial_jtag*.h` |
| 1.4 | `make install` graceloader on the device; confirm the showreel (current build) still starts and runs | done | graceloader `make install`; the showreel runs normally at 30 fps (F-15: SRAM −4 KiB) |
| 1.5 | Commit and push graceloader and `tanmatsu-template-grace` (commit/push authorised by the user for this step: "will also need a commit and push") | done | graceloader `9f08def`, template `56b711c`, both pushed |
| 1.6 | Showreel: `git fetch upstream && git merge upstream/main`; resolve conflicts; build; the app.so symbol check passes; device smoke test | done | Merge `0b3dd08` (no conflicts, **not pushed yet**); `make verify`: all symbols satisfied |
| **2** | **Showreel framework (no intended visual change)** | | |
| 2.1 | `showtime.c/.h`: realtime / fixed-step / `showtime_set`, `showtime_frame`, `showtime_now`, `showtime_exclude` | done | `main/showtime.c`: realtime (wall clock minus excluded stalls; first frame = 0) / fixed step / `showtime_set` |
| 2.2 | `xform.c/.h`: vec3, xform, look-at→(yaw,pitch,roll) matching the engine basis (F-5), Catmull-Rom, smoothstep, back-face test | done | `main/xform.c` (pure) + `main/camera.c` (engine-facing look-at). Checked in meshcheck: `mat3_from_fwd_up` = `mat3_from_ypr` over 1000 random poses; `look_at_angles` inverts forward; paths hit their points; `path_vel` = finite difference |
| 2.3 | `mesh.c/.h`: mesh + materials, `mesh_submit`, builders (box, capped cylinder, rectangular torus) | done | `main/mesh.c` (pure builders; winding self-corrected from the intended outward direction) + `main/mesh_render.c` (materials passed at submit, so liveries swap) |
| 2.4 | `tools/meshcheck.c` + `make meshcheck` (host gcc): outward normals, no degenerate triangles | done | `make meshcheck`: closed / consistent / positive volume per part / no slivers; negative tests (flipped face, inside-out box) are caught. The vendored ship model is also closed and outward (8 parts) |
| 2.5 | `assets/flame.c`: out of `ship.c`, parameterised texture/colour, time-hash flicker | done | `assets/flame.c`: blue/red styles, flicker = value noise of t (25 Hz, 0.88–1.10) |
| 2.6 | `assets/player_ship.c`: plates, UVs, `player_ship_submit(xform, throttle, t)` | done | `assets/player_ship.c`: mesh normalised to wingspan 1 and centred on mid-height; plates, UVs and flames as before |
| 2.7 | `scene.h`, `reel.c/.h`, `scenes/turntable.c` (t-driven); rewire `main.c` (showtime, reel, N key, screenshot→`showtime_exclude`); delete `ship.c/.h`; CMake | done | `scene.h`, `reel.c`, `scenes/turntable.c` (R = Rx(nod)·Ry(yaw), same motion as before); `main.c` rewired (showtime, reel, N, stall excluded from the clock); `ship.c/.h` deleted; builds, `make verify` OK. Device check folded into 3.3/3.4 |
| **3** | **Test automation (T2, T3)** | | |
| 3.1 | Device: `report.c`, `debugcon.c`, command queue in `on_update`, `APP_GIT_HASH`, `RUN perf`, `RUN shots`, `EXIT` → launcher | done | `report.c`, `debugcon.c`, `devtest.c`, `app_version.h.in`; integer protocol (ms=, secs=) because graceloader exports no float parser; `make verify` OK |
| 3.2 | Host: `testrun.py`, `recover.py`, `badgelink_retry.sh`, `png_diff.py`; Makefile (`mode_badgelink` idempotent, `install`/`run` deps, `testrun`/`cycle`/`recover`/`testrefs`/`testcompare`); `.gitignore results/` | done | `tools/testrun.py`, `recover.py`, `png_diff.py` (stdlib, from fonttest: the IDF python has no Pillow), `badgelink_retry.sh`; Makefile: idempotent `mode_badgelink`, `install`/`run` depend on it, test targets |
| 3.3 | Hands-free end to end: `make cycle TEST="shots scene=turntable t=…"` runs with no user action and ends with the badge back in the launcher; also exercise recovery (the app killed mid-run) | done | Hands-free: `make run` + `make testrun TEST="shots scene=turntable ms=0,2500,5000"` → 3 shots, END, app back to the launcher, 16 s total (F-18). Hashes deterministic across app restarts |
| 3.4 | Baselines on the unchanged engine: turntable reference shots (`testrefs`) + `perf scene=turntable secs=63` → a new row in `devdocs/performance.md` | done | Refs: turntable 0/2.5/5 s hashes in `tests/refs/manifest.json`; perf 30 s: rast 11.44 ms mean, 30 fps, SRAM 152/62 KiB (`devdocs/performance.md`) |
| **4** | **Engine (synthengine3D, V2.0)** | | |
| 4.1 | Near-clip helper + `scene_tri` (unchanged all-in-front fast path; shade/packed once; 1–2 tris) | done | `clip_near()` (camera-space Sutherland–Hodgman, carries u/v), `emit_tri()`; `scene_tri` shades once and emits 1–2 tris; all-in-front path unchanged |
| 4.2 | `scene_textured_tri` clipping (UV shift from the original UVs) | done | `emit_ttri()`; UV period shift from the original corners; the shade is computed once |
| 4.3 | `scene_line` endpoint clip | done | `scene_line`: the behind endpoint is lerped onto the plane |
| 4.4 | `scene_point`: API, `se_pt_t`, `SE_SCENE_POINT_CAP`, lazy PSRAM list, submit-time cull, overflow drop, resets | done | `scene_point`, `se_pt_t`, `SE_SCENE_POINT_CAP` 1024 (PSRAM, lazy), culled at submit (near plane, viewport) |
| 4.5 | Point raster in both renderers, `se_scene_raster_points`, `se_geometry_t.pts/pt_n`, `scene_point_stats` | done | `se_scene_raster_points()` last in both renderers; `se_geometry_t.pts/pt_n`; `scene_point_stats()` |
| 4.6 | Docs: `renderer.md`, `objects.md`, `se_config.h` comment, CHANGELOG 2.0 entry | done | `renderer.md` (clipping, points, caps), `objects.md` (points/starfield), `se_config.h` comment, CHANGELOG 2.0 (branch label corrected V1.5 → V2.0) |
| 4.7 | Automated regression: `testcompare` turntable shots **bit-identical** to the 3.4 refs; perf within noise of 3.4 | done | The turntable shots are bit-identical to the 3.4 refs (`--compare`: 3× identical). Perf 20 s: rast 11.52 ms mean (11.44 before), submit 1.01 ms (0.97), 30 fps, SRAM 152/62 KiB |
| 4.8 | Automated clip test: a dev scene `scenes/test_nearclip.c` (camera sweeping through a textured box and lines) → shots reviewed for smearing, with refs captured | open (see F-23) | Judging smearing needs an image (slow download, D-21). The flyby's chase through the spokes exercises clipping in real content; one image will be checked there. **Correction (F-23):** the flyby never shows a clipped face, so 6.5 did not cover this. The only real content that visibly clipped was the old pursuit camera (F-22); the user saw faces cut away, with no smearing reported. A deliberate check is still to do: a scene that crosses the near plane in view, checked with `scenecheck -v`, plus one fetched frame |
| **5** | **Asset generators** | | |
| 5.1 | Textures: `station_hull`, `station_ring`, `marauder_green`, `marauder_yellow`, `flame_red`; contact sheet; `metadata.json` | done | 5 textures (own RNG `rng2`, so the existing 5 PNGs stay byte-identical); contact sheet checked; `metadata.json` |
| 5.2 | `assets/starfield.c` | done | 600 stars (40% in a tilted band), power-law brightness, tints; `scene_point` at eye + dir·1000 |
| 5.3 | `assets/station.c` (passes meshcheck) | done | `station_mesh.c` (pure) + `station.c`: hub, docking port, 8 spokes, ring (48 segs); 592 tris, 11 closed parts (meshcheck); the window band is centred by building the ring at z 0..depth, then shifting it |
| 5.4 | `assets/marauder.c`, one type, green/yellow liveries, red flames (passes meshcheck) | done | `marauder_mesh.c` + `marauder.c`: lofted fuselage, delta wings, fins, nacelles, guns; 220 tris, 10 closed parts; liveries swap only the paint texture; red flames; `marauder_gun()` for lasers. New `mesh_loft` builder (tested in meshcheck, including clockwise input) |
| 5.5 | `assets/laser.c` | done | First as bolts, then replaced by beams (D-24): `laser_submit_beam(muzzle, target, t, t_fire, style)`, lit 0.12 s, range 80 |
| 5.6 | `scenes/asset_viewer.c` (unused dev scene, t-driven orbit per asset); automated shots of each asset → review; tune | done | `scenes/asset_viewer.c` (`assets`, 4 named shots × 8 s, not in the playlist). Perf per asset: F-19. Two frames fetched (marauder green at 12 s, station at 28 s): geometry, livery, red flames and stars correct. Tuning noted: the marauder paint repeat is dense (graph-paper look); the ring's window band only shows from oblique views |
| **6** | **Scene `spacestation_flyby`** | | |
| 6.1 | World layout, light, station phase solved so a gap is centred at t_cross | done | Station spins 0.15 rad/s; phase solved so the gap centre sits at (0, 12.5, 0) at t_cross = 8.5 s; far sun at (-500, 300, 400), 80% |
| 6.2 | Paths (player through the gap; marauders round the ring, then converge), orientation, bank, jink | done | 13-point Catmull-Rom paths on a shared 1.7 s grid; poses face the velocity; bank ∝ lateral acceleration (0.06 rad per u/s², cap 0.9); player barrel roll 15.0–16.4 s. Host replica: clearance player↔spokes 3.44, green↔ring 6.08, yellow↔ring 7.51, chase camera 3.30 units |
| 6.3 | Shot list: 4 shots with their cameras; ~20 s | done | Final: establish 0–5 / chase 5–9.2 / exit 9.2–12.6 / reverse 12.6–20 (Part C). The establish eye came from a grid search on the host replica (the closest eye keeping all ships and the wheel ≥ 25 px inside the frame); the exit shot was re-framed after the first look (6.5) |
| 6.4 | Laser fire schedule (red bolts, near-misses) | done | Each marauder fires every 0.35 s from 12 s, guns alternating, at the player + a 1.6-unit hashed miss; beams since D-24 |
| 6.5 | Playlist = [spacestation_flyby]; automated shots at key instants (including mid-gap for near clipping) → review and tune; the user judges the final look | done | Playlist = [spacestation_flyby]. Frames checked: chase 8.3 s (the spokes sweep close past the lens; ~~near-clip check of 4.8 ✓~~ corrected by F-23: nothing crosses the near plane in view, so this checked no clipping), reverse 17 s. Fixes after the first look: sun moved to (-500, 350, -60) (the shots looking back saw only unlit faces); reverse camera closer; marauders close to ~6 units; exit shot re-framed to eye (14, 20, -80), 9.2–12.6 s. The user watched the full sequence on the badge: "looks quite OK" |
| 6.6 | `perf scene=spacestation_flyby` → per-shot numbers; caps never hit; add a flyby section to `devdocs/performance.md` | done | F-20: 30 fps except the chase approach (23–25 fps, rast up to 48 ms while the textured wheel fills the screen); caps far from full. In `devdocs/performance.md` |
| 6.7 | Opening scene `marauder_pursuit` (added, D-25) | done | Part C2. First in the playlist. 30 fps, rast 25.6 ms mean / 32.2 max (the close textured ship) |
| 6.8 | Lasers as beams (added, D-24) | done | Bolts trailed from where the gun had been (behind a fast ship); beams start at the turret by construction |
| 6.9 | MJPEG video export (added, D-26) | done | Part C3. 26 s of video in 191.6 s; 780 frames after the rounding fix (F-21) |
| 6.10 | Pursuit: the yellow ship's right wing loses faces (user report) | done | F-22: the near plane at work, not a raster bug. Camera pulled back 20% (D-27); host sweep: nearest point ≥ 0.63 over the whole scene |
| **7** | **Wrap-up** | | |
| 7.1 | README (scene system, assets, N key, test automation), final pass on the tracking doc | done | README rewritten (reel, keys, layout, tests, export); `devdocs/performance.md` has the flyby, pursuit, per-asset and export numbers; this document brought up to date |
| 7.2 | Commit and push graceloader, engine (V2.0), then the showreel with the submodule pointer. **Only when the user asks.** | done | Pushed at each milestone on request: graceloader `9f08def`, template `56b711c`, engine V2.0 `8b5897d`; the showreel up to the export commit and this documentation pass |
| **8** | **Full reel: shared infrastructure (C4.3, C4.4)** | | |
| 8.1 | User confirms P-1…P-4 and the title-line layout | done | D-29: P-1…P-3 accepted; sky/ground as PPA fills (from Stunt Racer) instead of P-4; "Superior" larger so both lines are the same length; title textures from the hero ship / green marauder |
| 8.2 | `make scenecheck`: host stand-in engine (`tools/host/`), scenes and assets compiled on the host; near-plane margin, caps, clearances, framing. Run on the two existing scenes (it must reproduce F-22 on the old pursuit camera) | done | `tools/scenecheck.c`, `tools/host/` (engine stand-in + pax/ESP header stand-ins; the real engine headers for the types). `mesh_t.name` added for object labels; `mesh_render.c` built with `-Dmesh_submit=mesh_submit_real` so the checker wraps it without `#ifdef`s. Built-in self-test (clean / near / contact / cap). All four scenes OK in ~2 s; the old pursuit camera fails as it should (F-23). `flame.c` now includes `<stddef.h>` (it relied on pax for `NULL`) |
| 8.2a | Engine: the depth scale follows `RENDER_NEAR_CLIP_Z` (D-30) | done | `SCENE_DEPTH_SCALE = 64000 × near` in `se_scene.c` (the only definition; every depth encode goes through it); comments in `se_config.h`, `docs/configuration.md` (its near-plane line was also stale: "fully behind is dropped" → clipping), CHANGELOG 2.0. Default near 0.5 → 32000.0f exactly: turntable shots bit-identical to the refs on the badge (`make testcompare`, 3× identical). Not run on the badge with a different near plane |
| 8.3 | `backdrop.c`: PPA sky/ground port from Stunt Racer; per-scene backdrop in `scene_def_t` (black or sky/ground); `on_backdrop`/`on_render` rewired; space scenes unchanged (hash-compare shots before/after) | done | `main/backdrop.c` + `main/horizon.c` (engine-free, so `scenecheck` self-tests it: 2000 random poses, upside down included; a mutation (no upside-down case) is caught). `scene_def_t` gains `camera(t)` (D-31) and `backdrop`; all four scenes split. Dev scene `horizon` (unused): level pan, full roll, pitch, posts near and 2 km out. On the badge: turntable bit-identical to the refs; three fetched frames (roll ≈30°, ≈90°, 180°) correct, far post bases on the horizon; perf F-24 |
| 8.4 | `xform_t` with a general 3×3 (non-uniform scale); meshcheck math tests | done | No struct change: everything downstream already works on transformed points, so `r` may be any linear map with a positive determinant. `mat3_stretch(m, s)` (= m·diag(s)) and `mat3_det()`. meshcheck: 200 random stretches (point, determinant, volume), a ×6-stretched cylinder stays closed and outward; a mirror is caught as inside-out |
| 8.5 | Mesh parts: builders record the triangle range of each closed part; `mesh_submit_part()` | done | Every builder call records its vertex and triangle range (`mesh_t.parts`); `mesh_part_centre()`; `mesh_submit_part()` shares the submit code with `mesh_submit` (whose output order is unchanged). meshcheck: recorded parts = the connected solids found (station 11, marauder 10), contiguous and self-contained; a lost record is caught. scenecheck wraps `mesh_submit_part` too (objects `name/pN#k`); its self-test submits a part |
| 8.6 | `scenes/formation.c`: the pursuit's formation, weave and bank maths, shared; the pursuit renders unchanged (hash-compare shots before/after) | done | Straight-line formation; slots, weave and camera offsets in the formation's own frame (exact for axis-aligned flight: the axis is normalised by division). Refs captured first for the pursuit (4 instants) and, for the first time, the flyby (4). After: flyby 4/4 identical; pursuit 3/4 identical, 3.87 s differs by FMA rounding (F-25), accepted (D-32) and re-captured. scenecheck numbers unchanged |
| 8.7 | Player guns (`player_ship_gun`) and `LASER_STYLE_PLAYER` (blue) | done | Muzzles 0.1 raw units ahead of the pods' front faces (z 0.875, x ±5.0, y 1.299 raw). `LASER_BLUE` `0xFF40A0FF`, otherwise as the marauders' style. Asset viewer fires them in the player shot; one fetched frame (6.05 s): the beam leaves the pod's tip along the flight line |
| **9** | **Full reel: new assets** | | |
| 9.1 | Textures: `ground`, `pad`, `industrial_wall`, `rock`, `planet_terran`, `planet_gas`; contact sheet; `metadata.json` | done | `rng3` generator (the ten existing PNGs byte-identical): ground, pad, industrial_wall, rock (64×64), planet_terran, planet_gas (128×64, equirectangular; the gas giant reworked to broad uneven belts after the first contact sheet). `metadata.json`; texcache 16 → 32 entries. Flare stacks reuse `flame_red` (white → orange → red) instead of a new texture |
| 9.2 | `title_text` (stroke font, extruded, face/side materials; meshcheck) | done | New builder `mesh_stroke` (mitred, extruded path; one closed solid per stroke). Glyph grid: cap 7, x-height 5; B S d e i l o p r s u w and ':'. Lines 59.5 / 36.0 units wide, so "Superior" is set 1.65× larger. meshcheck: 21 + 13 strokes closed; a folded stroke is caught. Host render of the faces checked; on the badge both lines ~320 px, 30 fps (~490 textured tris) |
| 9.3 | `planet_base` set: pad, apron, buildings, flare stacks (orange flames), ridge (meshcheck) | done | Structures (pad + yellow frame, 2 halls, 3 tanks, 2 chimneys, pipe rack, 2 flare stacks with platforms: 19 solids), apron (8×8 grid, 56 units; open surface checked facing up), ridge (36 panels at 900 units, checked facing the base). Apron unlit and the PPA ground = the apron texture's `mean_argb` (`planet_base_backdrop()`), so the colours match (F-27). **15 fps** from the viewer's orbit (F-26); apron kept as it is for now (D-33) |
| 9.4 | `planet` (UV sphere, two textures; meshcheck); measure against a PPA colour-keyed layer and decide | done | `mesh_sphere` (24×12, u per corner so the seam needs no duplicate vertex); ~165 textured tris face the camera; 19.3 ms for a 290 px disc, 30 fps. The 128×64 map shows its texels this close. PPA-layer alternative not needed at this cost; revisit if a scene fills the screen with a planet |
| 9.5 | `asteroid` (displaced icosphere, seeded; meshcheck) | done | `mesh_blob` (icosphere, subdivided twice, radius from a callback); asteroid radius = 9 seeded waves over the sphere. 4 shapes; meshcheck: those plus 50 random seeds at 0.3 lumpiness all closed. Asteroid shot: 563 textured tris (the most of any shot), 13 ms, 30 fps |
| 9.6 | `warp` (out / in; stretch, flash) | done | `warp_pose()` (stretch ×6 / thin ×0.3 via `mat3_stretch`, racing 30 spans in 0.4 s), `warp_point()`, `warp_submit_flash()` (16 radial lines + a point, 0.25 s; warping in, the burst gathers and brightens). Too small in the viewer's framing to judge; the scenes will show it |
| 9.7 | `explosion` (parts, fireball, sparks) and `impact` | done | `marauder_submit_debris()` (10 parts, each with its own outward speed and tumble, `mat3_axis_angle`); `explosion_submit()`: two lumpy blobs, faces in three heat shades (mottled fire, after the first frame showed one flat blob), 28 sparks; `impact_submit()` (in the same module). scenecheck allow-lists now take globs (the parts start out touching) |
| 9.8 | `space_dust` | done | 160 motes wrapped in a 24-unit box round the camera; dots or streaks (towards the point the camera heads for). Dust shot 30 fps |
| 9.9 | Asset viewer: shots for the new assets; perf per asset (as F-19) | done | 12 shots × 8 s (title, both planets, asteroids, warp, explosion, dust, base); per-shot backdrop (`scene_def_t.backdrop_at`). Perf and frames: F-26. Also: host builds now fail on implicit declarations (one had made "Superior" 6 px wide on the host) |
| **10** | **Full reel: new scenes** (each: host replica/scenecheck clean → device shots → review → user's look) | | |
| 10.1 | Scene 1 `title`: layout solved on the host (left end ~5%, right end ~70%), drift-in, hold | todo | |
| 10.2 | Scene 2 `planet_landing` | todo | |
| 10.3 | Scene 3 `marauder_approach` | todo | |
| 10.4 | Scene 4 `pad_strafe` | todo | |
| 10.5 | Scene 5 `emergency_takeoff` | todo | |
| 10.6 | Scene 8 `warp_out` | todo | |
| 10.7 | Scene 9 `asteroid_ambush` | todo | |
| 10.8 | Scene 10 `marauder_downfall` | todo | |
| 10.9 | Scene 11 `hero_rolls` | todo | |
| **11** | **Full reel: integration** | | |
| 11.1 | Playlist = all eleven in order (C4.1); scene-to-scene continuity (liveries, directions, where the sun is); the user watches the whole reel on the badge | todo | |
| 11.2 | `perf` per scene; the slow shots listed in `devdocs/performance.md`; performance decisions with the user (D-7), including the planet base's apron (F-26, D-33) | todo | |
| 11.3 | Reference hashes for key frames of every scene (`testrefs`) | todo | |
| 11.4 | MJPEG export of the full reel; the user checks the video | todo | |
| **12** | **Full reel: wrap-up** | | |
| 12.1 | README, performance notes, this document | todo | |
| 12.2 | Commit and push, only when the user asks | todo | |

**Standing rule (D-15):** if a step hits an engine problem, set that step to `blocked (engine: …)`, log a finding, and ask the user before doing anything else. The same applies to graceloader and launcher problems.

**Per-step "done" criteria:**
- Code builds with no new warnings (`make build`).
- clang-format has been applied.
- The step's own automated check has passed.
- The status and notes have been updated.

## Part E: findings and decisions log

### Findings (F-n), each with date and source
- **F-1** 2026-09-18, engine: there is no near-plane clipping. A primitive is dropped only if every vertex has cz < 0.5; otherwise its vertices are clamped to z = 0.5 in `scene_project_cam`, which distorts both geometry and UVs (`se_scene.c` ~S:252, S:622, S:664, S:648).
- **F-2** Depth is 16-bit 1/z with scale 32000 (`SCENE_DEPTH_SCALE`). One step is about z²/32000: 0.003 at z=10, 0.31 at z=100, 1.25 at z=200. The effective far limit is z ≈ 32000, and at encoded depth 0 triangles are not drawn. Stars at 1000 encode to 32, which still draws.
- **F-3** Caps: flat triangles 4096, lines 4096 (private defines), textured 1024 (`SE_SCENE_TEXTURED_TRI_CAP`). Overflow drops silently.
- **F-4** Lines are depth-tested (`>=`) but never write depth, and are drawn after the flat and textured passes in both renderers. Their Bresenham walk is not clipped to the viewport.
- **F-5** Camera basis is M = Ry(yaw)·Rx(pitch)·Rz(roll), with forward = (sin yaw·cos pitch, −sin pitch, cos yaw·cos pitch), so positive pitch looks down. Look-at: yaw = atan2(dx, dz), pitch = atan2(−dy, √(dx²+dz²)).
- **F-6** Lighting is computed at submit time from world-space vertices. `two_sided` uses only the eye position, so it is 6DOF-safe. The camera must be set before geometry is submitted.
- **F-7** Frustum cull is screen-space only (all vertices outside the same edge). There is no far-plane cull.
- **F-8** Textures in PSRAM vs internal SRAM: +0.38 ms (+3.8%) on the textured pass. SRAM is 163 KiB free, largest block 62 KiB, with textures in PSRAM (`devdocs/performance.md`).
- **F-9** Current turntable reference: 30 fps, `rast` 6.6–14.3 ms depending on pose, vsync slack ≥ 13 ms.
- **F-10** Deployment:
  - Install and run fail with `ConnectionResetError` while the app runs; the user exits with F1.
  - The rfc2217 console can refuse the connection right after `make run`, so it needs a retry loop.
  - pyserial needs `source $IDF_SOURCE`.

- **F-11** Test automation in fonttest and idf6tests:
  - The host runner uses pyserial; the device listener uses the USJ driver.
  - Records are framed `@@X-KIND@@ json @@crc32@@`. The host reconnects with a `PING` loop, and the app exits via `bsp_device_restart_to_launcher` after a 300 ms flush.
  - The launcher switches to BadgeLink when it receives `BADGELINK\n` on the console. That request is sometimes lost, so it is resent.
  - Reference: `idf6tests/tools/sdtest.py`, `main/console.c`, `main/report.c`; `fonttest/tools/testrun.py`, `bench_png.py`.
- **F-12** Graceloader:
  - It does **not** export `usb_serial_jtag_driver_install` / `_read_bytes`; it does export `stdin`, `read`, `fcntl` and the USJ VFS functions.
  - The console is USJ (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`).
  - Panics halt (`PANIC_PRINT_HALT`).
  - `make sync-template` regenerates the exports and the fakelib.
- **F-13** Showreel Makefile: `mode_badgelink` sends once and isn't idempotent. `install` and `run` don't depend on it, and nothing uses a retry wrapper.
- **F-14** 2026-09-18, graceloader sync: the export list grew 5327 → 5346 (+19). That is the 8 requested USJ functions, 2 deprecated `esp_vfs_usb_serial_jtag_use_*` aliases, and 9 functions the driver links in (`xRingbuffer*` / `vRingbuffer*`, `vTaskSetTimeOutState`). Nothing was removed. `--check`: OK, 5346 symbols match.
- **F-15** 2026-09-18: with the new graceloader, the showreel's internal SRAM free went 163 → **159 KiB**, largest block unchanged at 62 KiB. Presumably the larger kbelf table plus the driver and ring buffer code. Not investigated further.
- **F-16** 2026-09-18, console RX: **my first conclusion was wrong.**
  - What I saw: the host sent `PING`s from my pyserial scripts, the app answered none (0 `PONG`), and I blamed graceloader/USB.
  - The user's own test disproved that: with a temporary echo in the app's listener, bytes the user typed arrived fine (`debugcon: rx 0x31 '1'` ...). So graceloader, the exported USJ driver and the app's listener all work.
  - The fault is on the host side, in how my scripts talk to the rfc2217 proxy. Not yet isolated. Suspects:
    - they pass `baudrate=115200`, which triggers rfc2217 port-setting negotiation; the `BADGELINK` one-liner that works doesn't;
    - open/close churn from the reconnect loop;
    - the hard reset in `recover.py`, after which the proxy refused rfc2217 negotiation for a while.

- **F-17** 2026-09-18, a cycle (`make cycle`) otherwise works hands-free up to the test:
  - build;
  - a lost first `BADGELINK` request, recovered by the resend loop;
  - install and run.
  The previous app (no console) was cleared with `make recover` (hard reset → launcher). Build + mode switch + install + start took nearly the whole 300 s budget. Python stdout is block-buffered when redirected, so the runner should run with `-u`.
- **F-18** 2026-09-18, console transport, resolved:
  - Host→badge bytes over the rfc2217 proxy can arrive late and in fragments, including leftovers from an earlier session that survive a chip reset (so they are held on the host side, not in the proxy, which writes straight through).
  - Fix, the user's idea: `testrun.py` sends 64 bare newlines before each `RUN` and resends until `BEGIN`. The newlines terminate any stale half-line in the app's line parser.
  - A per-byte echo in the app deadlocked the chain: app TX blocks, the proxy spins on a tty write without reading, the runner blocks in `write`. It was removed.
  - A hard reset sometimes boots the launcher instead of the app, so `--reset` is opt-in.
  - The whole shot test takes ~16 s, hands-free.
- **F-19** 2026-09-18, per-asset cost (`perf scene=assets secs=32`, each asset alone, orbit camera):
  - player: rast 8.1 ms, 96 + 64 tris;
  - marauder: 9.8 ms, 21 + 87;
  - station at 62 units: 14.8 ms, 12 + 273 textured.
  All at 30 fps, SRAM 152/62 KiB. The point list lands in PSRAM (16 KB).
  Also added `assets/texcache.c` (textures shared between assets; the flames moved to it).
- **F-20** 2026-09-18, flyby perf (`perf scene=spacestation_flyby`, 20 s):
  | shot | fps | rast mean / max |
  |---|---|---|
  | establish | 30.0 | 4.3 / 5.0 ms |
  | chase | **27.6** | **20.7 / 48.3 ms** |
  | exit | 30.0 | 7.4 / 15.9 ms |
  | reverse | 29.9 | 7.9 / 14.4 ms |
  The chase dips to 23–25 fps for ~3 s (t 6–8.5), when the textured ring and spokes fill the screen close up: fill-bound. Caps far from full (≤ 157 tris, ≤ 533 ttris, a few lines). SRAM 152/62 KiB. Performance is to be decided later (D-7).
- **F-21** 2026-09-18, MJPEG export (`make export`): 26.0 s of video in 191.6 s. JPEG encode ~200 ms/frame at quality 85, write ~23 ms/frame, 12–30 KB/frame, in `/sd/showreel/showreel.avi`.
  - The first run produced 781 frames instead of 780: the fixed-step show time accumulates to 5.99999 s, not 6.0 s. Fixed with a 1e-6 tolerance in `reel_frame`.
  - Engine: `audio_mixer_shutdown()` after an idle power-down logs `E i2s_common: i2s_channel_disable … not enabled yet` (harmless). Reported to the user, not worked around (D-15).

- **F-22** 2026-09-18, user report: in `marauder_pursuit` the yellow marauder's right wing lost faces, sometimes only partly, as it manoeuvred. Host replica of the scene (camera-space depth of every vertex, 30 fps): the right wingtip and the back of its fin came as near as 0.39 in camera-space z, inside the engine's near plane (`RENDER_NEAR_CLIP_Z` 0.5). In 67 of 180 frames visible faces were clipped, in 45 some were dropped entirely (t ≈ 1.5 s and 3.3–4.4 s). The clipper was working correctly. Side finding (engine; fixed on the user's request, D-30): `RENDER_NEAR_CLIP_Z` is `#ifndef`-overridable, but `SCENE_DEPTH_SCALE` (32000, `se_scene.c`) is hard-coded for near = 0.5. A smaller near plane would overflow the 16-bit depth (1/z × 32000 > 65535 below z ≈ 0.49) and wrap.

- **F-23** 2026-09-18, `make scenecheck`, first run:
  - **Pursuit:** the yellow ship's nearest visible point is 0.634 at 3.83 s (clear). With the old camera it is 0.389 at 3.87 s, visibly clipped in 50 frames (1.07–1.77 s and 3.27–4.17 s), so the check reproduces F-22. F-22's 67 frames also counted faces cut outside the view.
  - **Flyby:** no mesh comes nearer than 4.2 in view; the station's closest is 4.20 at 8.43 s (chase). The spokes cross the near plane only outside the frame, so 6.5's "spokes clip cleanly past the lens" was not what happened, and the flyby needs no near-plane exception. Clearances: player to station 3.49, marauders to station ≥ 7.18, the marauders to each other 2.99, player to marauders ≥ 5.08. List peaks per frame: 157 flat / 537 textured / 2 lines / 54 points (the reverse and exit shots), in line with the badge's own counts (F-20).
  - **Pursuit formation:** the two marauders pass within 0.43 of each other at 5.47 s.
  - **Documentation fix:** the horizontal field of view is 83° (±41.6°: focal length 450 on 800 px). The "42° FOV" in C2 and D-25 was the half-angle; the text is corrected.

- **F-24** 2026-09-18, sky/ground backdrop on the badge (`perf scene=horizon`):
  - 30 fps throughout.
  - `wait` is 2.5 ms level (two fills), 4–7.4 ms while rolling. The PPA fills whole rows only (`se_ppa_fill(y_top, h)`), so a tilted horizon's wedge falls to the CPU: up to half the screen near 90° of roll, ~5 ms.
  - If a planet scene rolls its camera steeply for long, the options are a rectangle fill in the engine's PPA helper (an engine change: ask, D-15) or splitting the wedge into row bands.
  - Also: Stunt Racer's backdrop assumed the ground below the line; the port detects upside down from the camera's up axis (cos pitch · cos roll).

- **F-25** 2026-09-18, badge vs host arithmetic. The badge's GCC fuses `a*b + c` into one FPU multiply-add (`-ffp-contract=fast`, GNU C's default; the P4's FPU has `fmadd.s`); the host build (x86-64, no `-mfma`) does not.
  - So code that is bit-identical on the host can differ in the last bit on the badge. The pursuit refactor (8.6) computes the centre and then adds the slot, where the old code did it in one expression, so it rounds once more. With `-mfma -ffp-contract=fast` on the host, 144 of 184 instants differ by 1 ulp in a coordinate (≤ ~8e-6 units, ≤ ~0.004 px). On the badge one of four reference frames changed.
  - No visible or timing effect, and still deterministic per build. Consequences: host replicas agree with the badge only to rounding (irrelevant to scenecheck's margins), and exact frame hashes flag last-bit changes like real regressions.

- **F-26** 2026-09-18, asset viewer on the badge (`perf scene=assets`, 96 s): every shot holds 30 fps except the planet base.
  - Rast per shot (ms): player 8.2, marauder 9.9, station 14.8, title 6.9, planet 19.3, asteroids 13.1, warp 0.4, explosion 3.2 (max 24), dust 3.6, **base 60**.
  - The base's cost is pixels, not triangles (~290 textured): from the viewer's orbit the textured apron covers ~800×250 px and the buildings more. Textured fill runs at roughly 5 Mpx/s (the planet: 19 ms for ~66k px), so any screen-sized textured layer costs ~50 ms. The PPA only saves what it paints, so the textured apron must stay small on screen.
  - Options: shrink the apron to the pad's surroundings; or accept a lower frame rate in the planet scenes. The user keeps the apron as it is for now (D-33).
  - SRAM unchanged: 152 / 62 KiB.
- **F-27** 2026-09-18: the lit apron (61, 51, 32) did not match the unlit PPA ground (99, 81, 57): a hard seam. Fixed by drawing the apron unlit (a flat plane takes one shade anyway) and taking the PPA ground from the texture's `mean_argb`: now (110, 90, 61) against (107, 89, 57). What remains visible is texture against flat colour.

### Decisions (D-n), each with date and who decided
- **D-1** User: "in the spirit of" Frontier II, with our own sequence and models, textured.
- **D-2** User: the player ship is the vendored synthracer ship.
- **D-3** User: no transparency for now.
- **D-4** User: stars are the new engine primitive `scene_point`.
- **D-5** User: all scene setup and scripting is in the app; the engine only renders the render list.
- **D-6** User: separate scene files, and separate reusable asset generators.
- **D-7** User: performance is evaluated after the fact.
- **D-8** User: near-plane clipping goes in the engine.
- **D-9** User: this is still unreleased 2.0 work on branch V2.0, with no version bump.
- **D-10** User: the turntable is kept as an unused scene.
- **D-11** User: animation is time-based through a clock wrapper, and fixed-step mode is prepared for a future 30 fps MJPEG render.
- **D-12** User: two marauders of one type (not the player's design), one in dirty green and one in darkened yellow, with red flames and red lasers drawn as lines.
- **D-13** Earlier, user: textures load into PSRAM for now; per-texture placement stays in the engine API.
- **D-15** User, standing rule: this is our own engine. **When an engine problem, limitation or bug shows up, stop and ask the user what to do.** Do not work around it in the app or the engine. Record the question and the answer here.
- **D-16** User: export the USJ driver from graceloader rather than using non-blocking stdin.
- **D-17** User: test automation must be hands-free. The app waits for a debug command, runs the test, then exits to the launcher; mode switches are automated.
- **D-18** User: graceloader changes go only through its existing Makefile targets (`sync-template`, which exports to `tanmatsu-template-grace`). The template gets its own commit and push, and the showreel takes the changes by merging from `upstream` (the template).
- **D-14** Claude; the user watched the result and raised none of these, so they stand:
  - Points are 1 px, unlit, depth-tested, with a lazy PSRAM list.
  - Stars are placed at camera + dir·1000.
  - 1 world unit is about the player's wingspan; the station ring has radius 24, with 8 spokes.
  - The chase cam uses path lag instead of a stateful filter.
- **D-19** 2026-09-18, user: keep graceloader's `PANIC_PRINT_HALT`. The test runner recovers a hung badge with a hard reset over rfc2217 (`recover.py`).
- **D-20** 2026-09-18, Claude: the engine-free code (`xform.c`, `mesh.c`, `*_mesh.c`) is kept separate from engine-facing code (`camera.c`, `mesh_render.c`, asset submit) so `make meshcheck` can compile it on the host.
- **D-21** 2026-09-18, user: no routine image downloads (BadgeLink needs well over a minute per 1.1 MB PNG). Shot tests compare framebuffer hashes reported over the console. Images only with `--fetch`, or by hand: `badgelink/tools/badgelink.sh --tcp $BADGELINKPORT fs download /sd/showreel/test/<scene>_<ms>.png out.png`.
- **D-22** 2026-09-18, user: test scripts are time-boxed; move on to the app. Tight timeouts: connect ≤ 12 s, stall 10 s.
- **D-23** 2026-09-18, user: flyby v1 accepted ("looks quite OK"). Next: a separate opening scene. The camera is close behind the marauders and right of the right marauder's centreline; they fly in formation with slight wing wiggles, firing at the player (out of frame). It establishes that the marauders won't give up.
- **D-24** 2026-09-18, user: lasers are beams, not bolts. A shot lights up from the gun's current position to its target for a fraction of a second (0.12 s); nothing travels. This replaced the bolt model, whose streaks started behind fast ships.
- **D-25** 2026-09-18, user: new opening scene `marauder_pursuit` (6 s, before the flyby): the marauders fill much of the screen from close behind and right of the right ship; the beams run off the screen edge (camera ~50° off the flight line, beyond the 41.6° half-angle of the horizontal field of view).
- **D-26** 2026-09-18, user: MJPEG video export as a compile option. Software JPEG encoder (stb_image_write, vendored) rather than exporting the P4's hardware encoder from graceloader; AVI container.

- **D-27** 2026-09-18, user: fix F-22 in the scene, not the engine. The pursuit camera moved back 20% along its offset, to (−0.90, 0.42, 1.08) from the right ship's slot. The engine's near plane stays at 0.5.
- **D-28** 2026-09-18, user: the full reel is eleven scenes (Part C4): title "Borderworlds:" / "Superior" (3D, drifting in, canted into the screen: left end near the left edge, right end further away at ~70% of the width); hero lands at an industrial planet base with flare stacks; marauders approach the planet; marauders fire on the landed hero; emergency take-off between the incoming marauders firing blue lasers; the existing pursuit; the existing station flyby; hero warps out, the marauders follow a few seconds later; another system: the hero warps in, hides behind an asteroid, the marauders pass, the hero pursues and fires; close-up of the marauders under blue fire, yellow explodes, green warps away; close-up of the hero flying fast with barrel rolls. Claude's proposals P-1…P-4 (C4.3) went to the user for an OK (step 8.1; answered in D-29).
- **D-29** 2026-09-18, user:
  - The stroke-built block letters (P-1), the stretch-and-flash warp (P-2) and the parts-fireball-sparks explosion (P-3) are accepted.
  - Title textures: "Borderworlds:" in the hero ship's textures, "Superior" in the green marauder's.
  - "Superior" is set larger so both lines are the same length.
  - Solid sky is fine, and large areas should be painted with the PPA, as Stunt Racer does: sky and ground fills with a CPU wedge for a rolled horizon (C4.3 `backdrop`).
- **D-30** 2026-09-18, user: fix the engine depth-scale bug (F-22 side finding): the scale is derived from the near plane. Also, user: the marauders flying very close to each other in the pursuit (0.43 apart, F-23) is by design.
- **D-31** 2026-09-18, Claude: scenes set their camera in a separate `camera(t)` callback, called before the backdrop is queued. The sky/ground backdrop needs the horizon, and so the camera, before the PPA starts. Keeping the fill ahead of `submit()` keeps it overlapped with the geometry work (Stunt Racer sets its camera in `on_update` for the same reason). Since scenes are pure functions of t, the split costs nothing.
- **D-32** 2026-09-18, user: ignore last-bit discrepancies like F-25 ("We are making a game engine here, not a scientific paper"). When a deliberate refactor changes a reference hash only through rounding, re-capture the reference with a reason; no bit-exactness gymnastics.
- **D-33** 2026-09-18, user: keep the planet base's apron as it is (56×56 units, textured) for now; the fill-rate question (F-26) is decided later, with the planet scenes' real framing (step 11.2).

## Verification (summary)
Automated wherever possible, via `make cycle`:
- **Engine:** turntable shots bit-identical to the pre-change refs (the fast path), plus a near-clip test scene.
- **Performance:** perf records per scene and shot.
- **Meshes:** meshcheck on the host.
- **Scenes:** shots at key instants, downloaded and reviewed by Claude.

The user is only asked for artistic judgement, and for any engine or loader problem (D-15).

## Critical files
- **Graceloader:** `main/CMakeLists.txt` / `exported_symbols.cmake`, then outputs regenerated by `make sync-template`, possibly `sdkconfig_tanmatsu` (panic).
- **Template (`../tanmatsu-template-grace`):** updated only by `make sync-template`; committed and pushed; merged into the showreel from `upstream`.
- **Engine:** `src/se_scene.c`, `include/se_scene.h`, `include/se_config.h`, `docs/renderer.md`, `docs/objects.md`, `CHANGELOG.md`.
- **Showreel:**
  - `main/main.c` and `main/ship.c`; `ship.c` is split into `assets/player_ship.c` and `assets/flame.c`.
  - New: `showtime`, `reel`, `scene.h`, `xform`, `camera`, `mesh`, `mesh_render`, `report`, `debugcon`, `devtest`, `assets/*` (incl. `texcache`), `scenes/*` (`marauder_pursuit`, `spacestation_flyby`, `turntable`, `asset_viewer`), `export_mjpeg` + `third_party/stb_image_write.h`.
  - `fakelib/liball.so` and headers, arriving via the `upstream` merge.
  - `tools/`: `testrun.py`, `recover.py`, `badgelink_retry.sh`, `png_diff.py`, `meshcheck.c`, `make_textures.py`.
  - `Makefile`, `CMakeLists.txt`, `metadata/metadata.json`, `tests/refs/`, `README.md`, `devdocs/performance.md`, `claudeplans/implementation.md`.
