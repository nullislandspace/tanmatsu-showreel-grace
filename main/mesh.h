#pragma once
// =====================================================================
//  Showreel  --  static meshes and procedural builders
// ---------------------------------------------------------------------
//  A mesh is a vertex list plus triangles, each with a material index
//  and its own three texture coordinates (so a texture seam never needs
//  a duplicated vertex). Pure data: no engine calls, so the host-side
//  check (tools/meshcheck.c) can build and verify every generator.
//  Submitting a mesh to the scene is mesh_render.h.
//
//  Winding: every builder emits triangles whose (b - a) x (c - a) points
//  OUT of the solid (xform.h), and shares vertices between the faces of
//  one solid, so each solid is a closed, consistently wound surface --
//  which is exactly what the mesh check verifies.
//
//  Builders work in the mesh's own model space, around the z axis where
//  they have one; mesh_transform_from() then moves the part just built
//  into place (e.g. a spoke rotated out to its angle).
// =====================================================================

#include <stdbool.h>
#include <stdint.h>
#include "xform.h"

typedef struct {
    uint16_t a, b, c;
    uint8_t  mat;
    float    uv[3][2];
} mesh_tri_t;

typedef struct {
    vec3_t*     v;
    int         vn, vcap;
    mesh_tri_t* t;
    int         tn, tcap;
    bool        failed;  // an allocation failed; the mesh is incomplete
} mesh_t;

// Start an empty mesh. Storage grows as parts are added (PSRAM on the
// badge).
void mesh_init(mesh_t* m);
void mesh_free(mesh_t* m);

// Raw access. Return the new vertex index, or -1 once the mesh has
// failed (or grown past 65535 vertices).
int  mesh_vert(mesh_t* m, vec3_t p);
void mesh_tri(mesh_t* m, int a, int b, int c, uint8_t mat, float const uv[3][2]);
// Quad a-b-c-d in outward (CCW-seen-from-outside) order, as (a,b,c) +
// (a,c,d). uv[4][2] in the same corner order.
void mesh_quad(mesh_t* m, int a, int b, int c, int d, uint8_t mat, float const uv[4][2]);

// Apply `x` to every vertex from index `first` on (the part just built).
// Rotations and uniform scales keep the winding outward.
void mesh_transform_from(mesh_t* m, int first, xform_t const* x);

// Axis-aligned box lo..hi. Planar mapping per face: one texture repeat
// every `uv_repeat` model units.
void mesh_box(mesh_t* m, vec3_t lo, vec3_t hi, uint8_t mat, float uv_repeat);

// Cylinder around the z axis, radius r, from z0 to z1 (z0 < z1), with
// `sides` facets. Caps optional (a capless end leaves the solid open,
// fine for an end buried inside another part -- the mesh check then
// reports it as open, so only do that on purpose). The side wraps the
// texture round the circumference; caps map planar.
void mesh_cylinder(mesh_t* m, float r, float z0, float z1, int sides, bool cap0, bool cap1, uint8_t mat_side,
                   uint8_t mat_cap, float uv_repeat);

// Ring around the z axis with a rectangular cross-section: inner radius
// r_in, outer r_out, from z0 to z1, `segs` segments. Four surfaces:
// outer (facing away from the axis), inner (facing the axis) and the two
// flat annuli (facing -z at z0, +z at z1).
void mesh_ring(mesh_t* m, float r_in, float r_out, float z0, float z1, int segs, uint8_t mat_outer, uint8_t mat_inner,
               uint8_t mat_face, float uv_repeat);

// Cone/pyramid around the z axis: base circle radius r at z0, apex at z1
// (either direction), `sides` facets, base capped.
void mesh_cone(mesh_t* m, float r, float z0, float z1, int sides, uint8_t mat_side, uint8_t mat_base, float uv_repeat);

// Signed volume of the triangles from index `first_tri` on (positive
// for a closed outward-wound solid).
float mesh_signed_volume(mesh_t const* m, int first_tri);
