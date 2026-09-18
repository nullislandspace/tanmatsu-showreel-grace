#pragma once
// =====================================================================
//  Showreel asset  --  the marauder (pirate fighter)
// ---------------------------------------------------------------------
//  One ship type (geometry in marauder_mesh.h) in several liveries: the
//  mesh and its UVs are shared, a livery only swaps the hull texture.
//  Red Frontier flames trail both nacelles.
// =====================================================================

#include "xform.h"

typedef enum {
    MARAUDER_GREEN = 0,  // old, dirty green paint
    MARAUDER_YELLOW,     // darkened, sooty yellow
    MARAUDER_LIVERY_COUNT,
} marauder_livery_t;

// Build the mesh and fetch the textures (texcache). Idempotent.
void marauder_init(void);
void marauder_shutdown(void);

// Draw one marauder posed by `x` (model space: nose +z, wingspan 1).
// `throttle` scales the flames (0 = off); `t` is show time and
// `flicker_seed` makes each ship's flames shimmer on their own.
void marauder_submit(xform_t const* x, marauder_livery_t livery, float throttle, double t, unsigned flicker_seed);

// World position of gun muzzle `side` (0 = left, 1 = right) on the ship
// posed by `x` -- where a laser bolt starts.
vec3_t marauder_gun(xform_t const* x, int side);
