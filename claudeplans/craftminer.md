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
| R.5 | Comment the space scenes out of `PLAYLIST` (D-44) | todo | Waits for the first CraftMiner scene (3.1): an empty playlist would leave the reel nothing to play. |
| **1** | **The voxel world** | | |
| 1.1 | Block textures and the face texture (`make_textures.py`, `rng4`); contact sheet review; `metadata.json`; `TEXCACHE_MAX` 48 | todo | |
| 1.2 | `voxel_world`: grid, generation, edit timeline, `voxel_height` | todo | |
| 1.3 | `voxel_mesh`: exposed faces, greedy merge, grass-side rule; meshcheck cases | todo | |
| 1.4 | `voxel_render`: chunks, near-textured / far-flat with fog tint, view culling, memoised remesh | todo | |
| 1.5 | Dev scene `cm_world_test` (not in the playlist): orbit and walk-through shots → device shots and `perf`, to set the textured radius, draw distance and cap before any scene is built | todo | |
| **2** | **Assets** | | |
| 2.1 | `miner` (parts, walk, swing), `pickaxe`, first-person arm; meshcheck | todo | |
| 2.2 | `voxel_fx` (outline, cracks, particles, item, pop) and `voxel_sky` (sun, moon, clouds) | todo | |
| **3** | **Scenes** (each: build, scenecheck, device shots at key instants → review → tune; `perf` per shot) | | |
| 3.1 | `cm_title` | todo | |
| 3.2 | `cm_overworld` | todo | |
| 3.3 | `cm_walk` | todo | |
| 3.4 | `cm_mining` | todo | |
| 3.5 | `cm_building` | todo | |
| 3.6 | `cm_nightfall` | todo | |
| **4** | **Wrap-up** | | |
| 4.1 | Perf section in `devdocs/performance.md`; decide on the textured cap | todo | |
| 4.2 | README (CraftMiner scenes); MJPEG export of the CraftMiner playlist for the user's review | todo | |
| 4.3 | Commit and push: **only when the user asks** | todo | |

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

### Decisions (D-n), each with date and who decided
- **D-40** 2026-09-19, user: the eleven space scenes are complete. New work goes on branch `craftminer`.
- **D-41** 2026-09-19, user: a few scenes of a simplified Minecraft-like world: a typical overworld, a person walking around, mining blocks, building a simple log cabin. The title reads "CraftMiner", to stay clear of copyright. Claude adds: all textures generated by us, our own character design, no mobs.
- **D-42** 2026-09-19, user: the cameras are a mix of first person and third person.
- **D-43** 2026-09-19, user: a live frame rate below 30 fps is fine (the MJPEG export is fixed-step either way).
- **D-44** 2026-09-19, user: the CraftMiner scenes will eventually be appended to the space reel. For easier manual checking, the space scenes are commented out of `PLAYLIST` for now; they stay in `ALL_SCENES`.
- **D-45** 2026-09-19, user: `SE_SCENE_TEXTURED_TRI_CAP` may be raised (e.g. to 2048) through the app build, if a measured view needs it.
- **D-46** 2026-09-19, user: the plan lives in `claudeplans/` of this repo, so it is under source control.
- **D-47** 2026-09-19, user: the existing space assets, scenes and textures move into a subdirectory, so each showreel segment has its own and nothing is mixed between segments by accident. Claude's layout is Part R: shared code sits in `main/common/` on purpose, textures go into per-segment subdirectories on the badge too, and `tools/segcheck.py` enforces the separation in `make build`. This is done first (steps R.1–R.5), before any CraftMiner code.

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
