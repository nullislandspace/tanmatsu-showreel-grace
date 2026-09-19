#pragma once
// =====================================================================
//  Showreel asset  --  the station's geometry (engine-free)
// ---------------------------------------------------------------------
//  A 2001-style wheel station, in its own model space: axis along z,
//  hub at the origin, the docking port facing +z. World units (1 = about
//  the player's wingspan). Built by station_build_mesh(), which the
//  host-side mesh check (tools/meshcheck.c) runs too.
// =====================================================================

#include "mesh.h"

// Dimensions, for choreography (paths through the gaps, around the rim).
#define STATION_HUB_R       3.0f   // hub cylinder radius
#define STATION_HUB_HALF_Z  2.0f   // hub half-length along the axis
#define STATION_RING_R_IN   21.0f  // ring inner wall radius
#define STATION_RING_R_OUT  24.0f  // ring outer wall radius
#define STATION_RING_HALF_Z 1.25f  // ring half-depth along the axis
#define STATION_SPOKES      8      // spokes, evenly spaced
#define STATION_SPOKE_HALF  0.6f   // spoke half-width (square section)

// Spoke k points along angle k * 2 pi / STATION_SPOKES from +x, in the
// wheel's plane, before the station's spin is applied.
static inline float station_spoke_angle(int k) {
    return (float)k * 6.2831853f / (float)STATION_SPOKES;
}

// Material slots of the mesh.
typedef enum {
    STATION_MAT_HULL = 0,  // hub and flat ring faces: station_hull.png
    STATION_MAT_RING,      // ring walls, with the window band: station_ring.png
    STATION_MAT_SPOKE,     // spokes: plate_gunmetal.png
    STATION_MAT_DOCK,      // the docking port's mouth: flat, dark
    STATION_MAT_COUNT,
} station_mat_t;

void station_build_mesh(mesh_t* m);
