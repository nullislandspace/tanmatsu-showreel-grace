#pragma once
// =====================================================================
//  Showreel asset  --  the planet base (scenes 2, 4, 5)
// ---------------------------------------------------------------------
//  The landing pad and the works behind it, the textured apron, the far
//  ridge and the burning flare stacks (planet_base_mesh.h for the layout
//  and units). The sky and the ground beyond the apron are the PPA
//  backdrop: a scene on this set uses planet_base_backdrop().
// =====================================================================

#include "backdrop.h"
#include "xform.h"

// Dusty amber haze over ochre ground. The apron is drawn unlit (a flat
// plane takes one shade anyway), and the PPA's flat ground beyond it is
// the apron texture's average colour, so the edge melts away whatever the
// scene light. PLANET_GROUND_ARGB is only the fallback without the texture.
#define PLANET_SKY_ARGB    0xFFB89A7Cu
#define PLANET_GROUND_ARGB 0xFF6A5638u

// Build the meshes and fetch the textures. Idempotent.
void planet_base_init(void);
void planet_base_shutdown(void);

// Draw the whole set at show time `t` (the flames flicker). Call after
// the scene's camera is set.
void planet_base_submit(double t);

// The backdrop for this set: PLANET_SKY_ARGB over the apron texture's
// average colour (valid after planet_base_init). For a scene's
// backdrop_at.
backdrop_t const* planet_base_backdrop(void);

// Centre of the pad's top surface: where a ship touches down.
vec3_t planet_base_pad_centre(void);
