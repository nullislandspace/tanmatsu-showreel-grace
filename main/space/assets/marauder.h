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

// The wreck of a marauder that exploded at `t_explode` posed by `at` and
// moving with `vel` (units per second), at time `t`: its parts (canopy,
// wings, fins, nacelles, guns, fuselage) fly apart, each with its own
// outward velocity on top of the ship's, tumbling about its own centre.
// Draw it from t_explode on, instead of the ship (explosion.h adds the
// fireball).
void marauder_submit_debris(xform_t const* at, vec3_t vel, marauder_livery_t livery, float t, float t_explode,
                            unsigned seed);

// World position of gun muzzle `side` (0 = left, 1 = right) on the ship
// posed by `x` -- where a laser bolt starts.
vec3_t marauder_gun(xform_t const* x, int side);

// Where a ray from `from` along `dir` (normalised) first strikes the marauder
// posed by `x`, no further than `max`: true with the distance in *dist
// (mesh_raycast). A laser that hits stops there.
bool marauder_raycast(xform_t const* x, vec3_t from, vec3_t dir, float max, float* dist);
