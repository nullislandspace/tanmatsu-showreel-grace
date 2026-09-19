#pragma once
// =====================================================================
//  CraftMiner  --  drawing the block world
// ---------------------------------------------------------------------
//  The world (voxel_world.h) is cut into VOX_CHUNK x VOX_CHUNK columns,
//  each meshed four ways (voxel_mesh.h). Every frame, per chunk, by the
//  distance from the eye to the chunk's nearest point:
//
//    - behind the camera or outside its view: skipped;
//    - within `fancy_dist`: everything, textured -- canopies you see
//      into (cut-out leaves), plants;
//    - within `tex_dist`: textured, canopies opaque, no plants;
//    - within `coarse_dist`: flat colours (each texture's mean), which
//      fill three to four times faster than textures
//      (devdocs/performance.md), fading towards `fog_argb` -- cheap fog;
//    - within `draw_dist`: the same at half resolution (2 x 2 x 2 blocks
//      a cell), about a quarter of the triangles;
//    - further: nothing; the backdrop's ground (in the fog colour) is
//      the world beyond.
//
//  The per-frame lists are the limit: a view must stay within the
//  engine's caps (4096 flat, 1024 textured triangles).
//
//  Chunks an edit changed are meshed again when next drawn; the result
//  depends only on the world's state, so the same t always draws the
//  same frame.
// =====================================================================

#include <stdbool.h>
#include <stdint.h>
#include "craftminer/voxel/voxel_world.h"
#include "mesh_render.h"
#include "synthengine3d.h"

typedef struct {
    float    fancy_dist;   // chunks nearer than this (closest point): everything
    float    tex_dist;     // ... textured
    float    coarse_dist;  // ... flat at full resolution; beyond, half resolution
    float    draw_dist;    // chunks further than this are not drawn
    float    fog0, fog1;   // flat colours fade from none at fog0 to all fog at fog1
    uint32_t fog_argb;     // what they fade to: the horizon's colour
    // Chunks overlapping this box (x0, z0, x1, z1 in blocks) are drawn
    // textured whatever their distance -- for something the shot is
    // about (the title's letters). Off while x1 <= x0 (the default).
    float    tex_box[4];
} vox_view_t;

// The day sky, and the view most shots use.
#define VOX_SKY_ARGB 0xFF8EC4F0u
#define VOX_VIEW_DEFAULT                                                                                               \
    {                                                                                                                  \
        .fancy_dist = 8.0f, .tex_dist = 18.0f, .coarse_dist = 36.0f, .draw_dist = 64.0f, .fog0 = 24.0f, .fog1 = 72.0f, \
        .fog_argb = VOX_SKY_ARGB                                                                                       \
    }

// The world, its textures and meshes (once; every scene that uses it
// calls this, and voxel_render_shutdown() when it is done).
bool voxel_render_init(void);
void voxel_render_shutdown(void);

// Bring the world to `edits` at t (voxel_world_sync), mesh again what
// changed, and submit what the current camera sees. Call after the
// camera is set.
void voxel_render_submit(vox_edit_t const* edits, int n, float t, vox_view_t const* view);

// A material's texture (NULL if it failed to load) and mean colour: for
// the things drawn outside the world (a dropped block, break particles).
se_texture_t const* voxel_mat_tex(int mat);
uint32_t            voxel_mat_argb(int mat);

// The three materials of voxel_build_cube() (top, sides, bottom) for a
// block (vox_block_t), textured.
void voxel_cube_mats(uint8_t block, mesh_mat_t out[3]);
