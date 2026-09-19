#pragma once
// =====================================================================
//  CraftMiner  --  the block world
// ---------------------------------------------------------------------
//  One block id per cell of a VOX_W x VOX_H x VOX_D grid; block (x, y, z)
//  fills the world-space cube [x, x+1] x [y, y+1] x [z, z+1] (+y up).
//  The terrain is generated once, deterministically, at init: hills of
//  grass over dirt over stone, a lake with sandy shores, coal in the
//  stone, trees -- and the places the scenes need, shaped by hand on top
//  (a meadow, a cliff to mine into, a flat plot for the cabin).
//
//  Scenes change the world over time through an edit list (blocks mined,
//  blocks placed), so the world at scene time t is the generated terrain
//  plus the edits made by t: a pure function of t like everything else
//  in the reel. voxel_world_sync() brings the working grid to that state
//  (cheaply: it only replays or undoes the edits in between) and tells
//  which chunks changed.
// =====================================================================

#include <stdbool.h>
#include <stdint.h>

#define VOX_W 128  // x
#define VOX_H 32   // y (up)
#define VOX_D 128  // z

#define VOX_WATER_LEVEL 9  // the lake's surface is the top of layer 9

typedef enum {
    VB_AIR = 0,
    VB_GRASS,
    VB_DIRT,
    VB_STONE,
    VB_COBBLE,
    VB_SAND,
    VB_WATER,
    VB_LOG,
    VB_PLANKS,
    VB_LEAVES,
    VB_COAL,
    VB_GLASS,       // see-through (cut-out), like the leaves
    VB_TORCH,       // a thin stick; the flame is drawn by the scene
    VB_FLOWER_RED,  // plants: two crossed quads, no collision
    VB_FLOWER_YELLOW,
    VB_TALL_GRASS,
    VB_COUNT
} vox_block_t;

// One change to the world: at scene time `t`, cell (x, y, z) becomes
// `block` (VB_AIR to mine it). Lists are sorted by t.
typedef struct {
    float   t;
    uint8_t x, y, z;
    uint8_t block;
} vox_edit_t;

// --- The places the scenes use (world coordinates, blocks) -------------------
// The meadow: flat grass, no trees, at ground level VOX_MEADOW_Y (the
// top of the grass is y = VOX_MEADOW_Y + 1).
#define VOX_MEADOW_X  40
#define VOX_MEADOW_Z  64
#define VOX_MEADOW_Y  12
// The cabin plot: flat, clear, the same level as the meadow, next to it.
#define VOX_PLOT_X    52
#define VOX_PLOT_Z    44
// The cliff: a stone plateau whose west face (x = VOX_CLIFF_X) is sheer,
// from the meadow's level up to VOX_CLIFF_TOP.
#define VOX_CLIFF_X   62
#define VOX_CLIFF_Z   72
#define VOX_CLIFF_TOP 20
// The lake's middle.
#define VOX_LAKE_X    88
#define VOX_LAKE_Z    34

// Smooth 2D value noise, 0..1, with features about `scale` blocks across
// (the terrain's; the clouds use it too).
float voxel_noise2(float x, float z, float scale, unsigned seed);

// Generate the terrain (once; later calls do nothing). The grid lives in
// PSRAM. False if it could not be allocated.
bool voxel_world_init(void);
void voxel_world_shutdown(void);

// The block at (x, y, z) in the current state: outside the grid, air
// above it and stone everywhere else (so the edges of the world have no
// faces except their tops).
uint8_t        voxel_block(int x, int y, int z);
// Column (x, z) of the current state: VOX_H blocks, bottom up; NULL
// outside the grid.
uint8_t const* voxel_column(int x, int z);
// The generated terrain, without any edits.
uint8_t        voxel_base_block(int x, int y, int z);

// Whether a block is something to stand on (not air, a plant or a
// torch).
bool voxel_solid(uint8_t block);

// The y of the top of the highest solid block in column (x, z), i.e.
// where something standing there has its feet (the water's surface for
// the lake).
int voxel_ground(int x, int z);

// Bring the grid to `edits[0..n)` applied up to (and including) time t.
// Sets dirty[cz * VOX_CHUNKS_X + cx] for every chunk whose faces changed
// (pass NULL to ignore). A different list (another scene) first undoes
// the previous one.
#define VOX_CHUNK    16
#define VOX_CHUNKS_X (VOX_W / VOX_CHUNK)
#define VOX_CHUNKS_Z (VOX_D / VOX_CHUNK)
void voxel_world_sync(vox_edit_t const* edits, int n, float t, bool* dirty);
