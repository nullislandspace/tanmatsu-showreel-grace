#pragma once
// =====================================================================
//  Showreel asset  --  asteroid geometry (engine-free)
// ---------------------------------------------------------------------
//  A lumpy rock: an icosphere (mesh_blob, subdivided twice: 320
//  triangles) whose radius wanders with direction -- a sum of a few
//  seeded waves over the sphere -- so every seed gives a different rock.
//  Model space: mean radius 1 round the origin.
// =====================================================================

#include "mesh.h"

// `lumpiness`: how far the surface strays from the sphere, as a
// fraction of the radius (0.25 is a battered potato, 0.1 nearly round).
void asteroid_build_mesh(mesh_t* m, unsigned seed, float lumpiness);
