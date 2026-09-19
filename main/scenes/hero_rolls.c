// =====================================================================
//  Showreel scene  --  hero_rolls (scene 11)
// ---------------------------------------------------------------------
//  The last scene (D-28): close beside the hero ship flying fast through
//  the second system, space dust streaming past to show the speed. It
//  flies straight, rolls twice about its own centreline and flies
//  straight on. At the end the camera drops back and the ship pulls away
//  from it into the distance, towards the gas giant.
//
//  One shot, riding along beside and a little ahead of the ship.
// =====================================================================

#include <math.h>
#include "assets/player_ship.h"
#include "assets/space_dust.h"
#include "camera.h"
#include "scenes/scenes.h"
#include "scenes/system2.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define SCENE_SECS 8.0f
#define ROLL0      2.2f  // the two rolls, eased in and out
#define ROLL1      4.8f
#define ROLLS      2.0f
#define T_DROP     5.8f  // the camera starts falling back

// --- Flight -----------------------------------------------------------------
#define SPEED     30.0f  // units per second, along -z
#define HERO_SPAN 1.0f
#define CAM_DECEL 16.0f  // units/s^2 the camera slows by from T_DROP

// Camera offset from the ship's line: across (-x: the ship's right, the
// sunlit side), up, and ahead (-z).
#define CAM_RIGHT -1.9f
#define CAM_UP    0.45f
#define CAM_AHEAD 1.1f

static float roll_angle(float t) {
    return ROLLS * 6.2831853f * smoothstep(ROLL0, ROLL1, t);
}

// The ship's line of flight at t.
static vec3_t line_pos(float t) {
    return v3(0.0f, 0.0f, -SPEED * t);
}

// On the line of flight, rolling about it: the model is centred on its
// origin (player_ship.c), so that is the hull's centreline.
static xform_t hero_pose(float t) {
    return (xform_t){mat3_from_fwd_up(v3(0.0f, 0.0f, -1.0f), v3(0.0f, 1.0f, 0.0f), roll_angle(t)), line_pos(t),
                     HERO_SPAN};
}

// How far the camera has dropped back behind its riding position.
static float cam_drop(float t) {
    float const d = fmaxf(t - T_DROP, 0.0f);
    return 0.5f * CAM_DECEL * d * d;
}

static vec3_t cam_vel(float t) {
    return v3(0.0f, 0.0f, -SPEED + CAM_DECEL * fmaxf(t - T_DROP, 0.0f));
}

static void rolls_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    space_dust_init();
    system2_init();
}

static void rolls_shutdown(void) {
    player_ship_shutdown();
    system2_shutdown();
}

static void rolls_enter(void) {
    system2_light();
}

static void rolls_camera(double td) {
    float const  t   = (float)td;
    vec3_t const eye = v3_add(line_pos(t), v3(CAM_RIGHT, CAM_UP, -CAM_AHEAD + cam_drop(t)));
    // At the ship, a little ahead of it while riding along; on the ship
    // as it pulls away.
    vec3_t const at  = v3_add(line_pos(t), v3(0.0f, 0.0f, -0.3f));
    camera_look_at(eye, at, 0.0f);
}

static void rolls_submit(double td) {
    float const t = (float)td;
    system2_submit_sky(t);
    space_dust_submit(cam_vel(t), 0.03f);
    xform_t const hero = hero_pose(t);
    player_ship_submit(&hero, 1.2f, td);
}

scene_def_t const SCENE_HERO_ROLLS = {
    .name     = "hero_rolls",
    .duration = SCENE_SECS,
    .init     = rolls_init,
    .shutdown = rolls_shutdown,
    .enter    = rolls_enter,
    .camera   = rolls_camera,
    .submit   = rolls_submit,
};
