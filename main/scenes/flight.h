#pragma once
// =====================================================================
//  Showreel  --  ships flying along paths (shared choreography)
// ---------------------------------------------------------------------
//  A ship on a Catmull-Rom path (xform.h) faces along its velocity and
//  banks into the turn: roll proportional to the sideways acceleration,
//  capped. Pure functions of time, like everything the scenes use.
// =====================================================================

#include "xform.h"

// Sideways acceleration (units/s^2) -> roll: the flyby's values.
#define FLIGHT_BANK_PER_ACC 0.06f
#define FLIGHT_BANK_MAX     0.9f

// The ship on `path` at t, `scale` its span, `extra_roll` on top of the
// bank (a barrel roll, a wobble).
xform_t flight_pose(path_t const* path, float t, float scale, float extra_roll);

// A ship on a straight line through `at_t0` at time t0 with velocity
// `vel`: position, and a pose facing along the line (no bank).
xform_t flight_pose_line(vec3_t at_t0, vec3_t vel, float t0, float t, float scale, float roll);
