#pragma once
// =====================================================================
//  Showreel asset  --  the marauder's geometry (engine-free)
// ---------------------------------------------------------------------
//  One pirate fighter type, deliberately unlike the player's ship: a
//  slim hexagonal fuselage with a raised cockpit, swept delta wings with
//  drooping tips, a fin on each wingtip, two underslung engine nacelles
//  and a gun under each wing root. Model space as the player's ship:
//  nose along +z, roof along +y, wingspan 1 unit.
//
//  Built from lofts, boxes and cylinders (mesh.h); the host-side mesh
//  check runs marauder_build_mesh() too.
// =====================================================================

#include "mesh.h"

typedef enum {
    MARAUDER_MAT_PAINT = 0,  // hull: the livery texture
    MARAUDER_MAT_METAL,      // nacelles, guns: plate_gunmetal.png
    MARAUDER_MAT_CANOPY,     // cockpit glass: flat
    MARAUDER_MAT_NOZZLE,     // engine mouths: flat, dark
    MARAUDER_MAT_COUNT,
} marauder_mat_t;

// Engine nozzles (model space): centres of the nacelles' rear faces.
#define MARAUDER_NOZZLE_X 0.17f  // +- for the two nacelles
#define MARAUDER_NOZZLE_Y (-0.03f)
#define MARAUDER_NOZZLE_Z (-0.52f)
#define MARAUDER_NOZZLE_R 0.045f  // flame base radius (nacelle: 0.055)
// Gun muzzles (model space), +-x for the two guns.
#define MARAUDER_GUN_X    0.12f
#define MARAUDER_GUN_Y    (-0.06f)
#define MARAUDER_GUN_Z    0.32f

void marauder_build_mesh(mesh_t* m);
