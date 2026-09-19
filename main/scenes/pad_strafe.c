// =====================================================================
//  Showreel scene  --  pad_strafe (scene 4)
// ---------------------------------------------------------------------
//  The marauders make a low strafing pass over the base, firing red
//  beams at the landed hero ship (D-28). They miss: the beams tear into
//  the ground round it, each hit throwing up an impact burst.
//
//  Two shots: riding behind the diving marauders, looking down the pass
//  at the pad; then low on the ground by the pad as they scream over.
// =====================================================================

#include <math.h>
#include "assets/explosion.h"
#include "assets/laser.h"
#include "assets/marauder.h"
#include "assets/planet_base.h"
#include "assets/player_ship.h"
#include "camera.h"
#include "scenes/flight.h"
#include "scenes/scenes.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define SCENE_SECS    7.0f
#define T_GROUND_SHOT 3.4f  // chase -> ground camera
#define FIRE_START    1.6f
#define FIRE_END      4.6f
#define FIRE_INTERVAL 0.17f  // the two ships alternate

// --- Scale: the planet scenes' (planet_landing.c) -----------------------------
#define HERO_SPAN     2.0f
#define HERO_HALF_H   0.128f  // model mid-height to belly at span 1
#define MARAUDER_SPAN 1.8f    // 0.9 of the hero, as in space

// The pass: in from the south-west, low over the pad at ~4.3 s, climbing
// away over the works to the north-east -- with the sun (south-west) at
// the marauders' backs, so a camera behind them sees the works lit. The
// yellow ship flies the same line 3 units to its right, a little behind.
static vec3_t const GREEN_PTS[] = {
    {-26.0f, 26.0f, -110.0f}, {-16.0f, 16.0f, -62.0f}, {-7.0f, 9.0f, -24.0f},
    {1.0f, 6.5f, 6.0f},       {10.0f, 11.0f, 38.0f},   {20.0f, 24.0f, 80.0f},
};
static vec3_t const YELLOW_PTS[] = {
    {-23.0f, 27.0f, -116.0f}, {-13.0f, 17.0f, -68.0f}, {-4.0f, 10.0f, -30.0f},
    {4.0f, 7.5f, 0.0f},       {13.0f, 12.0f, 32.0f},   {23.0f, 25.0f, 74.0f},
};
static path_t const GREEN_PATH  = {GREEN_PTS, 6, 0.0f, 1.5f};
static path_t const YELLOW_PATH = {YELLOW_PTS, 6, 0.1f, 1.5f};

// The landed hero: as planet_landing left it.
static xform_t hero_pose(void) {
    float const y = planet_base_pad_centre().y + HERO_HALF_H * HERO_SPAN;
    return (xform_t){mat3_rot_y(0.0f), v3(0.0f, y, 0.0f), HERO_SPAN};
}

// Shot k's aim point: on the ground round the hero, never on it.
static vec3_t aim(int k) {
    float const a = 6.2831853f * hash01(k, 0x57AFu);
    float const r = 2.2f + 2.8f * hash01(k, 0x57B0u);
    float const x = r * sinf(a), z = r * cosf(a);
    // On the pad (6 across) the ground is its top; beyond, y = 0.
    float const y = (fabsf(x) < 3.0f && fabsf(z) < 3.0f) ? planet_base_pad_centre().y : 0.0f;
    return v3(x, y, z);
}

// Where shot k, fired at tf towards its aim point, strikes: the aim
// point, or a building of the base in the way.
static vec3_t strike(int k, float tf) {
    bool const    yellow = (k & 1) != 0;
    xform_t const pose   = flight_pose(yellow ? &YELLOW_PATH : &GREEN_PATH, tf, MARAUDER_SPAN, 0.0f);
    vec3_t const  gun    = marauder_gun(&pose, (k >> 1) & 1);
    vec3_t const  target = aim(k);
    vec3_t const  dir    = v3_norm(v3_sub(target, gun));
    float         d      = v3_len(v3_sub(target, gun));
    planet_base_raycast(gun, dir, d, &d);
    return v3_add(gun, v3_scale(dir, d));
}

static void submit_fire(float t) {
    if (t < FIRE_START) return;
    // Every shot fired so far whose impact may still be burning.
    int const last = (int)floorf((fminf(t, FIRE_END) - FIRE_START) / FIRE_INTERVAL);
    for (int k = last; k >= 0; k--) {
        float const tf = FIRE_START + (float)k * FIRE_INTERVAL;
        if (t - tf > IMPACT_SECS && t - tf > LASER_STYLE_MARAUDER.duration) break;
        bool const    yellow = (k & 1) != 0;
        xform_t const pose   = flight_pose(yellow ? &YELLOW_PATH : &GREEN_PATH, t, MARAUDER_SPAN, 0.0f);
        vec3_t const  gun    = marauder_gun(&pose, (k >> 1) & 1);
        vec3_t const  at     = strike(k, tf);
        laser_submit_beam(gun, at, t, tf, &LASER_STYLE_MARAUDER);
        impact_submit(at, v3_sub(gun, at), 0.9f, t, tf, 300u + (unsigned)k);
    }
}

static void strafe_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    marauder_init();
    planet_base_init();
}

static void strafe_shutdown(void) {
    player_ship_shutdown();
    marauder_shutdown();
    planet_base_shutdown();
}

static void strafe_enter(void) {
    // planet_landing's low sun.
    se_light_set(&(se_light_t){.x = -500.0f, .y = 260.0f, .z = -300.0f, .brightness = 0.8f, .two_sided = true});
}

static void strafe_camera(double td) {
    float const  t    = (float)td;
    vec3_t const hero = hero_pose().pos;
    if (t < T_GROUND_SHOT) {
        // Just outside the trailing yellow ship (to its right) and above, a
        // moment back along its line, looking ahead at the green one and on
        // down the pass at the pad: yellow near on the right, green beyond.
        vec3_t const eye = v3_add(path_pos(&YELLOW_PATH, t - 0.3f), v3(1.6f, 1.4f, 0.0f));
        camera_look_at(eye, v3_lerp(path_pos(&GREEN_PATH, t), hero, 0.3f), 0.0f);
    } else {
        // Low by the pad, south-west of the hero and looking past it at the
        // works: the marauders roar in over the camera and away, the hits
        // bursting round the ship. A slow tilt up follows them out.
        float const  k      = smoothstep(T_GROUND_SHOT, SCENE_SECS, t);
        vec3_t const target = v3_lerp(v3(0.0f, 2.0f, 6.0f), v3(2.0f, 6.0f, 14.0f), k);
        camera_look_at(v3(-3.0f, 1.1f, -7.0f), target, 0.0f);
    }
}

static void strafe_submit(double td) {
    float const t = (float)td;
    planet_base_submit(td);
    xform_t const hero = hero_pose();
    player_ship_submit(&hero, 0.0f, td);
    xform_t const green  = flight_pose(&GREEN_PATH, t, MARAUDER_SPAN, 0.0f);
    xform_t const yellow = flight_pose(&YELLOW_PATH, t, MARAUDER_SPAN, 0.0f);
    marauder_submit(&green, MARAUDER_GREEN, 1.0f, td, 1u);
    marauder_submit(&yellow, MARAUDER_YELLOW, 1.0f, td, 2u);
    submit_fire(t);
}

static char const* strafe_shot(double t) {
    return t < T_GROUND_SHOT ? "chase" : "ground";
}

static backdrop_t const* strafe_backdrop(double t) {
    (void)t;
    return planet_base_backdrop();
}

scene_def_t const SCENE_PAD_STRAFE = {
    .name        = "pad_strafe",
    .duration    = SCENE_SECS,
    .init        = strafe_init,
    .shutdown    = strafe_shutdown,
    .enter       = strafe_enter,
    .camera      = strafe_camera,
    .submit      = strafe_submit,
    .shot        = strafe_shot,
    .backdrop_at = strafe_backdrop,
};
