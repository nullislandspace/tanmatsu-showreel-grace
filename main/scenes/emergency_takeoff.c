// =====================================================================
//  Showreel scene  --  emergency_takeoff (scene 5)
// ---------------------------------------------------------------------
//  The hero ship gets out (D-28): engines light, it lifts off straight
//  up and pitches into a steep climb, while the marauders come round for
//  a second pass, head-on and descending. The hero climbs between them
//  -- one passes either side -- firing its blue guns as they close.
//
//  Two shots: low by the pad for the lift-off, looking up; then riding
//  behind the climbing hero for the crossing.
// =====================================================================

#include <math.h>
#include "assets/laser.h"
#include "assets/marauder.h"
#include "assets/planet_base.h"
#include "assets/player_ship.h"
#include "camera.h"
#include "scenes/flight.h"
#include "scenes/scenes.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define SCENE_SECS      8.0f
#define T_IGNITE_END    1.0f  // throttle up to full, still on the pad
#define T_LIFT          1.2f  // wheels up
#define T_PITCH0        2.0f  // nose starts coming up ...
#define T_PITCH1        4.0f  // ... fully into the climb
#define T_CROSS         5.5f  // the marauders pass either side
#define T_CHASE_SHOT    3.8f
#define HERO_FIRE0      4.4f  // the hero's guns, alternating
#define HERO_FIRE1      5.3f
#define HERO_INTERVAL   0.15f
#define RAIDER_FIRE0    4.2f
#define RAIDER_FIRE1    5.2f
#define RAIDER_INTERVAL 0.2f

// --- Scale: the planet scenes' ------------------------------------------------
#define HERO_SPAN     2.0f
#define HERO_HALF_H   0.128f
#define MARAUDER_SPAN 1.8f
#define CROSS_SIDE    2.6f  // the marauders' lateral offset at the crossing

// The climb from the pad, one point a second from lift-off.
static vec3_t CLIMB_PTS[] = {
    {0.0f, 0.0f, 0.0f},  // y set at init: resting on the pad
    {0.0f, 2.5f, 0.3f},   {0.0f, 6.0f, 2.0f},   {0.0f, 10.0f, 6.0f},  {0.0f, 14.5f, 12.0f},
    {0.0f, 19.0f, 19.0f}, {0.0f, 23.0f, 27.0f}, {0.0f, 27.0f, 36.0f},
};
static path_t const CLIMB = {CLIMB_PTS, 8, T_LIFT, 1.0f};

// The marauders' second pass: straight lines, descending towards -z,
// through the hero's crossing point +- CROSS_SIDE at T_CROSS.
static vec3_t const RAIDER_VEL = {0.0f, -4.0f, -17.0f};

static float hero_rest_y(void) {
    return planet_base_pad_centre().y + HERO_HALF_H * HERO_SPAN;
}

static vec3_t hero_pos(float t) {
    if (t < T_LIFT) return v3(0.0f, hero_rest_y(), 0.0f);
    return path_pos(&CLIMB, t);
}

// Level on the pad; from T_PITCH0 the nose swings from dead ahead (+z)
// round to the climb's velocity.
static xform_t hero_pose(float t) {
    vec3_t fwd = v3(0.0f, 0.0f, 1.0f);
    if (t >= T_LIFT) {
        float const w = smoothstep(T_PITCH0, T_PITCH1, t);
        fwd           = v3_norm(v3_lerp(fwd, v3_norm(path_vel(&CLIMB, t)), w));
    }
    // A shudder while the engines spool up on the pad.
    float const shake = t < T_LIFT + 0.4f ? 0.015f * sinf(47.0f * t) * smoothstep(0.2f, T_IGNITE_END, t) : 0.0f;
    return (xform_t){mat3_from_fwd_up(fwd, v3(0.0f, 1.0f, 0.0f), shake), hero_pos(t), HERO_SPAN};
}

static float hero_throttle(float t) {
    return 0.3f + 0.9f * smoothstep(0.0f, T_IGNITE_END, t);  // flares past nominal
}

static vec3_t cross_point(void) {
    return path_pos(&CLIMB, T_CROSS);
}

static xform_t raider_pose(int side, float t) {
    vec3_t const at = v3_add(cross_point(), v3(side ? CROSS_SIDE : -CROSS_SIDE, 0.4f, 0.0f));
    // A slight roll away from the hero as they pass.
    return flight_pose_line(at, RAIDER_VEL, T_CROSS, t, MARAUDER_SPAN, side ? -0.35f : 0.35f);
}

static void submit_fire(float t) {
    // The hero: at each marauder in turn, from both pods.
    for (float tf = HERO_FIRE0; tf <= HERO_FIRE1 && tf <= t; tf += HERO_INTERVAL) {
        int const k = (int)lroundf((tf - HERO_FIRE0) / HERO_INTERVAL);
        if (!laser_lit(t, tf, &LASER_STYLE_PLAYER)) continue;
        xform_t const hero   = hero_pose(t);
        vec3_t const  target = raider_pose(k & 1, t).pos;
        laser_submit_beam(player_ship_gun(&hero, k & 1), target, t, tf, &LASER_STYLE_PLAYER);
    }
    // The marauders: at the hero, just wide.
    for (float tf = RAIDER_FIRE0; tf <= RAIDER_FIRE1 && tf <= t; tf += RAIDER_INTERVAL) {
        int const k = (int)lroundf((tf - RAIDER_FIRE0) / RAIDER_INTERVAL);
        if (!laser_lit(t, tf, &LASER_STYLE_MARAUDER)) continue;
        xform_t const raider = raider_pose(k & 1, t);
        vec3_t const  miss   = v3((hash01(k, 0x7A0u) - 0.5f) * 3.0f, 1.2f + hash01(k, 0x7A1u), 0.0f);
        laser_submit_beam(marauder_gun(&raider, (k >> 1) & 1), v3_add(hero_pos(t), miss), t, tf, &LASER_STYLE_MARAUDER);
    }
}

static void takeoff_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    marauder_init();
    planet_base_init();
    CLIMB_PTS[0].y = hero_rest_y();
}

static void takeoff_shutdown(void) {
    player_ship_shutdown();
    marauder_shutdown();
    planet_base_shutdown();
}

static void takeoff_enter(void) {
    se_light_set(&(se_light_t){.x = -500.0f, .y = 260.0f, .z = -300.0f, .brightness = 0.8f, .two_sided = true});
}

static void takeoff_camera(double td) {
    float const  t    = (float)td;
    vec3_t const hero = hero_pos(t);
    if (t < T_CHASE_SHOT) {
        // Low, west of the pad, looking east: the ship broadside and lit
        // (the sun is south-west), rising against the sky and the plain.
        camera_look_at(v3(-6.5f, 0.8f, -1.0f), v3_add(hero, v3(0.0f, 0.4f, 0.0f)), 0.0f);
    } else {
        // Behind and a little above the hero, a moment back along its
        // climb, looking ahead to where the marauders come from.
        vec3_t const eye   = v3_add(path_pos(&CLIMB, t - 0.3f), v3(0.0f, 1.2f, -0.6f));
        vec3_t const ahead = v3_add(hero, v3_scale(v3_norm(path_vel(&CLIMB, t)), 8.0f));
        camera_look_at(eye, ahead, 0.0f);
    }
}

static void takeoff_submit(double td) {
    float const t = (float)td;
    planet_base_submit(td);
    xform_t const hero = hero_pose(t);
    player_ship_submit(&hero, hero_throttle(t), td);
    for (int side = 0; side < 2; side++) {
        xform_t const r = raider_pose(side, t);
        marauder_submit(&r, side ? MARAUDER_YELLOW : MARAUDER_GREEN, 1.0f, td, 1u + (unsigned)side);
    }
    submit_fire(t);
}

static char const* takeoff_shot(double t) {
    return t < T_CHASE_SHOT ? "liftoff" : "crossing";
}

static backdrop_t const* takeoff_backdrop(double t) {
    (void)t;
    return planet_base_backdrop();
}

scene_def_t const SCENE_EMERGENCY_TAKEOFF = {
    .name        = "emergency_takeoff",
    .duration    = SCENE_SECS,
    .init        = takeoff_init,
    .shutdown    = takeoff_shutdown,
    .enter       = takeoff_enter,
    .camera      = takeoff_camera,
    .submit      = takeoff_submit,
    .shot        = takeoff_shot,
    .backdrop_at = takeoff_backdrop,
};
