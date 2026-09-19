#pragma once
// =====================================================================
//  Showreel asset  --  space dust
// ---------------------------------------------------------------------
//  Stars are infinitely far away, so they cannot show speed. Dust can:
//  motes fixed in space that a moving camera streams through. They fill
//  a box round the camera -- each mote's position wraps modulo the box,
//  so the cloud never runs out however far the camera flies -- and are
//  faint, so they read as dust rather than stars. Optionally each is
//  drawn as a streak (a slow shutter): from where it is to where it
//  appeared `streak` seconds ago, which, for a camera moving at v, is
//  v * streak further along -- so the streaks point back at the spot
//  the camera is flying towards.
// =====================================================================

#include "xform.h"

// Seed the motes. Idempotent.
void space_dust_init(void);

// Draw the dust round the current camera. `cam_vel` is the camera's
// velocity (units per second); `streak` the exposure in seconds (0: dots).
void space_dust_submit(vec3_t cam_vel, float streak);
