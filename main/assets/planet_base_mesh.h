#pragma once
// =====================================================================
//  Showreel asset  --  the planet base's geometry (engine-free)
// ---------------------------------------------------------------------
//  The set of scenes 2, 4 and 5: a landing pad on an industrial planet.
//  World space, 1 unit = the hero ship's wingspan: the ground is y = 0,
//  the pad is centred on the origin, the works stand behind it (+z).
//
//    structures  pad (with its markings), two corrugated halls, three
//                storage tanks, two chimneys, a pipe rack and two flare
//                stacks: closed solids (meshcheck)
//    apron       the textured ground round the pad, a grid of quads
//                facing up: an open surface; beyond it the PPA paints
//                the ground (backdrop.h)
//    ridge       a far ring of hills on the horizon, facing the base:
//                an open surface, flat and hazy
// =====================================================================

#include "mesh.h"

typedef enum {
    BASE_MAT_PAD = 0,  // pad.png
    BASE_MAT_MARK,     // pad markings: flat, emissive yellow
    BASE_MAT_WALL,     // industrial_wall.png
    BASE_MAT_TANK,     // station_hull.png (off-white)
    BASE_MAT_METAL,    // plate_gunmetal.png
    BASE_MAT_APRON,    // ground.png
    BASE_MAT_RIDGE,    // flat, emissive haze
    BASE_MAT_COUNT,
} base_mat_t;

// The pad: its top surface is y = BASE_PAD_TOP over |x|, |z| <= half.
#define BASE_PAD_HALF 3.0f
#define BASE_PAD_TOP  0.25f

// Flare stacks: where each flame burns (the top of the stack).
#define BASE_FLARES 2
extern float const BASE_FLARE_TOP[BASE_FLARES][3];
#define BASE_FLARE_R 0.35f  // stack radius

void planet_base_build_structures(mesh_t* m);
void planet_base_build_apron(mesh_t* m);
void planet_base_build_ridge(mesh_t* m);
