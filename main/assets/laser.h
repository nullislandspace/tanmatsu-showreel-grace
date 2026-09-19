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

#define LASER_RED  0xFFFF3020u
#define LASER_BLUE 0xFF40A0FFu

typedef struct {
    float    duration;  // seconds a shot stays lit
    float    range;     // longest beam, world units: far enough to leave any shot's frame
    uint32_t argb;
} laser_style_t;

// The marauders' guns: red.
extern laser_style_t const LASER_STYLE_MARAUDER;
// The hero ship's guns: blue, otherwise the same.
extern laser_style_t const LASER_STYLE_PLAYER;

// True while the shot fired at `t_fire` is lit at time `t`.
static inline bool laser_lit(float t, float t_fire, laser_style_t const* style) {
    return t >= t_fire && t < t_fire + style->duration;
}

// Draw the beam of the shot fired at `t_fire`, if it is lit at `t`: from
// `muzzle` (the gun's position NOW) towards `target`, at most
// style->range long.
void laser_submit_beam(vec3_t muzzle, vec3_t target, float t, float t_fire, laser_style_t const* style);

// A shot fired straight ahead, that hits nothing: from `muzzle` along
// `dir` (the gun's forward, normalised) the full style->range, so it
// runs out of the frame instead of stopping in mid-air.
void laser_submit_ray(vec3_t muzzle, vec3_t dir, float t, float t_fire, laser_style_t const* style);
