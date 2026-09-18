#pragma once
// =====================================================================
//  Showreel asset  --  planets
// ---------------------------------------------------------------------
//  A textured sphere (mesh_sphere, 24 x 12: ~260 textured triangles face
//  the camera) with an equirectangular map, lit by the scene light, so a
//  low sun gives it a day and a night side. Model space: radius 1 round
//  the origin, poles on +-y; a scene sizes it with xform_t.scale and
//  spins it about its pole.
//
//    PLANET_TERRAN  the industrial planet of scenes 2-5, from orbit
//    PLANET_GAS     the banded gas giant of the second system
// =====================================================================

#include "xform.h"

typedef enum {
    PLANET_TERRAN = 0,
    PLANET_GAS,
    PLANET_KIND_COUNT,
} planet_kind_t;

// Build the sphere and fetch the maps (texcache). Idempotent.
void planet_init(void);
void planet_shutdown(void);

// Draw a planet posed by `x`. Call after the scene's camera is set.
void planet_submit(xform_t const* x, planet_kind_t kind);
