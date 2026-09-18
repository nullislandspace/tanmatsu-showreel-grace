#pragma once
// =====================================================================
//  Showreel asset  --  laser bolts
// ---------------------------------------------------------------------
//  A bolt is a short red streak (scene_line, so unlit and never hidden
//  by its own glow) travelling in a straight line. It is a pure function
//  of time: where it is follows from where and when it was fired, so a
//  scene only needs a fire schedule, no bolt state.
// =====================================================================

#include <stdint.h>
#include "xform.h"

#define LASER_RED 0xFFFF3020u

typedef struct {
    float    speed;     // world units per second
    float    length;    // streak length, world units
    float    lifetime;  // seconds until it fades out of existence
    uint32_t argb;
} laser_style_t;

// The marauders' bolts.
extern laser_style_t const LASER_STYLE_MARAUDER;

// Draw a bolt fired from `muzzle` along the unit vector `dir`, `age`
// seconds ago. Nothing is drawn before it is fired (age < 0) or once
// its lifetime is over.
void laser_submit_bolt(vec3_t muzzle, vec3_t dir, float age, laser_style_t const* style);
