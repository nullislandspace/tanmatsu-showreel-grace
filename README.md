# SynthEngine Showreel

A demo reel for [SynthEngine3D](https://github.com/nullislandspace/synthengine3D) on the
[Tanmatsu](https://nicolaielectronics.nl/docs/tanmatsu/), run through
[Graceloader](https://github.com/nullislandspace/tanmatsu-graceloader).

**Status: one reel item.** `main/ship.c` puts the Race the Synth ship on a turntable
against a black screen -- 169 vertices, 306 triangles and 84 outline edges, back-face
culled and flat-shaded per face. The reel itself is not designed yet; this is the first
item and the proof that the scene pipeline takes real geometry.

The mesh in `main/objects/ship_model.h` is vendored from
[`tanmatsu-synthracer-grace`](https://github.com/nullislandspace/tanmatsu-synthracer-grace),
which generates it from `openscad/ship.3mf`. It is auto-generated: re-export it there
rather than editing it here. Note that the engine has no lighting of its own -- `scene_tri`
takes one flat colour per triangle -- so the shading is computed in `ship.c` at submit
time.

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

If `install` cannot reach the device it is usually sitting in the launcher's USB debug
mode. `make mode_badgelink` switches it back to BadgeLink mode; `make mode_debug` is the
opposite direction.

## License

This software is under the [MIT license](https://opensource.org/license/mit).

(C) 2026 Rene Schickbauer
