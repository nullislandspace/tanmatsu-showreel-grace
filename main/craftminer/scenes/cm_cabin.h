#pragma once
// =====================================================================
//  CraftMiner  --  the log cabin (built in cm_building, finished in
//  cm_nightfall)
// ---------------------------------------------------------------------
//  On the plot next to the meadow: 7 x 5 blocks, walls of planks three
//  high between log corner posts, a doorway in the south wall (facing
//  the meadow), glass windows in the other three walls, a stepped plank
//  roof overhanging by a block, and two torches either side of the door.
// =====================================================================

#include "craftminer/scenes/cm_common.h"

#define CABIN_X0     (VOX_PLOT_X - 3)  // the walls' outer faces: x0 .. x1, z0 .. z1
#define CABIN_X1     (VOX_PLOT_X + 3)
#define CABIN_Z0     (VOX_PLOT_Z - 2)
#define CABIN_Z1     (VOX_PLOT_Z + 2)
#define CABIN_FLOOR  (VOX_MEADOW_Y + 1)  // the lowest wall blocks
#define CABIN_DOOR_X VOX_PLOT_X
#define CABIN_MAX    200

// The cabin's blocks in building order (all t = 0): the four corner
// posts' bottom blocks first, then the walls layer by layer, the roof,
// the torches last. Returns the count.
int cm_cabin_blocks(cm_place_t* out, int max);

// The torches (cells), for their flames.
#define CABIN_TORCHES 2
extern int const CABIN_TORCH[CABIN_TORCHES][3];
