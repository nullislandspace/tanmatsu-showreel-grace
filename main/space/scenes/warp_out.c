// =====================================================================
//  Showreel scene  --  warp_out (scene 8)
// ---------------------------------------------------------------------
//  The hero ship gets away (D-28): it pulls ahead of the marauders, still
//  under their fire, and jumps to hyperspace (warp.h) -- a stretch along
//  its flight line, a flash, gone. The marauders fly on for a few
//  seconds, then jump after it, one after the other. The station, small,
//  far off.
//
//  The marauders fire along their noses, turned onto just past the hero:
//  close misses, which run on out of the frame (a beam never runs through
//  the hero; one that meets it stops on its hull).
//
//  Two shots, both riding with the formation. The chase: off to the side
//  (the sunlit one), looking across it -- so the beams cross the screen
//  and leave it, instead of shrinking to a point ahead as they would seen
//  from behind; each marauder's misses pass just above (green) or below
//  (yellow) the hero, never in front of or behind it, where they would
//  look like hits. The guns stop before the cut to the warp: behind the
//  pair, looking past them along the flight line, so the stretch and the
//  flash of each warp are in view.
// =====================================================================

#include <math.h>
#include "camera.h"
#include "space/assets/laser.h"
#include "space/assets/marauder.h"
#include "space/assets/player_ship.h"
#include "space/assets/starfield.h"
#include "space/assets/station.h"
#include "space/assets/warp.h"
#include "space/scenes/formation.h"
#include "space/space.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define SCENE_SECS    8.0f
#define SHOT_WARP     2.4f  // chase -> warp camera
#define T_HERO_WARP   3.0f  // the stretch starts
#define T_GREEN_WARP  5.3f
#define T_YELLOW_WARP 5.9f
#define FIRE1         2.2f  // the marauders' guns, from t = 0, alternating; the last one out before the cut
#define FIRE_INTERVAL 0.2f
#define AIM_TURN      0.4f  // seconds to bring a nose off its aim once the hero is gone

// --- Ships ------------------------------------------------------------------
#define HERO_SPAN     1.0f
#define MARAUDER_SPAN 0.9f
// The hero: ahead of the formation and pulling away.
#define HERO_AHEAD    2.5f  // units ahead of the formation's centre at t = 0 ...
#define HERO_GAIN     1.2f  // ... gaining this many units a second
// The marauders' aim: just past the hero (half-span 0.5), this far off
// the line of fire.
#define MISS_MIN      0.95f
#define MISS_MAX      1.25f

// Along -z at 14 units/s, as in the flyby.
static formation_t const FORMATION = {
    .origin = {0.0f, 0.0f, 0.0f}, .vel = {0.0f, 0.0f, -14.0f}, .bank_per_acc = 0.06f, .bank_max = 0.6f};

// Side by side, the yellow one a little back (marauder_approach's pair).
static slot_t const SLOT_GREEN = {
    .offset = {-0.9f, 0.15f, 0.0f},
    .weave  = {0.2f, 0.29f, 0.7f, 0.1f, 0.41f, 0.2f, 0.08f, 0.7f, 1.2f},
};
static slot_t const SLOT_YELLOW = {
    .offset = {0.9f, -0.1f, -0.8f},
    .weave  = {-0.18f, 0.35f, 2.4f, 0.12f, 0.47f, 1.9f, 0.1f, 0.85f, 0.3f},
};

// --- Background -------------------------------------------------------------
// The station, small, far off beyond the chase, high in the frame.
#define STATION_POS \
    { 600.0f, 170.0f, -380.0f }
#define STATION_SPIN 0.15f

// --- Camera -------------------------------------------------------------------
// In the formation's frame (+x is the sunlit side). The chase: out to
// the side and a little above, level with the space between the pair
// and the hero, looking across at it.
#define CAM_OFFSET \
    { 6.5f, 1.4f, -0.2f }
#define CAM_LOOK \
    { 0.0f, 0.2f, 2.3f }
// The warp: behind the pair and a little above, looking ahead past them.
#define CAM_WARP_OFFSET \
    { -0.3f, 1.0f, -4.2f }
#define CAM_WARP_LOOK \
    { 0.2f, 0.4f, 10.0f }

// The hero's own pose (no warp): straight ahead of the formation,
// swaying a little.
static xform_t hero_base(float t) {
    vec3_t const local =
        v3(0.1f + 0.25f * sinf(0.9f * t), 0.55f + 0.15f * sinf(1.3f * t + 0.5f), HERO_AHEAD + HERO_GAIN * t);
    vec3_t const pos = v3_add(formation_centre(&FORMATION, t), formation_to_world(&FORMATION, local));
    mat3_t const r   = mat3_from_fwd_up(v3_norm(FORMATION.vel), v3(0.0f, 1.0f, 0.0f), 0.12f * sinf(0.9f * t + 1.2f));
    return (xform_t){r, pos, HERO_SPAN};
}

// A ship's own pose (no warp) as a function of time.
typedef xform_t (*pose_fn_t)(float t);

// How far a marauder has its nose on its aim: fully while it fires,
// back along its flight once the hero is gone.
static float aim_weight(float t) {
    return 1.0f - smoothstep(FIRE1, FIRE1 + AIM_TURN, t);
}

// Where marauder `yellow` aims at t: just past the hero, MISS_MIN ..
// MISS_MAX off the line of fire -- above it (green) or below (yellow),
// swaying a little either side of straight up or down.
static vec3_t raider_aim(bool yellow, vec3_t from, float t) {
    vec3_t const hero  = hero_base(fminf(t, T_HERO_WARP)).pos;
    vec3_t const los   = v3_norm(v3_sub(hero, from));
    vec3_t const side  = v3_norm(v3_cross(los, v3(0.0f, 1.0f, 0.0f)));
    vec3_t const up    = v3_cross(side, los);
    float const  phase = yellow ? 2.1f : 0.0f;
    float const  a     = (yellow ? -1.5707963f : 1.5707963f) + 0.45f * sinf(1.6f * t + phase);
    float const  r     = MISS_MIN + (MISS_MAX - MISS_MIN) * (0.5f + 0.5f * sinf(2.3f * t + 2.0f * phase));
    return v3_add(hero, v3_add(v3_scale(side, r * cosf(a)), v3_scale(up, r * sinf(a))));
}

// A marauder in its slot, the whole ship (bank and all) turned so its
// nose is on its aim.
static xform_t raider_base(slot_t const* slot, bool yellow, float t) {
    xform_t      x    = formation_pose(&FORMATION, slot, t, MARAUDER_SPAN);
    vec3_t const f0   = v3_norm(x.r.fwd);
    vec3_t const f    = v3_norm(v3_lerp(f0, v3_norm(v3_sub(raider_aim(yellow, x.pos, t), x.pos)), aim_weight(t)));
    vec3_t const axis = v3_cross(f0, f);
    float const  s    = v3_len(axis);
    if (s > 1e-6f) {
        mat3_t const turn = mat3_axis_angle(v3_scale(axis, 1.0f / s), atan2f(s, v3_dot(f0, f)));
        x.r               = mat3_mul(&turn, &x.r);
    }
    return x;
}

static xform_t green_base(float t) {
    return raider_base(&SLOT_GREEN, false, t);
}

static xform_t yellow_base(float t) {
    return raider_base(&SLOT_YELLOW, true, t);
}

// The flash where the ship posed by `base` (at warp time `tw`) vanishes.
static void submit_flash(pose_fn_t base, float tw, float size, float t, unsigned seed) {
    xform_t const at_flash = base(tw + WARP_STRETCH_SECS);
    warp_submit_flash(warp_point(&at_flash, WARP_OUT), size, t, tw, WARP_OUT, seed);
}

// The shot lit at t, if any: the two ships in turn, each from alternate
// guns, straight along its nose -- on out of the frame, or onto the
// hero's hull if it meets it.
static void submit_fire(float t) {
    if (t > FIRE1 + LASER_STYLE_MARAUDER.duration) return;
    int const   k  = (int)floorf(t / FIRE_INTERVAL);
    float const tf = (float)k * FIRE_INTERVAL;
    if (tf > FIRE1 || !laser_lit(t, tf, &LASER_STYLE_MARAUDER)) return;
    xform_t const ship = (k & 1) ? yellow_base(t) : green_base(t);
    vec3_t const  gun  = marauder_gun(&ship, (k >> 1) & 1);
    vec3_t const  dir  = v3_norm(ship.r.fwd);
    float         d    = LASER_STYLE_MARAUDER.range;
    xform_t const base = hero_base(t);
    xform_t       hero;
    if (warp_pose(&base, t, T_HERO_WARP, WARP_OUT, &hero)) player_ship_raycast(&hero, gun, dir, d, &d);
    laser_submit_beam(gun, v3_add(gun, v3_scale(dir, d)), t, tf, &LASER_STYLE_MARAUDER);
}

static void warp_out_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    marauder_init();
    station_init();
    starfield_init();
}

static void warp_out_shutdown(void) {
    player_ship_shutdown();
    marauder_shutdown();
    station_shutdown();
}

static void warp_out_enter(void) {
    // The flyby's sun.
    se_light_set(&(se_light_t){.x = -500.0f, .y = 350.0f, .z = -60.0f, .brightness = 0.8f, .two_sided = true});
}

static void warp_out_camera(double td) {
    float const  t      = (float)td;
    vec3_t const centre = formation_centre(&FORMATION, t);
    bool const   chase  = t < SHOT_WARP;
    vec3_t const eye    = chase ? (vec3_t)CAM_OFFSET : (vec3_t)CAM_WARP_OFFSET;
    vec3_t const look   = chase ? (vec3_t)CAM_LOOK : (vec3_t)CAM_WARP_LOOK;
    camera_look_at(v3_add(centre, formation_to_world(&FORMATION, eye)),
                   v3_add(centre, formation_to_world(&FORMATION, look)), 0.0f);
}

static void warp_out_submit(double td) {
    float const t = (float)td;
    starfield_submit();
    xform_t const station = {mat3_rot_z(STATION_SPIN * t), STATION_POS, 1.0f};
    station_submit(&station);

    xform_t pose;
    xform_t base = hero_base(t);
    if (warp_pose(&base, t, T_HERO_WARP, WARP_OUT, &pose)) player_ship_submit(&pose, 1.0f, td);
    submit_flash(hero_base, T_HERO_WARP, 1.6f, t, 81u);

    base = green_base(t);
    if (warp_pose(&base, t, T_GREEN_WARP, WARP_OUT, &pose)) marauder_submit(&pose, MARAUDER_GREEN, 1.0f, td, 1u);
    submit_flash(green_base, T_GREEN_WARP, 1.3f, t, 82u);

    base = yellow_base(t);
    if (warp_pose(&base, t, T_YELLOW_WARP, WARP_OUT, &pose)) marauder_submit(&pose, MARAUDER_YELLOW, 1.0f, td, 2u);
    submit_flash(yellow_base, T_YELLOW_WARP, 1.3f, t, 83u);

    submit_fire(t);
}

static char const* warp_out_shot(double t) {
    return t < SHOT_WARP ? "chase" : "warp";
}

scene_def_t const SCENE_WARP_OUT = {
    .name     = "warp_out",
    .duration = SCENE_SECS,
    .init     = warp_out_init,
    .shutdown = warp_out_shutdown,
    .enter    = warp_out_enter,
    .camera   = warp_out_camera,
    .submit   = warp_out_submit,
    .shot     = warp_out_shot,
};
