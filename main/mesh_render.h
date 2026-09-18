#pragma once
// =====================================================================
//  Showreel  --  submitting meshes to the scene
// ---------------------------------------------------------------------
//  The materials are handed over at submit time, not stored in the mesh,
//  so one mesh can be drawn in several liveries (e.g. two marauders of
//  the same type in different paint).
// =====================================================================

#include <stdint.h>
#include "mesh.h"
#include "synthengine3d.h"

typedef struct {
    se_texture_t const* tex;    // NULL: flat `argb`
    uint32_t            argb;   // flat colour, and the fallback if tex failed to load
    uint32_t            flags;  // SE_TRI_* (e.g. SE_TRI_EMISSIVE)
} mesh_mat_t;

// Transform `m` by `x`, cull faces turned away from the current camera
// eye, and submit the rest (scene_textured_tri / scene_tri). A triangle
// whose material index is >= mat_n is skipped. Call after the scene's
// camera is set.
void mesh_submit(mesh_t const* m, xform_t const* x, mesh_mat_t const* mats, int mat_n);

// The same for one part of `m` (mesh.h: one builder call's solid), e.g.
// a fragment of an exploding ship, each with its own `x`. Only that
// part's vertices are transformed.
void mesh_submit_part(mesh_t const* m, int part, xform_t const* x, mesh_mat_t const* mats, int mat_n);

// World-space vertices of the last mesh_submit() (valid until the next
// one), e.g. for drawing an outline over it. After mesh_submit_part(),
// only that part's entries are current.
vec3_t const* mesh_last_world(void);
