#pragma once
// =====================================================================
//  Showreel asset  --  the player's ship (Race the Synth's hull)
// ---------------------------------------------------------------------
//  The mesh is objects/ship_model.h, vendored from
//  tanmatsu-synthracer-grace (generated there from openscad/ship.3mf);
//  nothing here hand-edits it, so a re-export drops straight in.
//
//  Model space, after normalising: nose along +z, roof along +y, the
//  wingspan exactly 1 unit wide (x in -0.5..0.5), centred on the hull's
//  mid-height -- so a scene sizes the ship with xform_t.scale = the
//  wingspan it wants, and spins it about its own centre.
//
//  The gold hull is textured with bare-metal plates (riveted fuselage,
//  brushed wings, gunmetal pods, tread-plate belly); the battery panel,
//  lamps and magnet poles stay flat colours. Two blue Frontier flames
//  trail the engine pods.
// =====================================================================

#include "xform.h"

// Load the plates and build the mesh. Idempotent. Returns the number of
// plates that loaded (0..4); a missing plate leaves its faces gold.
int  player_ship_init(char const* asset_dir);
void player_ship_shutdown(void);

// Draw the ship posed by `x` (model space as above). `throttle` scales
// the flames (0 = off, 1 = nominal cruise); `t` is show time, for the
// flicker. Call after the scene's camera is set.
void player_ship_submit(xform_t const* x, float throttle, double t);

// Model-space bounds of the hull (flames not included).
void player_ship_bounds(vec3_t* lo, vec3_t* hi);

// How far the flames reach behind the hull's rear face at throttle 1
// and full flicker, in model units (for framing).
float player_ship_flame_reach(void);
