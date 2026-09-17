# SynthEngine Showreel

A demo reel for [SynthEngine3D](https://github.com/nullislandspace/synthengine3D) on the
[Tanmatsu](https://nicolaielectronics.nl/docs/tanmatsu/), run through
[Graceloader](https://github.com/nullislandspace/tanmatsu-graceloader).

**Status: scaffold.** The engine is wired into the build and `main/main.c` draws one
spinning triangle -- enough to prove the toolchain, the link and the projection. The reel
itself is not designed yet.

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
