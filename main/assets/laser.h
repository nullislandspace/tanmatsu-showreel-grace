#pragma once
// =====================================================================
//  Showreel asset  --  lasers
// ---------------------------------------------------------------------
//  A laser shot is a beam, not a projectile: for a fraction of a second
//  a red line lights up from the gun to where it is aimed (scene_line,
//  so it is unlit and nothing shades it). It is drawn from the gun's
//  position at the current instant, so it always starts at the turret
//  however the ship moves. A pure function of time, like everything
//  else: a scene keeps a fire schedule and asks, per shot, whether that
//  shot is still lit.
// =====================================================================

#include <stdbool.h>
#include <stdint.h>
#include "xform.h"

#define LASER_RED 0xFFFF3020u

typedef struct {
    float    duration;  // seconds a shot stays lit
    float    range;     // longest beam, world units (it stops at the target if nearer)
    uint32_t argb;
} laser_style_t;

// The marauders' guns.
extern laser_style_t const LASER_STYLE_MARAUDER;

// True while the shot fired at `t_fire` is lit at time `t`.
static inline bool laser_lit(float t, float t_fire, laser_style_t const* style) {
    return t >= t_fire && t < t_fire + style->duration;
}

// Draw the beam of the shot fired at `t_fire`, if it is lit at `t`: from
// `muzzle` (the gun's position NOW) towards `target`, at most
// style->range long.
void laser_submit_beam(vec3_t muzzle, vec3_t target, float t, float t_fire, laser_style_t const* style);
