#pragma once
// =====================================================================
//  Showreel asset  --  explosions and laser impacts (D-29)
// ---------------------------------------------------------------------
//  Pure functions of time since the event, and no transparency:
//
//  explosion  a fireball -- a white-hot core inside an orange shell, two
//             lumpy blobs of flat emissive colour that swell, flicker,
//             turn and die down, darkening into the backdrop -- and
//             sparks: short lines flying out and cooling. A ship's
//             wreckage is the ship's own business (marauder_submit_debris).
//  impact     where a laser hits: a spray of short hot lines off the
//             surface for a moment.
// =====================================================================

#include "xform.h"

#define EXPLOSION_SECS 2.0f  // fireball and sparks are gone by then
#define IMPACT_SECS    0.2f

// Build the fireball blobs. Idempotent.
void explosion_init(void);
void explosion_shutdown(void);

// The explosion that went off at `t_explode` at `centre` (moving with
// `drift`, units per second: the ship's momentum), for a ship `size`
// units across, at time `t`. Nothing before t_explode or after it is over.
void explosion_submit(vec3_t centre, vec3_t drift, float size, float t, float t_explode, unsigned seed);

// A laser hit at `at` on a surface facing `normal`, at time `t` for a hit
// at `t_hit`; `size` scales the spray.
void impact_submit(vec3_t at, vec3_t normal, float size, float t, float t_hit, unsigned seed);
