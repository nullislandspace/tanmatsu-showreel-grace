#!/usr/bin/env python3
"""Keep the reel's segments apart (claudeplans/craftminer.md, Part R).

Each segment of the reel has its code in main/<segment>/ and its textures
in textures/<segment>/. Nothing may be mixed between them by accident, so
this fails when:

  - a file in main/<segment>/ includes a header of another segment;
  - the core (main/*.c|h, main/common/, main/dev/) includes a segment's
    header -- except main/reel.c, the one place that knows every segment;
  - anything but main/reel.c and main/dev/ includes a dev/ header;
  - an include climbs out of its directory ("../");
  - a segment names a texture outside textures/<segment>/, or the core
    names any segment's texture ("<segment>/....png" string literals).

What more than one segment uses belongs in main/common/, on purpose.

    python3 tools/segcheck.py space craftminer   # the segments (Makefile SEGMENTS)
    python3 tools/segcheck.py --self-test        # plants violations; each must fail

Exit status: 0 clean, 1 violations found, 5 usage or self-test failure.
"""
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
PNG = re.compile(r'"([^"\n]*\.png)"')
SKIP = {"third_party"}


def owner(rel, segments):
    """The segment a file under main/ belongs to, or None for the core."""
    parts = rel.parts
    return parts[0] if len(parts) > 1 and parts[0] in segments else None


def check(main_dir, segments):
    errors = []
    for path in sorted(main_dir.rglob("*")):
        if path.suffix not in (".c", ".h") or not path.is_file():
            continue
        rel = path.relative_to(main_dir)
        if rel.parts[0] in SKIP:
            continue
        own = owner(rel, segments)
        text = path.read_text(encoding="utf-8", errors="replace")
        line_of = lambda pos: text.count("\n", 0, pos) + 1  # noqa: E731
        is_reel = rel.as_posix() == "reel.c"
        for m in INCLUDE.finditer(text):
            inc = m.group(1)
            first = inc.split("/")[0]
            where = f"main/{rel.as_posix()}:{line_of(m.start())}"
            if ".." in inc.split("/"):
                errors.append(f'{where}: include "{inc}" climbs out of its directory')
            elif first in segments and first != own and not is_reel:
                who = f"segment '{own}'" if own else "the core"
                errors.append(f'{where}: {who} includes "{inc}" of segment \'{first}\'')
            elif first == "dev" and not (is_reel or rel.parts[0] == "dev"):
                errors.append(f'{where}: include "{inc}": dev/ is only for reel.c and the dev scenes')
        for m in PNG.finditer(text):
            name = m.group(1)
            first = name.split("/")[0] if "/" in name else None
            where = f"main/{rel.as_posix()}:{line_of(m.start())}"
            if own is not None and first != own:
                errors.append(f'{where}: segment \'{own}\' names texture "{name}" outside textures/{own}/')
            elif own is None and first in segments:
                errors.append(f'{where}: the core names texture "{name}" of segment \'{first}\'')
    return errors


def self_test():
    """Each planted violation must be reported; the clean tree must pass."""
    cases = {
        "clean": {},
        "cross include": {"b/scenes/x.c": '#include "a/assets/y.h"\n'},
        "core includes segment": {"common/z.c": '#include "a/assets/y.h"\n'},
        "dev include": {"a/scenes/x2.c": '#include "dev/dev.h"\n'},
        "climbing include": {"a/scenes/x3.c": '#include "../../b/b.h"\n'},
        "foreign texture": {"a/assets/t.c": 'char const* T = "b/rock.png";\n'},
        "bare texture": {"a/assets/t2.c": 'char const* T = "rock.png";\n'},
        "core texture": {"common/t3.c": 'char const* T = "a/rock.png";\n'},
    }
    base = {
        "reel.c": '#include "a/a.h"\n#include "b/b.h"\n#include "dev/dev.h"\n',
        "a/a.h": '#include "scene.h"\n',
        "b/b.h": '#include "common/texcache.h"\n',
        "a/assets/y.c": '#include "a/assets/y.h"\nchar const* T = "a/rock.png";\n',
        "dev/horizon.c": '#include "dev/dev.h"\n',
        "common/texcache.c": 'char const* F = "/%s_%06d.png";\n',
    }
    ok = True
    for name, extra in cases.items():
        with tempfile.TemporaryDirectory() as tmp:
            main_dir = Path(tmp)
            for rel, body in {**base, **extra}.items():
                (main_dir / rel).parent.mkdir(parents=True, exist_ok=True)
                (main_dir / rel).write_text(body)
            errors = check(main_dir, {"a", "b"})
            want_fail = name != "clean"
            if bool(errors) != want_fail:
                ok = False
                print(f"self-test '{name}': expected {'a violation' if want_fail else 'none'}, got {errors}")
    print("segcheck self-test: " + ("OK" if ok else "FAILED"))
    return ok


def main():
    args = sys.argv[1:]
    if args == ["--self-test"]:
        return 0 if self_test() else 5
    if not args or any(a.startswith("-") for a in args):
        print(__doc__)
        return 5
    segments = set(args)
    for seg in sorted(segments):
        if not (ROOT / "main" / seg).is_dir():
            print(f"segcheck: no main/{seg}/")
            return 5
    errors = check(ROOT / "main", segments)
    for e in errors:
        print(e)
    print(f"segcheck: {len(segments)} segment(s), " + (f"{len(errors)} violation(s)" if errors else "clean"))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
