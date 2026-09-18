# Implementation plan: scene system + "spacestation_flyby"

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
  - **Lasers:** a scripted fire schedule. A bolt's position is `muzzle(t_fire) + dir·speed·(t − t_fire)`, so there is no bolt pool state to integrate.
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
  laser.c/.h           laser_submit_bolt(muzzle, dir, speed, t_since_fire): one emissive red scene_line
main/scenes/
  turntable.c          current main-screen behaviour, unused (not in playlist, still compiled)
  spacestation_flyby.c
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

**Shots (~20 s, then loop):**
1. **0–5 s, establish:** a wide static camera looks at the turning station. The three ships approach from afar, flames lit.
2. **5–10 s, chase cam:** smoothed offset behind and above the player. Spokes sweep past as it threads the gap (t ≈ 8.5).
3. **10–14 s, exit side:** the camera looks back at the wheel. The player bursts through, and the marauders arc round the ring's rim.
4. **14–20 s, reverse chase:** the camera is ahead of the player, looking back. The player jinks in the foreground, the marauders close in behind, and red bolts streak past (they miss).

**Marauders:** two ships of the same type, differing only in livery:
- **#1:** old, dirty **green** paint.
- **#2:** darkened **yellow**.

Both have **red** engine flames and fire **red** lasers, drawn as emissive `scene_line` bolts.

**New textures** (`tools/make_textures.py`, seeded, 64×64 unless noted):
- `station_hull.png`
- `station_ring.png`: panels with window rows.
- `marauder_green.png` and `marauder_yellow.png`: the same panel/grime pattern, so the two liveries match in structure, with worn, scratched and chipped paint over bare metal. Green is dirty and faded; yellow is darkened and sooty.
- `flame_red.png`: 64×8, white-hot to orange to deep red, the counterpart of `flame.png`.

The spokes reuse `plate_gunmetal.png`.

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
| 4.8 | Automated clip test: a dev scene `scenes/test_nearclip.c` (camera sweeping through a textured box and lines) → shots reviewed for smearing, with refs captured | skipped (folded into 6.5) | Judging smearing needs an image (slow download, D-21). The flyby's chase through the spokes exercises clipping in real content; one image will be checked there |
| **5** | **Asset generators** | | |
| 5.1 | Textures: `station_hull`, `station_ring`, `marauder_green`, `marauder_yellow`, `flame_red`; contact sheet; `metadata.json` | done | 5 textures (own RNG `rng2`, so the existing 5 PNGs stay byte-identical); contact sheet checked; `metadata.json` |
| 5.2 | `assets/starfield.c` | done | 600 stars (40% in a tilted band), power-law brightness, tints; `scene_point` at eye + dir·1000 |
| 5.3 | `assets/station.c` (passes meshcheck) | done | `station_mesh.c` (pure) + `station.c`: hub, docking port, 8 spokes, ring (48 segs); 592 tris, 11 closed parts (meshcheck); the window band is centred by building the ring at z 0..depth, then shifting it |
| 5.4 | `assets/marauder.c`, one type, green/yellow liveries, red flames (passes meshcheck) | done | `marauder_mesh.c` + `marauder.c`: lofted fuselage, delta wings, fins, nacelles, guns; 220 tris, 10 closed parts; liveries swap only the paint texture; red flames; `marauder_gun()` for lasers. New `mesh_loft` builder (tested in meshcheck, including clockwise input) |
| 5.5 | `assets/laser.c` | done | `laser_submit_bolt(muzzle, dir, age, style)`: a pure function of age (speed 60, length 2, lifetime 1.2 s) |
| 5.6 | `scenes/asset_viewer.c` (unused dev scene, t-driven orbit per asset); automated shots of each asset → review; tune | done | `scenes/asset_viewer.c` (`assets`, 4 named shots × 8 s, not in the playlist). Perf per asset: F-19. Two frames fetched (marauder green at 12 s, station at 28 s): geometry, livery, red flames and stars correct. Tuning noted: the marauder paint repeat is dense (graph-paper look); the ring's window band only shows from oblique views |
| **6** | **Scene `spacestation_flyby`** | | |
| 6.1 | World layout, light, station phase solved so a gap is centred at t_cross | done | Station spins 0.15 rad/s; phase solved so the gap centre sits at (0, 12.5, 0) at t_cross = 8.5 s; far sun at (-500, 300, 400), 80% |
| 6.2 | Paths (player through the gap; marauders round the ring, then converge), orientation, bank, jink | done | 13-point Catmull-Rom paths on a shared 1.7 s grid; poses face the velocity; bank ∝ lateral acceleration (0.06 rad per u/s², cap 0.9); player barrel roll 15.0–16.4 s. Host replica: clearance player↔spokes 3.44, green↔ring 6.08, yellow↔ring 7.51, chase camera 3.30 units |
| 6.3 | Shot list: 4 shots with their cameras; ~20 s | done | establish 0–5 / chase 5–10 / exit 10–14 / reverse 14–20. The establish and exit eyes were chosen by a grid search on the host replica: the closest eye keeping all ships (and, for establish, the wheel) ≥ 25 px inside the frame |
| 6.4 | Laser fire schedule (red bolts, near-misses) | done | Each marauder fires every 0.35 s from 12 s, guns alternating, aimed at the player 0.25 s ahead + a 1.6-unit hashed miss |
| 6.5 | Playlist = [spacestation_flyby]; automated shots at key instants (including mid-gap for near clipping) → review and tune; the user judges the final look | done | Playlist = [spacestation_flyby]. Frames checked: chase 8.3 s (the spokes clip cleanly past the lens: the near-clip check of 4.8 ✓), reverse 17 s. Fixes after the first look: sun moved to (-500, 350, -60) (the shots looking back saw only unlit faces); reverse camera closer; marauders close to ~6 units; exit shot re-framed to eye (14, 20, -80), 9.2–12.6 s. The user watched the full sequence on the badge: "looks quite OK" |
| 6.6 | `perf scene=spacestation_flyby` → per-shot numbers; caps never hit; add a flyby section to `devdocs/performance.md` | todo | |
| **7** | **Wrap-up** | | |
| 7.1 | README (scene system, assets, N key, test automation), final pass on the tracking doc | todo | |
| 7.2 | Commit and push graceloader, engine (V2.0), then the showreel with the submodule pointer. **Only when the user asks.** | todo | |

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
- **D-14** Claude, pending review:
  - Points are 1 px, unlit, depth-tested, with a lazy PSRAM list.
  - Stars are placed at camera + dir·1000.
  - 1 world unit is about the player's wingspan; the station ring has radius 24, with 8 spokes.
  - The chase cam uses path lag instead of a stateful filter.
- **D-19** 2026-09-18, user: keep graceloader's `PANIC_PRINT_HALT`. The test runner recovers a hung badge with a hard reset over rfc2217 (`recover.py`).
- **D-20** 2026-09-18, Claude: the engine-free code (`xform.c`, `mesh.c`, `*_mesh.c`) is kept separate from engine-facing code (`camera.c`, `mesh_render.c`, asset submit) so `make meshcheck` can compile it on the host.
- **D-21** 2026-09-18, user: no routine image downloads (BadgeLink needs well over a minute per 1.1 MB PNG). Shot tests compare framebuffer hashes reported over the console. Images only with `--fetch`, or by hand: `badgelink/tools/badgelink.sh --tcp $BADGELINKPORT fs download /sd/showreel/test/<scene>_<ms>.png out.png`.
- **D-22** 2026-09-18, user: test scripts are time-boxed; move on to the app. Tight timeouts: connect ≤ 12 s, stall 10 s.
- **D-23** 2026-09-18, user: flyby v1 accepted ("looks quite OK"). Next: a separate opening scene. The camera is close behind the marauders and right of the right marauder's centreline; they fly in formation with slight wing wiggles, firing at the player (out of frame). It establishes that the marauders won't give up.

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
  - New: `showtime`, `reel`, `scene.h`, `xform`, `mesh`, `report`, `debugcon`, `assets/*`, `scenes/*`.
  - `fakelib/liball.so` and headers, arriving via the `upstream` merge.
  - `tools/`: `testrun.py`, `recover.py`, `badgelink_retry.sh`, `png_diff.py`, `meshcheck.c`, `make_textures.py`.
  - `Makefile`, `CMakeLists.txt`, `metadata/metadata.json`, `tests/refs/`, `README.md`, `devdocs/performance.md`, `claudeplans/implementation.md`.
