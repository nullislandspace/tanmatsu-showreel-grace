#pragma once
// =====================================================================
//  Showreel asset  --  asteroids
// ---------------------------------------------------------------------
//  Rocky blobs (asteroid_mesh.h) in rock.png. A few distinct shapes are
//  built at init; a scene picks one by index and poses it (scale = the
//  radius it wants, a slow tumble).
// =====================================================================

#include "xform.h"

#define ASTEROID_SHAPES 4

// Build the shapes and fetch the texture. Idempotent.
void asteroid_init(void);
void asteroid_shutdown(void);

// Draw rock `shape` (0 .. ASTEROID_SHAPES - 1) posed by `x`.
void asteroid_submit(int shape, xform_t const* x);

// Where a ray from `from` along `dir` (normalised) first strikes the rock `shape`
// posed by `x`, no further than `max`: true with the distance in *dist
// (mesh_raycast). A laser that hits stops there.
bool asteroid_raycast(int shape, xform_t const* x, vec3_t from, vec3_t dir, float max, float* dist);
