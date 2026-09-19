#pragma once
// =====================================================================
//  Showreel  --  ships flying in formation (shared choreography)
// ---------------------------------------------------------------------
//  A formation flies a straight line at constant velocity. Each ship
//  keeps a slot in the formation's own frame and weaves about it: a
//  sideways and an up/down sway, each a sine with its own frequency and
//  phase, and it banks into the sideways acceleration plus a rocking
//  roll. Everything is a pure function of scene time (scene.h).
//
//  The formation's frame: forward along the velocity, right level
//  (perpendicular to world up), up completing it. Slots, weave and
//  camera offsets are given in that frame, so the same slot means the
//  same place whichever way the formation flies.
//
//  Used by marauder_pursuit (and the later marauder scenes).
// =====================================================================

#include "xform.h"

typedef struct {
    vec3_t origin;        // centre at t = 0
    vec3_t vel;           // world units per second; not vertical
    float  bank_per_acc;  // roll (rad) per unit of sideways acceleration
    float  bank_max;      // cap on that roll
} formation_t;

// A ship's sway about its slot. A negative amplitude starts the sway the
// other way.
typedef struct {
    float lat_amp, lat_hz, lat_ph;     // sideways (along the formation's right)
    float vert_amp, vert_hz, vert_ph;  // up / down
    float rock_amp, rock_hz, rock_ph;  // extra roll on top of the bank
} weave_t;

typedef struct {
    vec3_t  offset;  // formation frame: x right, y up, z forward (negative: behind)
    weave_t weave;
} slot_t;

// The formation's centre at t.
vec3_t formation_centre(formation_t const* f, float t);

// A direction or offset given in the formation's frame, in world axes.
vec3_t formation_to_world(formation_t const* f, vec3_t local);

// Where slot `s` is at t, without its weave (for cameras that ride with
// the formation while the ships move about in the frame).
vec3_t formation_slot_pos(formation_t const* f, slot_t const* s, float t);

// Position, velocity and acceleration of the ship in slot `s` at t.
void formation_motion(formation_t const* f, slot_t const* s, float t, vec3_t* p, vec3_t* v, vec3_t* a);

// Its full pose: facing its velocity, banked into the weave and rocking;
// `scale` is the model's scale (e.g. its span).
xform_t formation_pose(formation_t const* f, slot_t const* s, float t, float scale);
