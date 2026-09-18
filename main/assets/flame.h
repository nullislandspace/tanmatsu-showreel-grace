#pragma once
// =====================================================================
//  Showreel asset  --  Frontier-style thruster flame
// ---------------------------------------------------------------------
//  Frontier: Elite II style: a solid glowing spike behind an engine
//  nozzle, flickering in length. A `FLAME_SIDES`-sided cone whose base
//  sits on the nozzle and whose tip trails behind the ship (model -z).
//  Emissive (the scene light never darkens it) and textured with a
//  white-hot-to-colour gradient along its length, or a flat colour if
//  the texture is missing. Reusable by any ship; the style picks the
//  colour.
// =====================================================================

#include "xform.h"

typedef enum {
    FLAME_BLUE = 0,  // the player: flame.png
    FLAME_RED,       // marauders: flame_red.png
    FLAME_STYLE_COUNT,
} flame_style_t;

#define FLAME_SIDES 6

// Load the flame textures from `asset_dir`. Idempotent. A missing
// texture is logged and that style falls back to a flat colour.
void flame_init(char const* asset_dir);
void flame_shutdown(void);

// One flame on the ship posed by `x`: base centred on `nozzle` (model
// space), base circumradius `r`, trailing `len` model units along -z.
// The base hexagon starts at angle 0 on +x, like a hexagonal pod face.
// Faces turned away from the camera are culled.
void flame_submit(xform_t const* x, vec3_t nozzle, float r, float len, flame_style_t style);

// Flicker: a length multiplier around 1.0, a pure function of show time
// `t`. Give each flame its own `seed` so they shimmer independently.
float flame_flicker(double t, unsigned seed);
