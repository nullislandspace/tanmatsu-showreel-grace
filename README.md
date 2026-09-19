# SynthEngine Showreel

A demo reel for [SynthEngine3D](https://github.com/nullislandspace/synthengine3D) on the
[Tanmatsu](https://nicolaielectronics.nl/docs/tanmatsu/), run through
[Graceloader](https://github.com/nullislandspace/tanmatsu-graceloader).

A Frontier: Elite II-style intro, "in the spirit of": our own sequence and models,
textured instead of flat-shaded.

## The reel

The playlist (`main/reel.c`) plays eleven scenes in a loop, about 112 s
(`claudeplans/implementation.md`, Part C4):

1. **`title`** (12.5 s) -- "Borderworlds:" / "Superior" in 3D block letters, drifting in
   from above and below, canted into the screen, on the planet's sky; then the hero
   flies past behind the lettering.
2. **`planet_landing`** (12 s) -- the player's ship comes down at an industrial planet
   base (halls, tanks, burning flare stacks) and settles on the pad.
3. **`marauder_approach`** (6 s) -- the two marauders fly towards the planet.
4. **`pad_strafe`** (7 s) -- they make a low pass over the base, their lasers tearing
   up the ground round the landed ship.
5. **`emergency_takeoff`** (8 s) -- the ship lifts off and climbs between the
   marauders as they come round again; both sides turn to aim and fire, and miss.
6. **`marauder_pursuit`** (6 s) -- two marauders in formation, seen from close behind
   the right one, weaving and firing their lasers straight ahead at the player's ship (out of frame).
7. **`spacestation_flyby`** (20 s) -- the player's ship threads the gap between two
   spokes of a turning 2001-style wheel station; the marauders take the long way round
   the ring, close onto its tail and open fire -- beams straight off their noses, near
   misses and three hits that burn on its hull. Four shots: establish, chase (through the gap),
   exit (looking back at the wheel), reverse chase.
8. **`warp_out`** (7.5 s) -- the player's ship pulls ahead and jumps to hyperspace;
   the marauders fly on, then jump after it, one by one.
9. **`asteroid_ambush`** (16 s) -- another solar system, a gas giant in the sky: the
   ship drops out of warp and hides behind a big asteroid; the marauders warp in on its
   trail and fly past; it slides out behind them and opens fire.
10. **`marauder_downfall`** (8.8 s) -- close beside the pair under blue fire from behind:
    the yellow marauder is hit three times and blows apart; the green one climbs away
    and warps out.
11. **`hero_rolls`** (8 s) -- close beside the player's ship at speed, space dust
    streaming past: two barrel rolls, then it pulls away towards the gas giant.

Three more scenes are compiled in but not played: `turntable` (the player's ship on a
turntable, the original single item), `assets` (a development viewer, one named shot
per asset) and `horizon` (a test of the sky/ground backdrop under a rolling camera).
The planet scenes run at 10-30 fps depending on how much textured surface fills the
view; that is still to be decided (`devdocs/performance.md`).

Keys: **N** skips to the next scene, **P** saves a screenshot to
`/sd/showreel/shotNNN.png` (uncompressed PNG, ~1.1 MB; stalls about a second, which the
show clock leaves out), **F1** returns to the launcher.

## How it is built

- **One subdirectory per reel segment.** The space act lives in `main/space/`
  (`assets/`, `scenes/`, `space.h`) with its textures in `textures/space/`; each later
  segment gets its own pair the same way. Shared code sits in `main/common/` on purpose,
  the core (clock, reel, meshes, camera, backdrop, tests, export) in `main/` itself, and
  the core's dev scenes in `main/dev/`. `tools/segcheck.py` runs in every `make build` and
  fails on an include or texture name that crosses segments (`make segcheck` also runs
  its self-test).
- **Scenes** (`main/space/scenes/`) are pure functions of scene time: `camera(t)` sets the
  camera and `submit(t)` draws exactly instant t, with no state from frame to frame.
  Ships follow Catmull-Rom paths (`main/xform.c`), face their velocity and bank into
  turns.
- **The backdrop** (`main/backdrop.c`) is painted by the PPA, not the CPU: black space,
  or sky and ground split at the horizon (`main/horizon.c`, as in Stunt Racer, but
  also upside down). A scene declares which in its `scene_def_t`; the camera is set
  first so the fills can run while the scene submits.
- **The show clock** (`main/showtime.c`) is the only time source: the wall clock, or
  fixed steps (for the video export and the shot tests).
- **Assets** (`main/space/assets/`) are generators any scene can reuse: the player's ship, the
  marauder (one type, green and yellow liveries), the wheel station, a starfield of
  single-pixel `scene_point`s, Frontier-style engine flames and laser beams (red for the
  marauders, blue for the player). Station and marauder are built procedurally from
  mesh builders (`main/mesh.c`: box, cylinder, ring, cone, loft); every builder call
  records a part, so a ship can come apart piece by piece (`mesh_submit_part`), and a
  pose may stretch as well as rotate (`mat3_stretch`, for the warp).
- **More assets for the full reel**: the title's 3D lettering (`title_text`, a stroke
  font on `mesh_stroke`), the planet base with its PPA sky (`planet_base`), planets
  (`mesh_sphere`), asteroids (`mesh_blob`), the warp, explosions with tumbling debris,
  laser impacts and space dust. The asset viewer (`assets`, not in the playlist) shows
  each of them as a named shot.
- **Formations** (`main/space/scenes/formation.c`): ships keeping slots in a formation's own
  frame, weaving, banking and rocking; shared by the marauder scenes.
- The player's mesh in `main/space/objects/ship_model.h` is vendored from
  [`tanmatsu-synthracer-grace`](https://github.com/nullislandspace/tanmatsu-synthracer-grace),
  which generates it from `openscad/ship.3mf`. It is auto-generated: re-export it there
  rather than editing it here.
- The textures in `textures/<segment>/` are generated by `tools/make_textures.py` (numpy +
  Pillow); `make textures` regenerates them. They are committed, so a normal build doesn't
  need Python, and `make install` uploads them into the same subdirectories next to
  `app.so`.

The design, the step-by-step plan with its status, and every finding and decision made
along the way are in [`claudeplans/implementation.md`](claudeplans/implementation.md) (the
space act) and [`claudeplans/craftminer.md`](claudeplans/craftminer.md) (CraftMiner, in
progress on branch `craftminer`).
Frame timings are logged to the console once a second; [`devdocs/performance.md`](devdocs/performance.md)
holds the reference numbers.

The engine lives in [`synthengine3D/`](synthengine3D/) as a git submodule, so a fresh
checkout needs:

```sh
git clone --recursive git@github.com:nullislandspace/tanmatsu-showreel-grace.git
# or, in an existing checkout:
git submodule update --init --recursive
```

## Building

Everything goes through the Makefile:

```sh
make prepare    # one-time: fetch ESP-IDF and the badgelink tool
make build      # -> build/app.so
make verify     # check every undefined symbol is provided by graceloader
make install    # upload to the device
make run        # launch it
```

`install` and `run` switch the badge to BadgeLink mode first (`make mode_badgelink`,
which also gets a running showreel out of the way); `make mode_debug` is the opposite
direction.

## Checks and device tests

```sh
make segcheck                                         # host: no includes or texture names across main/<segment>/ (+ self-test)
make meshcheck                                        # host: math + every mesh generator (closed, outward-facing)
make scenecheck [SCENES="name ..."]                   # host: every scene at 30 fps -- near plane, list caps, clearances, beams, framing
make testrun TEST="perf scene=spacestation_flyby"     # device: per-second and per-shot timings
make testrun TEST="shots scene=turntable ms=0,2500"   # device: render exact instants, report frame hashes
make cycle TEST="..."                                 # build, install, run, then the test
make testcompare TEST="shots ..."                     # compare the frame hashes with tests/refs/manifest.json
make recover                                          # after a crash or hang: reset the badge
```

The app runs the test when it receives the command over the debug console
(`main/devtest.h`) and returns to the launcher by itself; `tools/testrun.py` writes the
results to `results/`. Shot images stay on the SD card in `/sd/showreel/test/`;
`TESTFLAGS=--fetch` downloads them over BadgeLink, which is slow (over a minute each).

## Video export

`make export` builds the reel with the `SHOWREEL_EXPORT_MJPEG` option, installs and starts
it: it renders every playlist scene once at exactly 30 frames per second of show time and
writes an MJPEG AVI to `/sd/showreel/showreel.avi` (about 14 minutes for the 112 s reel),
then returns to the launcher. `make install` puts the live reel back. To get an MP4:

```sh
ffmpeg -i showreel.avi -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -movflags +faststart showreel.mp4
```

## License

This software is under the [MIT license](https://opensource.org/license/mit).

(C) 2026 Rene Schickbauer
