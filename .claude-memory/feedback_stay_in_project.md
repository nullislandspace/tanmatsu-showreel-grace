---
name: Stay in project directory
description: Never run build commands in other project directories like tanmatsu-launcher
type: feedback
---

Never run build commands (idf.py, make, cmake) in other project directories. The launcher repo is only for reading/copying source files.

**Why:** Running builds in the wrong directory is confusing and wastes time.

**How to apply:** Always stay in /home/cavac/src/tanmatsu/tanmatsu-showreel-grace for all build commands. Only use tanmatsu-launcher for reading source files to copy/adapt.
