# Implementation plan: CraftMiner (branch `craftminer`)

Living document: design, step-by-step status, findings and decisions.
Updated whenever a step starts or finishes, something is measured, or
something is decided. Plan approved by the user on 2026-09-19.


## Context
The eleven space scenes are complete (branch `main`). On the new branch `craftminer`, the showreel gets a second act:
- a simplified, Minecraft-like block world;
- a person walks around, mines blocks and builds a small log cabin;
- the title reads **"CraftMiner"**.

Everything is our own: generated textures, our own character, no mobs or other recognisable designs.

**User decisions (to log as D-40 onwards):**
- Cameras: a mix of first person and third person.
- A live frame rate below 30 fps is fine. The MJPEG export is fixed-step, so the video stays smooth anyway.
- The scenes will eventually be **appended** to the space reel. For now the space scenes are only **commented out of `PLAYLIST`**; they stay in `ALL_SCENES` and remain testable.
- `SE_SCENE_TEXTURED_TRI_CAP` may be raised (e.g. to 2048) through the app build, but only if a measured view needs it.

**The known constraint (devdocs/performance.md):** textured fill runs at about 5 Mpx/s, so a screen full of textured blocks costs about 50 ms. Flat fill is 3–4× cheaper, and the PPA backdrop is free. The design therefore:
- textures only what is near the camera;
- draws distant blocks in flat, fog-tinted colours;
- lets the sky and the backdrop ground carry the far view.

## About this document
This file (`claudeplans/craftminer.md`) is committed with the work. It contains:
- the design (Parts R, V, A, S, T below),
- the step-by-step plan with a status table (Part D),
- the findings log and the decisions log (Part E).

Numbering continues from the space reel's `claudeplans/implementation.md`: findings from **F-31**, decisions from **D-40**. The standing rules from there still apply:
- **D-15:** engine, graceloader or launcher problems → stop and ask; no workarounds.
- Tight timeouts.
- No routine BadgeLink downloads (`--fetch` only when looks must be judged).
- Commit and push only when the user asks.

## Part R: one subdirectory per reel segment (D-47)
Each segment of the reel (the space act, CraftMiner, any later one) lives in its own subdirectory: code, assets, scenes and textures alike. Nothing can then be mixed between segments by accident. What more than one segment uses sits in shared directories and is listed there on purpose.

**Source layout:**
```
main/                     core: main, reel, showtime, devtest/debugcon/report, export, screenshot,
                          profile, scene.h, mesh + mesh_render, xform, camera, backdrop, horizon
main/common/              shared code: texcache now; flame and starfield once CraftMiner uses them (step 2.2)
main/dev/                 core dev scenes: horizon_test (the backdrop)
main/space/               the space act: space.h (its scene declarations), objects/ (the vendored ship)
  assets/                 player_ship, marauder, station, planet, planet_base, asteroid, laser,
                          explosion, warp, space_dust, title_text (+ their *_mesh.c)
  scenes/                 the eleven scenes, turntable, asset_viewer, flight, formation, system2
main/craftminer/          CraftMiner: craftminer.h (its scene declarations)
  voxel/                  world, mesher, render, fx, sky (Part V)
  assets/                 miner, pickaxe (Part A)
  scenes/                 cm_* (Part S)
textures/space/*.png      the 16 existing PNGs, moved with git mv (byte-identical)
textures/craftminer/*.png the block and face textures
```
- **Only what is really shared goes to `common/`** (done during the move, F-35): `texcache` moved there at once. `flame` hard-codes the space act's two flame textures and `starfield` is used only by space scenes so far, so both stay in `space/assets/` until CraftMiner uses them (step 2.2). At that point they move, and `flame` takes its texture as a parameter.
- **Includes** name the directory: `"space/assets/marauder.h"`, `"common/flame.h"`, `"craftminer/voxel/voxel_world.h"`. `main/scenes/scenes.h` is split into `space/space.h`, `craftminer/craftminer.h` and `dev/dev.h`. `reel.c` includes all three: it is the one place that knows every segment.
- **Textures:** each segment's textures go to a subdirectory on the badge too: `<app>/space/rock.png`, `<app>/craftminer/grass_top.png`. Code asks for them by that path (`texcache_get("space/rock.png")`).
  - `metadata.json` uses `"source_file": "space/rock.png", "target_file": "space/rock.png"`. The launcher creates target subdirectories (`fs_utils_mkdir_recursive`, `tanmatsu-launcher/main/app_management.c:217`), and other apps in the app repository already ship assets in subdirectories.
  - `make install` creates the subdirectories and uploads into them. `make apprepo` copies with the subdirectories. The old flat copies on the badge are deleted once.
- **`tools/make_textures.py`** writes each group into its segment's directory. The space groups keep their seeds, so their PNGs stay byte-identical.
- **Enforced, not just a convention:** a new `tools/segcheck.py` runs as part of `make build` and fails the build when:
  - a file in `main/<segment>/` includes a header from another segment;
  - core (`main/*.c`, `main/common/`, `main/dev/`) includes any segment header, except `reel.c`;
  - a segment names a texture outside its own directory (`"<segment>/…png"` string literals).
- **Host tools:** the scenecheck and meshcheck source lists and includes move to the new paths. `tests/refs/` is unchanged, since shot names come from scene names.
- **The move changes no behaviour:**
  - Before it, `make testcompare` runs over every reference (turntable, marauder_pursuit, spacestation_flyby). Any reference made stale by the D-39 fixes is re-captured with a reason.
  - After it, the same comparisons must give **identical** hashes. The `make scenecheck` report must be unchanged, and `make meshcheck` must pass.

## Part X: cut-out transparency in the engine (D-48)
The user asked for cut-out transparency: a texel is either drawn or a hole. It reverses D-3 for that kind; blended transparency stays out. The work is engine work on branch V2.0, still unreleased: no version bump, and it goes into the 2.0 CHANGELOG entry.

- **Texture loader (`se_texture.c`):** the PNG's alpha is no longer discarded. A texel with alpha < 128 becomes the reserved key `SE_TEXEL_CUTOUT` (0xF81F, RGB565 magenta). An opaque texel that happens to equal the key is nudged by one blue step. `se_texture_t` gains `bool cutout`, true when the texture has any hole. `mean_argb` averages only the opaque texels.
- **Textured rasterizer (`se_scene.c`):** a sibling of `scene_vrun_tex` for cutout textures. It fetches the texel first, and a hole writes neither colour nor depth. Opaque textures keep the existing loop untouched, so their output stays bit-identical. The raycast renderer draws textured triangles through the same pass, so both built-in renderers get it; custom renderers see `tex->cutout`.
- **Docs:** `se_texture.h`, `docs/renderer.md` and `README.md` stop saying "alpha discarded", and the CHANGELOG 2.0 entry gets the addition.
- **Checked by:** every reference shot identical (opaque path unchanged), and device shots of cutout leaves in the CraftMiner world test scene.
- **CraftMiner uses it for** leaves with gaps (near trees get their inner faces too), flowers and grass tufts as crossed quads, torches, and glass in the cabin windows.

## Part Q: quarter-resolution rendering in the engine (D-49)
The block world is fill-bound at 800×480 (F-37). The user asked for rendering every other pixel of every other line into a half-size buffer, scaled back up by the PPA, switchable per scene. Engine work on V2.0 (unreleased, 2.0 CHANGELOG entry).

- **Engine (`se_scene.c`):** `scene_set_render_scale(1|2)`, latched by `scene_begin()`. The projection's output is halved (one place, `scene_project_cam`), so target pixel (i, j) is what full resolution draws at (2i, 2j); the camera, the `RENDER_*` projection, the viewport (kept in full-screen pixels, halved per frame), culling, clipping and lighting are unchanged. The rasterizers address the target through one helper with the frame's stride. The raycast renderer falls back to the z-buffer at quarter resolution. `se_geometry_t.scale`.
- **Engine (`se_ppa.c`):** `se_ppa_blit_scaled` (the 2× upscale), `se_ppa_layer_sync` (write back and invalidate a buffer the CPU draws and the PPA fills and reads), `se_ppa_buf_invalidate` (before the CPU reads what the PPA wrote).
- **App:** `scene_def_t.quarter`; `main.c` renders backdrop and scene into a half-size PPA layer, syncs it, has the PPA scale it 2× onto the screen and invalidates the screen for screenshots and the encoder; `backdrop.c` works in either buffer (the horizon scaled).
- **The PPA's scaler interpolates** (F-39): soft pixels, not blocks. A CPU nearest-neighbour doubling was the alternative; the user looked at the soft result on the badge and kept it ("looks fine"; the PPA costs the CPU nothing).

## Part V: the voxel world (`main/craftminer/voxel/`)

**`voxel_world.c/.h`: the block grid**
- A `uint8_t` block id per cell, 128 × 32 × 128 (x, y, z) = 512 KB in PSRAM.
- It is generated deterministically at `init`:
  - a heightmap from `value_noise` / `hash01` (`main/xform.h`);
  - a stone layer under dirt under grass;
  - sand at water level, and a lake of opaque water;
  - coal ore in the stone;
  - trees (log trunk plus a leaf blob) placed by hash.
- Hand-shaped places are stamped in on top: a meadow, a stone cliff face for mining, and a flat cabin plot.
- `voxel_height(x, z)` gives the ground height, for feet and cameras.

**Block types, 16×16 textures**, nearest-texel sampling for the chunky look:

| Block | Faces |
|---|---|
| grass | top / side / bottom = dirt |
| dirt | all faces |
| stone | all faces |
| cobblestone | all faces |
| sand | all faces |
| water | all faces |
| log | side / top |
| planks | all faces |
| leaves | opaque, like Minecraft's "fast graphics" |
| coal ore | all faces |

That is 12 textures, plus the character's face: about 13 new PNGs. The texture cache now has 16 of 32 slots in use, so raise `TEXCACHE_MAX` in `main/common/texcache.c` to 48 for headroom. That is an app constant, not engine code.

**Edits over time, which keeps the reel a pure function of t:**
- A scene supplies a time-sorted edit list, each edit being (t, x, y, z, new block): the blocks mined, the cabin placed.
- The world at time t is the generated base plus the edits with time ≤ t.

**`voxel_mesh.c`: the mesher** (engine-free, so meshcheck can test it)
- It meshes 16×16-column chunks into `mesh_t` (`main/mesh.h`, using `mesh_vert` / `mesh_quad`).
- Only faces that border air or water are kept.
- Coplanar faces of the same material merge into rectangles (greedy meshing). The UVs repeat, so one texture tiles across a whole rectangle.
- Grass sides merge only horizontally, so the green strip stays one block tall.
- Winding is CCW-outward, so `mesh_submit`'s back-face cull works.

**`voxel_render.c`: chunk submission**
- Chunks are submitted with `mesh_submit(chunk, &identity, mats, n)`. That keeps scenecheck's per-object checks.
- **Level of detail per chunk:**
  - Near chunks (within about 24 units) get the textured material table.
  - Further chunks get a flat table: each texture's `mean_argb`, lit per face by the engine, with slight per-block tint variants picked by hash.
  - The flat colours are blended toward the sky colour with distance, which acts as cheap fog.
  - Past the draw distance (about 64–80 units, tuned by measurement), the backdrop ground in the fog colour takes over. This is `backdrop_t.ground_argb`, the same trick as planet_base's `mean_argb`.
- Chunks behind the camera or outside the view cone are skipped before submission.
- **Remeshing:** when the edits applied at t change a chunk, it is remeshed.
  - This is memoised per chunk and keyed by the number of edits applied, so it is deterministic: the same t always gives the same geometry.
  - Only the one or two chunks being edited are ever rebuilt.

## Part A: assets (`main/craftminer/assets/`)

**Character, `miner.c/.h`**
- A blocky figure of our own design: a yellow hard hat with a lamp, a red shirt, blue overalls and brown boots.
- Parts: head (with a textured face), hat, body, 2 arms and 2 legs. Each part is built with `mesh_box` and has its own pivot, so the parts go through `mesh_submit_part`.
- The pose is a pure function of t:
  - `miner_walk(phase)`: legs and arms swinging, with a slight bob;
  - `miner_swing(phase)`: the arm arc for mining or placing;
  - a head turn.
- It holds a pickaxe or a block in its hand.

**First-person arm and pickaxe:** the same parts, placed in the camera's frame at the lower right, with a swing arc.

**Pickaxe, `pickaxe.c`:** a small voxel model: a handle and a head, flat colours.

**Mining feedback, `voxel/voxel_fx.c`:**
- **Target outline:** a black `scene_line` box round the targeted block. Lines win against their own face through the engine's depth bias.
- **Cracks:** procedural dark line segments on the face being hit, growing in 3–4 stages. No extra textures.
- **Break particles:** about 12 small flat cubes in the block's mean colour, thrown out under gravity, each a pure function of (t − t_break).
- **Dropped item:** a small textured cube that spins and bobs, then is drawn to the player and vanishes.
- **Placing:** a new block pops in, scaling 0.8 → 1.0 over about 0.15 s.

**Sky, `voxel/voxel_sky.c`:**
- A square sun and moon: emissive flat quads far out along the light direction.
- Square clouds: flat, emissive, off-white slabs at y ≈ 45, drifting with t.
- Stars from the shared `common/starfield`, used at night.

**Torch flame:** the shared `common/flame`, with CraftMiner's own flame texture, for torches by the cabin door at night.

## Part S: scenes (about 74 s)

Each scene is its own file in `main/craftminer/scenes/`, declared in `craftminer/craftminer.h` and added to `ALL_SCENES` and `PLAYLIST`. The space entries in `PLAYLIST` are commented out.

| # | Scene | ~s | Camera | Content |
|---|---|---|---|---|
| 1 | `cm_title` | 10 | third person | "CraftMiner" in **voxel letters**: blocks from a 5×7 pixel font (C r a f t M i n e), in stone and grass blocks, standing in the sky above the landscape. The camera drifts round them. They reuse the world mesher: the letters are just blocks. |
| 2 | `cm_overworld` | 12 | flying | A flyover: hills, trees, the lake, clouds, the sun. The establishing shot. |
| 3 | `cm_walk` | 10 | third person | The miner walks across the meadow. The camera tracks beside, then swings behind. |
| 4 | `cm_mining` | 14 | first person + third person | At the cliff: outline, cracks, break, particles, item pickup. Stone, then coal ore, then stone again, tunnelling in. A short third-person cutaway of the miner swinging. |
| 5 | `cm_building` | 16 | first person, then third person | A few blocks placed in first person. Then a time-lapse orbit: the cabin goes up block by block — log corners, plank walls, a door and window gap, a stepped plank roof. |
| 6 | `cm_nightfall` | 12 | third person | The finished cabin. The sky shifts to sunset and then to night: `backdrop_at` sky colour, and the light and fog colours per t. The moon rises, stars appear, torches flicker by the door. A slow pull-back to finish. |

## Part T: tooling

- **Textures:** in `tools/make_textures.py`, a new seeded group `rng4` with 16×16 block functions (drawn so they tile), the face texture and a torch flame, written to `textures/craftminer/`. Run `make textures`; add every PNG to `metadata/metadata.json`; check with `--preview` (the contact sheet).
- **Build:**
  - Add the new sources to `APP_SOURCES` in `CMakeLists.txt`, which is an explicit list.
  - Add the engine-free mesh builders (`voxel_mesh.c`, the miner and pickaxe builders) to `MESHCHECK_SRCS` in the Makefile.
- **meshcheck** (`tools/meshcheck_assets.h`):
  - the miner and the pickaxe, which must be closed and outward-facing;
  - mesher cases: a single block gives a closed cube; two adjacent blocks share no inner face; a greedy slab gives the expected face count.
- **scenecheck** (`tools/scenecheck.c`):
  - add the six scenes to `CHECKS[]`;
  - raise `MAX_OBJ` (16) and `MAX_LABELS` for the chunk objects;
  - `contact_ok` for the chunks touching each other and the miner touching the ground;
  - `near_ok` for the first-person arm and the block being mined.
- **If the cap is raised:** the same `-DSE_SCENE_TEXTURED_TRI_CAP=2048` goes into the app CMake (reaching the `synthengine3d` target) and the Makefile's `SCENECHECK_CFLAGS`, and is logged as a decision.

## Part D: step-by-step plan with status tracking

**Status values:** `todo` · `in progress` · `done` · `blocked (why)` · `skipped (why)`. Each step's Notes column records its result, with links to Findings (F-n) and Decisions (D-n).

| # | Step | Status | Notes |
|---|---|---|---|
| **0** | **Tracking** | | |
| 0.1 | Write the approved plan to `claudeplans/craftminer.md`, with the status table and D-40… | done | 2026-09-19; Part R added at the user's request (D-47) |
| **R** | **One subdirectory per segment (Part R, D-47)** | | |
| R.1 | Baseline: `make testcompare` over every reference; re-capture any made stale by D-39 (with a reason); save the `make scenecheck` report | done | 2026-09-19: turntable (3) and marauder_pursuit (4) identical; flyby 3 of 4 identical, 17 s different (the D-39 raycast hits), checked by eye and re-captured (`flyby-hits-raycast-D39`, F-34). scenecheck and meshcheck reports saved. |
| R.2 | Move: `git mv` code into `main/{common,dev,space}/`, textures into `textures/space/`; split `scenes.h`; fix includes, `texcache_get` paths, `CMakeLists.txt`, Makefile (install/apprepo/TEXTURES, scenecheck/meshcheck sources), `metadata.json`, `make_textures.py` output dirs | done | 2026-09-19: `scenes.h` → `space/space.h` + `dev/dev.h`; `main/objects/` → `space/objects/`; Makefile `SEGMENTS := space` drives TEXTURES, install/apprepo subdirectories and the scenecheck sources; `make textures` regenerates the 16 PNGs byte-identical. Only `texcache` in `common/` so far (F-35). |
| R.3 | `tools/segcheck.py` in `make build`; its own self-test (a planted cross-segment include must fail) | done | 2026-09-19: rules: cross-segment includes, core including a segment (except `reel.c`), `dev/` includes, `../` includes, texture names outside the segment's directory. Self-test plants seven violations plus a clean tree (`make segcheck`). |
| R.4 | Verify: build clean; meshcheck; scenecheck report unchanged; install (subdirectories, old flat textures deleted once); `testcompare` identical to R.1; README layout section | done | 2026-09-19: live and export builds clean; meshcheck and scenecheck reports unchanged; installed into `<app>/space/`, the 16 flat copies deleted; all 11 reference shots identical, so the textures load from the subdirectory. README: layout, `make segcheck`. `make format` now skips `third_party/` and `space/objects/` (F-35). |
| R.5 | Comment the space scenes out of `PLAYLIST` (D-44) | done | 2026-09-19, with the six CraftMiner scenes in. |
| **X** | **Cut-out transparency in the engine (Part X, D-48)** | | |
| X.1 | Loader: alpha → `SE_TEXEL_CUTOUT`, `cutout` flag, opaque-only mean | done | 2026-09-19: the log line marks cut-out textures (", cut-out"). |
| X.2 | Rasterizer: cutout sibling of the textured span loop | done | 2026-09-19: `scene_vrun_tex_cutout`, chosen per triangle; the raycast renderer uses the same pass. |
| X.3 | Docs, README, CHANGELOG 2.0 | done | 2026-09-19 |
| X.4 | Verify: every reference identical; cutout device shots (with 1.5) | done | 2026-09-19: all 11 reference shots identical; cm_world_test's tree shot shows the canopy's gaps (sky and inner leaves through them). |
| **Q** | **Quarter-resolution rendering (Part Q, D-49)** | | |
| Q.1 | Engine: render scale, projection halving, target indexing, viewport, raycast fallback | done | 2026-09-19 |
| Q.2 | Engine: `se_ppa_blit_scaled`, `se_ppa_layer_sync`, `se_ppa_buf_invalidate`; docs, CHANGELOG | done | 2026-09-19 |
| Q.3 | App: `scene_def_t.quarter`, half-size layer, upscale, scale-aware backdrop | done | 2026-09-19 |
| Q.4 | Verify: every reference identical at scale 1; quarter frames correct; perf | done | 2026-09-19: all 11 references identical; the quarter frame matches the full one in content and horizon (the upscale's interpolation rules out a per-pixel comparison of the screen); perf F-39. |
| **1** | **The voxel world** | | |
| 1.1 | Block textures and the face texture (`make_textures.py`, `rng4`); contact sheet review; `metadata.json`; `TEXCACHE_MAX` 48 | done | 2026-09-19: 20 textures (16×16; a seed per texture, not a shared stream); cut-out ones (leaves, flowers, tall grass, glass) saved as RGBA; opaque `leaves_fast` for canopies further off. |
| 1.2 | `voxel_world`: grid, generation, edit timeline, `voxel_height` | done | 2026-09-19: plus plants, `voxel_ground`/`voxel_solid`, `voxel_column`. |
| 1.3 | `voxel_mesh`: exposed faces, greedy merge, grass-side rule; meshcheck cases | done | 2026-09-19: reads a dense grid (chunk + border); fancy/fast modes; half-resolution cells; skirts. 11 meshcheck cases (volume = cells, area = exposed faces, counts). |
| 1.4 | `voxel_render`: chunks, near-textured / far-flat with fog tint, view culling, memoised remesh | done | 2026-09-19: three meshes per chunk (fancy, fast, coarse), built on first use; fog on the flat ones (F-36). |
| 1.5 | Dev scene `cm_world_test` (not in the playlist): orbit and walk-through shots → device shots and `perf`, to set the textured radius, draw distance and cap before any scene is built | done | 2026-09-19: lists within the caps without raising any (F-36); 4.3–11.7 fps live (F-37). |
| **2** | **Assets** | | |
| 2.1 | `miner` (parts, walk, swing), `pickaxe`, first-person arm; meshcheck | done | 2026-09-19: `miner_mesh.c` (head with a textured face, hat, brim, lamp; body; arm; leg; pickaxe; all closed, meshcheck) and `miner.c` (pose as a pure function; `miner_stroke`; first-person arm at 0.75 scale). `xform_mul` in the core. |
| 2.2 | `voxel_fx` (outline, cracks, particles, item, pop) and `voxel_sky` (sun, moon, clouds) | done | 2026-09-19: plus the torch flame; a shared block cube (`voxel_build_cube`, meshcheck); `starfield` moved to `common/` with `starfield_submit_above` (no stars on the ground beyond the draw distance); `flame` stays in `space/` (the torch uses its own). Device shots of cm_world_test's props and first-person shots reviewed. |
| **3** | **Scenes** (each: build, scenecheck, device shots at key instants → review → tune; `perf` per shot) | | |
| 3.1 | `cm_title` | done | 2026-09-19: scenecheck OK; watched live on the badge by the user: "scenes look fine" (D-52). Per-shot perf still to measure (4.1). |
| 3.2 | `cm_overworld` | done | 2026-09-19: scenecheck OK; watched live on the badge by the user: "scenes look fine" (D-52). Per-shot perf still to measure (4.1). |
| 3.3 | `cm_walk` | done | 2026-09-19: scenecheck OK; watched live on the badge by the user: "scenes look fine" (D-52). Per-shot perf still to measure (4.1). |
| 3.4 | `cm_mining` | done | 2026-09-19: scenecheck OK; watched live on the badge by the user: "scenes look fine" (D-52). Per-shot perf still to measure (4.1). |
| 3.5 | `cm_building` | done | 2026-09-19: scenecheck OK; watched live on the badge by the user: "scenes look fine" (D-52). Per-shot perf still to measure (4.1). |
| 3.6 | `cm_nightfall` | done | 2026-09-19: scenecheck OK; watched live on the badge by the user: "scenes look fine" (D-52). Per-shot perf still to measure (4.1). |
| **4** | **Wrap-up** | | |
| 4.1 | Perf section in `devdocs/performance.md`; decide on the textured cap | todo | |
| 4.2 | README (CraftMiner scenes); MJPEG export of the CraftMiner playlist for the user's review | todo | |
| 4.3 | Commit and push: **only when the user asks** | done | 2026-09-19: engine (V2.0) and showreel committed and pushed; `craftminer` merged into `main` and pushed (D-52). The playlist on `main` plays CraftMiner only until D-44's final order (space act first) is restored. |

## Part E: findings and decisions log

### Findings (F-n), each with date and source
- **F-31** 2026-09-19, devdocs/performance.md and the space reel: textured fill runs at roughly 5 Mpx/s, so a screen-sized textured layer costs about 50 ms whatever the triangle count. Flat fill is 3–4× cheaper; the PPA backdrop is free. A voxel view is mostly terrain, so this shapes Part V's level of detail.
- **F-32** 2026-09-19, engine and app limits relevant here:
  - Caps: flat 4096 and lines 4096 (fixed in the engine); textured 1024 and points 1024 (`#ifndef`, overridable with `-D`).
  - Textures: power-of-two edges up to 512, RGB565, no alpha, nearest-texel, UVs outside 0..1 repeat.
  - `texcache` holds 32 textures (16 in use), file names under 32 characters.
  - `mesh_box` takes one material for all six faces; per-face materials need `mesh_vert`/`mesh_quad`.
  - scenecheck tracks only `mesh_submit` objects, at most 16 per frame (`MAX_OBJ`).
- **F-33** 2026-09-19, the badge's bridge: the console (`localhost:4001`) and BadgeLink (`:4003`) ports can accept and drop connections while the bridge is down; `make testcompare` failed twice in `mode_badgelink` before the user brought it back. After that, a full `testcompare` from a fresh clone of `82af0f6` passed: marauder_pursuit, 4 shots identical to the references. So everything the test framework needs is in git.

- **F-34** 2026-09-19, R.1 baseline: the `spacestation_flyby` 17 s reference predated D-39 (hits now raycast onto the hull) and differed; the other 10 reference shots were identical. The new frame was fetched and checked by eye (sparks on the hull, a miss leaving the frame edge) and re-captured.
- **F-35** 2026-09-19, the move (R.2–R.4):
  - `make format` used to run clang-format over all of `main/`, including the vendored `third_party/stb_image_write.h` and the generated `ship_model.h`. Both were restored, and the target now skips those directories.
  - `flame.c` hard-codes the space act's flame textures (`space/flame.png`, `space/flame_red.png`), so it cannot be shared as-is; it moves to `common/` with its texture as a parameter when CraftMiner needs it.
  - The launcher creates asset subdirectories on install (`fs_utils_mkdir_recursive`, `tanmatsu-launcher/main/app_management.c:217`); other apps in the app repository ship assets in subdirectories.

- **F-36** 2026-09-19, the first world test (scenecheck): all chunks meshed full-detail overflowed both lists (4746 flat, 2181 textured). Leaves were 58% of the near triangles (the insides of see-through canopies), plants 15%. Levels of detail brought it to ≤ 2262 flat and ≤ 910 textured, with no cap raised:
  - see-through canopies and plants only within 8 blocks (fancy mesh);
  - textured with opaque "fast" leaves within 18 (fast mesh);
  - flat colours with fog to 36, half resolution (2×2×2 blocks a cell) to 64 (coarse mesh, with skirts where it meets full resolution, coloured like the ground; without them the sky showed through the step);
  - fewer trees (38% of candidate spots) and plants (3–6% of grass).
  Meshing all four levels of all 64 chunks at start took 4.6 s on the badge and pushed app start past the test runner's 12 s connect budget. With the mesher reading a dense copy of each chunk and only the height band that can have faces, the fast and far levels sharing one mesh, and the rest built on first use, world start-up is 0.3 s.
- **F-37** 2026-09-19, `perf scene=cm_world_test`: overview 10.4 fps (flat pass ~65 ms: 1400 flat triangles, the whole screen of terrain with hills behind hills), walk 6.3 fps (textured ~95 ms + flat ~45 ms), tree 5.4 fps (the camera under a canopy: textured ~175 ms). The block world is fill-bound at 800×480 on both paths. Front-to-back sorting (`depth_order`) cut the canopy shot from 220 to 175 ms and cost the overview a little, so it is a per-scene choice (`scene_def_t.depth_order`, off for the space scenes). Faster would need engine work: rendering the 3D at half resolution and scaling it up (which suits the blocky look), or a faster raster loop. To offer the user, not done (D-15).

- **F-38** 2026-09-19, z-buffer vs raycast renderer, same build, `perf` per shot (rast mean in ms; asked by the user):

  | scene / shot | z-buffer | raycast |
  |---|---:|---:|
  | cm_world_test overview (1430 flat, 100 textured tris) | 76 | 391 |
  | cm_world_test walk (1490 flat, 150 textured) | 138 | 346 |
  | cm_world_test tree (60 flat, 100 textured) | 174 | 180 |
  | marauder_pursuit | 18 | 23 |
  | spacestation_flyby establish / chase / exit / reverse | 4.3 / 25 / 10 / 11 | 9.0 / 29 / 16 / 22 |

  The z-buffer wins everywhere; the raycaster only replaces the flat pass (textured triangles go through the same textured pass in both), so where the frame is textured (the tree) they tie. On the block world's flat terrain the raycaster is 2.5–5× slower: big, overlapping triangles bin into many tiles, and each pixel of a tile tests every triangle binned there. The reel stays on the z-buffer.

- **F-39** 2026-09-19, quarter resolution on the badge (`cm_world_test`, PPA upscale):

  | shot | fps full → quarter | rast ms full → quarter |
  |---|---:|---:|
  | overview | 10.4 → 20.6 | 76 → 24 |
  | walk | 6.3 → 15.5 | 138 → 40 |
  | tree | 5.4 → 15.6 | 174 → 46 |

  Rasterizing drops to about a third (not a quarter: per-triangle setup does not shrink). `wait` is 6.5 ms per frame: the PPA fills, the layer sync and the 2× upscale, waited for. What now limits the frame is the CPU before rasterizing: submit about 14–16 ms (mesh_submit transforms every chunk vertex, though the chunks are already in world coordinates) and prepare about 4 ms (sorting). The PPA's scaler interpolates: in the upscaled frame only 41% of the 2×2 blocks are one colour, and intermediate shades appear between texels; the driver has no filter setting.

- **F-40** 2026-09-19, scenecheck of the six scenes (peak entries per frame, flat / textured): title 2194 / 1785 (letters kept textured, F-41), overworld 3160 / 1244, walk 2679 / 1179, mining 2254 / 675, building 2524 / 1257, nightfall 2518 / 1143. Four scenes need more than the engine's default 1024 textured triangles, all well under 2048; the flat list stays under 4096.
- **F-41** 2026-09-19, the title: its letters are about 36 blocks from the camera, beyond the textured range, so they came out in flat colours; textured to 44 blocks, the frame needed 2878 textured triangles. A box in the view (`vox_view_t.tex_box`) keeps just the letters' chunks textured instead: 1785. The first camera path also ran through a hill (scenecheck: a chunk through the near plane); it is now lifted clear of the ground along the whole path, worked out at init.

### Decisions (D-n), each with date and who decided
- **D-40** 2026-09-19, user: the eleven space scenes are complete. New work goes on branch `craftminer`.
- **D-41** 2026-09-19, user: a few scenes of a simplified Minecraft-like world: a typical overworld, a person walking around, mining blocks, building a simple log cabin. The title reads "CraftMiner", to stay clear of copyright. Claude adds: all textures generated by us, our own character design, no mobs.
- **D-42** 2026-09-19, user: the cameras are a mix of first person and third person.
- **D-43** 2026-09-19, user: a live frame rate below 30 fps is fine (the MJPEG export is fixed-step either way).
- **D-44** 2026-09-19, user: the CraftMiner scenes will eventually be appended to the space reel. For easier manual checking, the space scenes are commented out of `PLAYLIST` for now; they stay in `ALL_SCENES`.
- **D-45** 2026-09-19, user: `SE_SCENE_TEXTURED_TRI_CAP` may be raised (e.g. to 2048) through the app build, if a measured view needs it.
- **D-46** 2026-09-19, user: the plan lives in `claudeplans/` of this repo, so it is under source control.
- **D-47** 2026-09-19, user: the existing space assets, scenes and textures move into a subdirectory, so each showreel segment has its own and nothing is mixed between segments by accident. Claude's layout is Part R: shared code sits in `main/common/` on purpose, textures go into per-segment subdirectories on the badge too, and `tools/segcheck.py` enforces the separation in `make build`. This is done first (steps R.1–R.5), before any CraftMiner code.
- **D-48** 2026-09-19, user: add cut-out transparency to the engine (Part X), then continue with CraftMiner. Claude had estimated it on request: cut-out is about 100–150 lines, costs nothing for opaque textures and is low-risk; blended transparency would be 1–2 days, z-buffer only, with +30–50% per translucent pixel. This reverses D-3 for cut-out only.
- **D-49** 2026-09-19, user: add quarter-resolution rendering, switchable per scene: every other pixel of every other line into a smaller buffer, upscaled by the PPA (Part Q). Then continue with CraftMiner. After seeing it on the badge, the user kept the PPA's soft upscale ("looks fine"; much faster than a CPU upscale) and declined the CPU nearest-neighbour variant.
- **D-50** 2026-09-19, Claude: per-scene engine options in `scene_def_t` -- `depth_order` (front-to-back sorting; F-37) and `quarter` (D-49), both off for the space scenes, whose references are unchanged.
- **D-51** 2026-09-19, Claude, under D-45 (measured: F-40): `SE_SCENE_TEXTURED_TRI_CAP` = 2048 for the app build (CMakeLists.txt, before the engine is added; the Makefile passes the same to scenecheck). About 70 KB more PSRAM (the list does not fit internal SRAM); no per-frame cost.
- **D-52** 2026-09-19, user: after watching the six scenes live on the badge, "scenes look fine": commit and push the engine and the app, merge `craftminer` into `main`, push everything. Still open: the per-shot perf section (4.1), the README and a video export (4.2), and D-44's final order (the space act back in the playlist, CraftMiner after it).

## Verification
- **Host:** `make meshcheck` (the new meshes and mesher cases) and `make scenecheck` (all scenes, the new ones included; no cap overruns; near and contact rules).
- **Device:** `make cycle TEST="shots scene=cm_x ms=…"` and `perf scene=cm_x`, per shot. Claude reviews the frames, fetched only when looks must be judged; the user judges the final look.
- **Space scenes unchanged:** after the move (Part R) and at the end, `make testcompare` over every reference gives identical hashes; the scenecheck report is unchanged by the move.
- **Segments stay apart:** `tools/segcheck.py` passes (it runs in every `make build`).
- **Build:** clean, no new warnings; clang-format applied.
- **End to end:** `make export` produces the CraftMiner video; `make install` restores the live reel afterwards.

## Critical files
- **Moved (Part R):** `main/assets/*` → `main/space/assets/` or `main/common/`; `main/scenes/*` → `main/space/scenes/` or `main/dev/`; `textures/*.png` → `textures/space/`.
- **New:**
  - `main/craftminer/voxel/{voxel_world,voxel_mesh,voxel_render,voxel_fx,voxel_sky}.{c,h}`
  - `main/craftminer/assets/{miner,pickaxe}.{c,h}`
  - `main/craftminer/scenes/cm_*.c`, `main/craftminer/craftminer.h`, `main/space/space.h`, `main/dev/dev.h`
  - `textures/craftminer/*.png` (blocks, face, torch flame)
  - `tools/segcheck.py`
  - `claudeplans/craftminer.md`
- **Changed:**
  - `main/reel.c` (ALL_SCENES, PLAYLIST), `main/common/texcache.c`, every moved file's includes
  - `CMakeLists.txt`, `Makefile`, `metadata/metadata.json`
  - `tools/make_textures.py`, `tools/scenecheck.c`, `tools/meshcheck_assets.h`
  - `README.md`, `devdocs/performance.md`
- **Reused:**
  - `mesh_*` builders and `mesh_submit` / `mesh_submit_part` (`main/mesh.h`, `mesh_render.h`)
  - `camera_look_at`, `value_noise`, `hash01`, `smoothstep`, `path_t` (`main/xform.h`, `camera.h`)
  - `backdrop_t` / `backdrop_at` (`main/backdrop.h`)
  - `common/`: `texcache_get`, `flame`, `starfield`
- **Engine:** no changes planned. Anything that turns up there → stop and ask (D-15).
