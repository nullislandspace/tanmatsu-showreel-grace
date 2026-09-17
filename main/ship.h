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

// Advance the turntable. dt is seconds, as handed to on_update.
void ship_update(float dt);

// Submit the mesh for this frame: between scene_begin() and
// scene_render(). Emits filled triangles (back faces culled, flat
// region colours -- the engine's light shades them) plus the wireframe
// ridge outline.
void ship_submit(void);

// The hull's centre in world space: the point the turntable spins
// about, and what the app aims the scene light relative to. Any
// pointer may be NULL.
void ship_center(float* x, float* y, float* z);
