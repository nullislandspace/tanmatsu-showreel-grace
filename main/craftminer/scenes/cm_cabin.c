// =====================================================================
//  CraftMiner  --  the log cabin (see cm_cabin.h)
// =====================================================================

#include "craftminer/scenes/cm_cabin.h"

int const CABIN_TORCH[CABIN_TORCHES][3] = {
    {CABIN_DOOR_X - 1, CABIN_FLOOR, CABIN_Z0 - 1},
    {CABIN_DOOR_X + 1, CABIN_FLOOR, CABIN_Z0 - 1},
};

typedef struct {
    cm_place_t* out;
    int         n, max;
} list_t;

static void add(list_t* l, int x, int y, int z, uint8_t b) {
    if (l->n < l->max) l->out[l->n++] = (cm_place_t){0.0f, (uint8_t)x, (uint8_t)y, (uint8_t)z, b};
}

static bool corner(int x, int z) {
    return (x == CABIN_X0 || x == CABIN_X1) && (z == CABIN_Z0 || z == CABIN_Z1);
}

// A wall block's kind: glass in the three windows (the middle of each
// wall but the front, at the second layer), air in the doorway.
static uint8_t wall(int x, int y, int z) {
    int const layer = y - CABIN_FLOOR;
    if (corner(x, z)) return VB_LOG;
    if (z == CABIN_Z0 && x == CABIN_DOOR_X && layer < 2) return VB_AIR;
    bool const mid = (z == CABIN_Z1 && x == CABIN_DOOR_X) || ((x == CABIN_X0 || x == CABIN_X1) && z == VOX_PLOT_Z);
    if (mid && layer == 1) return VB_GLASS;
    return VB_PLANKS;
}

int cm_cabin_blocks(cm_place_t* out, int max) {
    list_t l = {out, 0, max};
    // The corner posts' feet.
    add(&l, CABIN_X0, CABIN_FLOOR, CABIN_Z0, VB_LOG);
    add(&l, CABIN_X1, CABIN_FLOOR, CABIN_Z0, VB_LOG);
    add(&l, CABIN_X1, CABIN_FLOOR, CABIN_Z1, VB_LOG);
    add(&l, CABIN_X0, CABIN_FLOOR, CABIN_Z1, VB_LOG);
    // The walls, a layer at a time, round the cabin: front (west to
    // east), east side, back (east to west), west side.
    for (int y = CABIN_FLOOR; y < CABIN_FLOOR + 3; y++) {
        for (int x = CABIN_X0; x <= CABIN_X1; x++) {
            if (y == CABIN_FLOOR && corner(x, CABIN_Z0)) continue;
            uint8_t const b = wall(x, y, CABIN_Z0);
            if (b != VB_AIR) add(&l, x, y, CABIN_Z0, b);
        }
        for (int z = CABIN_Z0 + 1; z <= CABIN_Z1; z++) {
            if (y == CABIN_FLOOR && corner(CABIN_X1, z)) continue;
            add(&l, CABIN_X1, y, z, wall(CABIN_X1, y, z));
        }
        for (int x = CABIN_X1 - 1; x >= CABIN_X0; x--) {
            if (y == CABIN_FLOOR && corner(x, CABIN_Z1)) continue;
            add(&l, x, y, CABIN_Z1, wall(x, y, CABIN_Z1));
        }
        for (int z = CABIN_Z1 - 1; z > CABIN_Z0; z--) add(&l, CABIN_X0, y, z, wall(CABIN_X0, y, z));
    }
    // The roof: four layers, each a block in from the one below, the
    // first overhanging the walls by one.
    for (int k = 0; k < 4; k++) {
        int const y = CABIN_FLOOR + 3 + k;
        for (int z = CABIN_Z0 - 1 + k; z <= CABIN_Z1 + 1 - k; z++) {
            for (int x = CABIN_X0 - 1 + k; x <= CABIN_X1 + 1 - k; x++) add(&l, x, y, z, VB_PLANKS);
        }
    }
    for (int i = 0; i < CABIN_TORCHES; i++) add(&l, CABIN_TORCH[i][0], CABIN_TORCH[i][1], CABIN_TORCH[i][2], VB_TORCH);
    return l.n;
}
