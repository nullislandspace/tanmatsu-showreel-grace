#pragma once
// =====================================================================
//  Showreel  --  the Race the Synth ship, on a turntable
// ---------------------------------------------------------------------
//  One non-trivial mesh (169 verts / 306 tris / 84 outline edges),
//  spinning in the middle of an otherwise black screen. The first reel
//  item, and the thing that proves the scene pipeline handles real
//  geometry rather than one triangle.
// =====================================================================

#include <stdbool.h>

// Load the hull's plate textures from `asset_dir` (the app's install
// directory) into internal SRAM, and work out which plate each gold
// face gets and where on it. Call once from on_init, before the first
// ship_submit. Returns the number of plates that loaded; any plate that
// failed leaves its faces in the flat gold, so the ship always draws.
int ship_init(char const* asset_dir);

// Unload the plate textures. Between frames only (se_texture_unload).
void ship_shutdown(void);

// Advance the turntable. dt is seconds, as handed to on_update.
void ship_update(float dt);

// Submit the mesh for this frame: between scene_begin() and
// scene_render(). The gold hull goes in as textured triangles (metal
// plates), the other regions as flat ones, all with back faces culled
// and all shaded by the engine's light; then the two engine flames,
// emissive so the light leaves them bright; then (if enabled) the
// wireframe ridge outline.
void ship_submit(void);

// The hull's centre in world space: the point the turntable spins
// about, and what the app aims the scene light relative to. Any
// pointer may be NULL.
void ship_center(float* x, float* y, float* z);
