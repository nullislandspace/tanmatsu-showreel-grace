#pragma once
// =====================================================================
//  Showreel  --  the second solar system (scenes 9-11)
// ---------------------------------------------------------------------
//  Where the hero ship ambushes the marauders (D-28): its own sky (the
//  starfield turned, so the band crosses it elsewhere), a whiter, harsher
//  sun from -x, and a banded gas giant hanging far off ahead and
//  to the left of the ships' flight direction (-z). The scenes share it
//  so the three read as one place.
// =====================================================================

#include "xform.h"

// Fetch the planet and the stars. Idempotent.
void system2_init(void);
void system2_shutdown(void);

// The system's sun (se_light_set); call from a scene's enter.
void system2_light(void);

// The sky -- turned stars and the gas giant -- for the current camera, at
// show time t (the giant turns, very slowly). Call first in submit, after
// the camera is set.
void system2_submit_sky(float t);
