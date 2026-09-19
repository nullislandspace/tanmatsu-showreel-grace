// =====================================================================
//  Showreel scene  --  asteroid_ambush (scene 9)
// ---------------------------------------------------------------------
//  Another solar system (system2.h). The hero ship drops out of warp,
//  brakes hard and tucks in behind a big asteroid. A few seconds later
//  the marauders warp in on its trail, fly on and pass the rock on the
//  other side. The hero slides out behind them, falls in on their tail
//  and opens fire with its blue guns (D-28).
//
//  World: the big asteroid at the origin; the marauders fly a straight
//  line along -z, 12 units to its right (+x); the hero hides 9.5 units
//  to its left.
//
//  Three shots: out on the hiding side of the rock (the arrival and the
//  hiding); low by the rock on the marauders' line as they drop in and
//  roar past; riding behind the hero as it comes out after them.
// =====================================================================

#include <math.h>
#include "assets/asteroid.h"
#include "assets/laser.h"
#include "assets/marauder.h"
#include "assets/player_ship.h"
#include "assets/warp.h"
#include "camera.h"
#include "scenes/formation.h"
#include "scenes/scenes.h"
#include "scenes/system2.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define SCENE_SECS    16.0f
#define T_HERO_IN     1.0f  // the hero's warp-in ends: normal pose from here
#define T_HIDE        5.0f  // at rest behind the rock
#define T_GREEN_IN    5.0f  // the marauders' warp-ins end
#define T_YELLOW_IN   5.4f
#define T_PASS        7.5f   // the formation passes the rock (z = 0)
#define T_EMERGE      9.0f   // the hero moves out ...
#define T_ON_TAIL     13.0f  // ... and sits on their tail from here
#define FIRE0         12.8f  // the hero's guns, alternating
#define FIRE1         15.6f
#define FIRE_INTERVAL 0.15f
#define AIM_TURN      0.4f  // seconds to bring the nose on / off the aim

#define SHOT_PASS   4.6f  // before the marauders drop in, so their flashes are in it
#define SHOT_AMBUSH 9.0f

// --- Ships ------------------------------------------------------------------
#define HERO_SPAN     1.0f
#define MARAUDER_SPAN 0.9f

// The hero's arrival: out of warp at 2x path speed, braking to rest at
// the last point (path time 0..4 mapped onto T_HERO_IN..T_HIDE).
static vec3_t const ARRIVE_PTS[] = {
    {-3.0f, 3.5f, 40.0f}, {-6.0f, 2.5f, 24.0f}, {-9.0f, 1.2f, 11.0f}, {-9.8f, 0.6f, 4.5f}, {-9.5f, 0.5f, 2.0f},
};
static path_t const ARRIVE = {ARRIVE_PTS, 5, 0.0f, 1.0f};
#define ARRIVE_SPAN 4.0f  // path time of the last point

// The marauders: through (12, 1, 0) at T_PASS, along -z at 14 units/s.
static formation_t const FORMATION = {
    .origin = {12.0f, 1.0f, 14.0f * T_PASS}, .vel = {0.0f, 0.0f, -14.0f}, .bank_per_acc = 0.06f, .bank_max = 0.6f};
static slot_t const SLOT_GREEN = {
    .offset = {-0.9f, 0.15f, 0.0f},
    .weave  = {0.2f, 0.29f, 0.7f, 0.1f, 0.41f, 0.2f, 0.08f, 0.7f, 1.2f},
};
static slot_t const SLOT_YELLOW = {
    .offset = {0.9f, -0.1f, -0.8f},
    .weave  = {-0.18f, 0.35f, 2.4f, 0.12f, 0.47f, 1.9f, 0.1f, 0.85f, 0.3f},
};
// Where the hero sits on their tail, in the formation's frame.
#define TAIL_SLOT \
    { 0.2f, 1.0f, -7.0f }
// On the way out it swings wide of the rock: this far at the half-way point.
#define EMERGE_SWING \
    { -2.0f, 0.0f, -9.0f }

// --- The rocks ------------------------------------------------------------------
#define ROCK_RADIUS 6.0f
typedef struct {
    int    shape;
    vec3_t pos;
    float  radius;
    vec3_t axis;
    float  spin;
} rock_t;
static rock_t const ROCKS[] = {
    {0, {0.0f, 0.0f, 0.0f}, ROCK_RADIUS, {0.3f, 1.0f, 0.2f}, 0.05f},
    {1, {-18.0f, 6.0f, -25.0f}, 1.2f, {1.0f, 0.4f, 0.1f}, 0.3f},
    {2, {22.0f, -5.0f, 18.0f}, 0.8f, {0.2f, 0.3f, 1.0f}, 0.45f},
    {3, {-6.0f, -9.0f, 30.0f}, 1.5f, {0.6f, 1.0f, 0.5f}, 0.2f},
    {1, {14.0f, 8.0f, -40.0f}, 1.0f, {0.1f, 1.0f, 0.9f}, 0.35f},
};
#define ROCK_N ((int)(sizeof(ROCKS) / sizeof(ROCKS[0])))

// --- The hero ---------------------------------------------------------------------

static vec3_t hide_pos(float t) {
    return v3_add(ARRIVE_PTS[4], v3(0.0f, 0.08f * sinf(1.3f * t), 0.0f));
}

static vec3_t tail_pos(float t) {
    return v3_add(formation_centre(&FORMATION, t), formation_to_world(&FORMATION, (vec3_t)TAIL_SLOT));
}

// The arrival path's time at scene time t: 2x speed at T_HERO_IN,
// braking to rest at T_HIDE; before T_HERO_IN, straight on backwards
// (the warp-in draws that part).
static float arrive_tau(float t) {
    if (t < T_HERO_IN) return 2.0f * (t - T_HERO_IN);
    float const f = fminf((t - T_HERO_IN) / (T_HIDE - T_HERO_IN), 1.0f);
    return ARRIVE_SPAN * (1.0f - (1.0f - f) * (1.0f - f));
}

static vec3_t hero_pos(float t) {
    if (t < T_HIDE) return path_pos(&ARRIVE, arrive_tau(t));
    if (t < T_EMERGE) return hide_pos(t);
    // Out from behind the rock onto their tail: a blend from rest to the
    // moving slot, swung wide of the rock on the way.
    float const  w     = smoothstep(T_EMERGE, T_ON_TAIL, t);
    vec3_t const blend = v3_lerp(hide_pos(t), tail_pos(t), w);
    return v3_add(blend, v3_scale((vec3_t)EMERGE_SWING, sinf(3.1415927f * w)));
}

static vec3_t hero_vel(float t) {
    float const h = 0.02f;
    return v3_scale(v3_sub(hero_pos(t + h), hero_pos(t - h)), 0.5f / h);
}

// Which way the hero faces with no aim: along the arrival path; at rest,
// the way it came to rest; moving off, turning onto its velocity.
static vec3_t hero_heading(float t) {
    vec3_t const rest = v3_norm(path_vel(&ARRIVE, ARRIVE_SPAN));
    if (t < T_HIDE) return v3_norm(path_vel(&ARRIVE, arrive_tau(t)));
    if (t < T_EMERGE) return rest;
    vec3_t const v = hero_vel(t);
    float const  k = clampf(v3_len(v) / 5.0f, 0.0f, 1.0f);
    return v3_norm(v3_lerp(rest, v3_norm(v), k));
}

// The hero's aim while firing: a little above the pair, sweeping across
// them -- every beam a near miss (scene 10 has the hits).
static vec3_t hero_aim(float t) {
    vec3_t const g   = formation_pose(&FORMATION, &SLOT_GREEN, t, MARAUDER_SPAN).pos;
    vec3_t const y   = formation_pose(&FORMATION, &SLOT_YELLOW, t, MARAUDER_SPAN).pos;
    vec3_t const mid = v3_scale(v3_add(g, y), 0.5f);
    float const  s   = sinf(6.2831853f * 0.7f * (t - FIRE0));
    return v3_add(mid, formation_to_world(&FORMATION, v3(1.4f * s, 1.2f, 0.0f)));
}

static xform_t hero_base(float t) {
    vec3_t       fwd = hero_heading(t);
    float const  a   = smoothstep(FIRE0 - AIM_TURN, FIRE0, t) * (1.0f - smoothstep(FIRE1, FIRE1 + AIM_TURN, t));
    vec3_t const pos = hero_pos(t);
    if (a > 0.0f) fwd = v3_norm(v3_lerp(fwd, v3_norm(v3_sub(hero_aim(t), pos)), a));
    // Bank into the turn out from behind the rock.
    float const  h     = 0.05f;
    vec3_t const acc   = v3_scale(v3_sub(hero_vel(t + h), hero_vel(t - h)), 0.5f / h);
    vec3_t const right = v3_norm(v3_cross(v3(0.0f, 1.0f, 0.0f), fwd));
    float const  bank  = t < T_EMERGE ? 0.0f : clampf(-0.06f * v3_dot(acc, right), -0.8f, 0.8f);
    return (xform_t){mat3_from_fwd_up(fwd, v3(0.0f, 1.0f, 0.0f), bank), pos, HERO_SPAN};
}

static xform_t green_base(float t) {
    return formation_pose(&FORMATION, &SLOT_GREEN, t, MARAUDER_SPAN);
}

static xform_t yellow_base(float t) {
    return formation_pose(&FORMATION, &SLOT_YELLOW, t, MARAUDER_SPAN);
}

// A ship warping in at `tw` whose own pose is base(t): the ship once it
// is there, the flash before.
static void submit_warp_in(xform_t (*base)(float), float tw, float t, float size, unsigned seed, bool hero,
                           marauder_livery_t livery) {
    xform_t const b = base(t);
    xform_t       pose;
    if (warp_pose(&b, t, tw, WARP_IN, &pose)) {
        if (hero) {
            player_ship_submit(&pose, 1.0f, t);
        } else {
            marauder_submit(&pose, livery, 1.0f, t, livery == MARAUDER_GREEN ? 1u : 2u);
        }
    }
    xform_t const at_flash = base(tw - WARP_STRETCH_SECS);
    warp_submit_flash(warp_point(&at_flash, WARP_IN), size, t, tw, WARP_IN, seed);
}

static void submit_fire(float t) {
    if (t < FIRE0) return;
    int const   k  = (int)floorf((t - FIRE0) / FIRE_INTERVAL);
    float const tf = FIRE0 + (float)k * FIRE_INTERVAL;
    if (tf > FIRE1 || !laser_lit(t, tf, &LASER_STYLE_PLAYER)) return;
    xform_t const hero = hero_base(t);
    laser_submit_ray(player_ship_gun(&hero, k & 1), v3_norm(hero.r.fwd), t, tf, &LASER_STYLE_PLAYER);
}

// --- Scene ------------------------------------------------------------------

static void ambush_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    marauder_init();
    asteroid_init();
    system2_init();
}

static void ambush_shutdown(void) {
    player_ship_shutdown();
    marauder_shutdown();
    asteroid_shutdown();
    system2_shutdown();
}

static void ambush_enter(void) {
    system2_light();
}

static void ambush_camera(double td) {
    float const t = (float)td;
    if (t < SHOT_PASS) {
        // Out on the hiding side of the rock, looking in along the way the
        // hero comes: it drops out of warp in the distance and brakes into
        // cover in front of the rock, the rock between it and the
        // marauders' line beyond.
        camera_look_at(v3(-18.0f, 4.0f, -3.0f), v3(-6.0f, 1.0f, 14.0f), 0.0f);
    } else if (t < SHOT_AMBUSH) {
        // Low beside the rock, a few units off the marauders' line: their
        // flashes far off, then turning to follow them in, past and away.
        camera_look_at(v3(8.2f, -1.5f, -6.0f), formation_centre(&FORMATION, t), 0.0f);
    } else {
        // Behind and above the hero, looking ahead along its nose.
        xform_t const hero = hero_base(t);
        vec3_t const  fwd  = v3_norm(hero.r.fwd);
        vec3_t const  eye  = v3_add(v3_add(hero.pos, v3_scale(fwd, -3.2f)), v3(0.0f, 1.0f, 0.0f));
        camera_look_at(eye, v3_add(hero.pos, v3_scale(fwd, 6.0f)), 0.0f);
    }
}

static void ambush_submit(double td) {
    float const t = (float)td;
    system2_submit_sky(t);
    for (int i = 0; i < ROCK_N; i++) {
        rock_t const* r = &ROCKS[i];
        xform_t const x = {mat3_axis_angle(v3_norm(r->axis), r->spin * t), r->pos, r->radius};
        asteroid_submit(r->shape, &x);
    }
    submit_warp_in(hero_base, T_HERO_IN, t, 1.6f, 91u, true, MARAUDER_GREEN);
    submit_warp_in(green_base, T_GREEN_IN, t, 1.3f, 92u, false, MARAUDER_GREEN);
    submit_warp_in(yellow_base, T_YELLOW_IN, t, 1.3f, 93u, false, MARAUDER_YELLOW);
    submit_fire(t);
}

static char const* ambush_shot(double t) {
    return t < SHOT_PASS ? "arrival" : t < SHOT_AMBUSH ? "pass" : "ambush";
}

scene_def_t const SCENE_ASTEROID_AMBUSH = {
    .name     = "asteroid_ambush",
    .duration = SCENE_SECS,
    .init     = ambush_init,
    .shutdown = ambush_shutdown,
    .enter    = ambush_enter,
    .camera   = ambush_camera,
    .submit   = ambush_submit,
    .shot     = ambush_shot,
};
