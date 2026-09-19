#pragma once
// =====================================================================
//  CraftMiner  --  what the scenes share
// ---------------------------------------------------------------------
//  Everything a scene does is a pure function of its time t; these
//  helpers keep it that way:
//
//    time of day   the sun, the light, the sky and fog colours for a
//                  "day" value (1 midday .. 0 night), for the sunset;
//    digging       a list of blocks to mine, each with its start and
//                  break time: the outline and cracks while it is hit,
//                  the chips and the dropped item after, and the world
//                  edits that remove it;
//    building      a list of blocks to place, each popping in at its
//                  time: the pop, and the edits that add it;
//    the world     one call that sets up everything (world, miner,
//                  effects) for a scene's init/shutdown.
// =====================================================================

#include <stdbool.h>
#include <stdint.h>
#include "craftminer/voxel/voxel_render.h"
#include "xform.h"

// --- Setup -----------------------------------------------------------------------
void cm_init(void);
void cm_shutdown(void);

// --- Time of day ------------------------------------------------------------------
typedef struct {
    vec3_t   sun_dir;   // towards the sun (normalised)
    uint32_t sky_argb;  // the backdrop's sky
    uint32_t fog_argb;  // the horizon: far chunks, the backdrop's ground
    float    light;     // 1 day .. 0 night (clouds, sky bodies)
} cm_daylight_t;

// `day`: 1 is the reel's afternoon, 0 night; in between, a sunset.
cm_daylight_t cm_daylight(float day);
// Set the engine light for it (call from camera(), every frame, since
// the sunset changes it).
void          cm_light(cm_daylight_t const* d);
// The view for it (VOX_VIEW_DEFAULT with the fog colour).
vox_view_t    cm_view(cm_daylight_t const* d);
// The sky, the world (with `edits`) -- call first in submit().
void cm_world_submit(float t, cm_daylight_t const* d, vox_edit_t const* edits, int n_edits, vox_view_t const* view);

// The ground height (feet) at a world point.
float cm_ground(float x, float z);

// --- Digging ------------------------------------------------------------------------
typedef struct {
    float   start, brk;  // hit from `start`, breaks at `brk`
    uint8_t x, y, z;
    uint8_t block;  // what it is (for the chips and the item)
    float   pick;   // the item is picked up this long after the break (0: never)
} cm_dig_t;

// The edits of a dig list (block -> air at each break), for the world.
// `out` holds n entries; the list must be in break order.
void cm_dig_edits(cm_dig_t const* digs, int n, vox_edit_t* out);

// Outline and cracks on the block being hit at t (if any), chips and
// items of the broken ones; `to` is where picked-up items fly (the
// miner's middle).
void cm_dig_submit(cm_dig_t const* digs, int n, float t, vec3_t to);

// The index of the block being hit at t, or -1.
int cm_dig_current(cm_dig_t const* digs, int n, float t);

// The pickaxe's swing at t for a dig list: strokes while a block is hit,
// at rest otherwise (for miner_pose_t.swing or the first-person arm).
float cm_dig_swing(cm_dig_t const* digs, int n, float t);

// --- Building ------------------------------------------------------------------------
typedef struct {
    float   t;  // it starts to pop in
    uint8_t x, y, z;
    uint8_t block;
} cm_place_t;

// The edits of a placement list: each block arrives when its pop ends.
void cm_place_edits(cm_place_t const* places, int n, vox_edit_t* out);
// The pops in progress at t.
void cm_place_submit(cm_place_t const* places, int n, float t);
