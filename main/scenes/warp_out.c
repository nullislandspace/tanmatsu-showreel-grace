// =====================================================================
//  Showreel scene  --  warp_out (scene 8)
// ---------------------------------------------------------------------
//  The hero ship gets away (D-28): it pulls ahead of the marauders and
//  jumps to hyperspace (warp.h) -- a stretch along its flight line, a
//  flash, gone. The marauders fly on for a few seconds, then jump after
//  it, one after the other. The station, small, far off to the left.
//
//  One shot: riding behind the pair, looking past them at the hero.
// =====================================================================

#include <math.h>
#include "assets/marauder.h"
#include "assets/player_ship.h"
#include "assets/starfield.h"
#include "assets/station.h"
#include "assets/warp.h"
#include "camera.h"
#include "scenes/formation.h"
#include "scenes/scenes.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define SCENE_SECS    7.5f
#define T_HERO_WARP   2.2f  // the stretch starts
#define T_GREEN_WARP  4.8f
#define T_YELLOW_WARP 5.4f

// --- Ships ------------------------------------------------------------------
#define HERO_SPAN     1.0f
#define MARAUDER_SPAN 0.9f
// The hero: ahead of the formation and pulling away.
#define HERO_AHEAD    2.5f  // units ahead of the formation's centre at t = 0 ...
#define HERO_GAIN     1.5f  // ... gaining this many units a second

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
// The station, small, far off ahead-left and above: out of the way of
// the ships and the flashes in the middle of the frame.
#define STATION_POS \
    { -300.0f, 70.0f, -600.0f }
#define STATION_SPIN 0.15f

// --- Camera -------------------------------------------------------------------
// In the formation's frame: behind the pair and a little above,
// looking ahead past them.
#define CAM_OFFSET \
    { -0.3f, 1.0f, -4.2f }
#define CAM_LOOK \
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

static xform_t green_base(float t) {
    return formation_pose(&FORMATION, &SLOT_GREEN, t, MARAUDER_SPAN);
}

static xform_t yellow_base(float t) {
    return formation_pose(&FORMATION, &SLOT_YELLOW, t, MARAUDER_SPAN);
}

// The flash where the ship posed by `base` (at warp time `tw`) vanishes.
static void submit_flash(pose_fn_t base, float tw, float size, float t, unsigned seed) {
    xform_t const at_flash = base(tw + WARP_STRETCH_SECS);
    warp_submit_flash(warp_point(&at_flash, WARP_OUT), size, t, tw, WARP_OUT, seed);
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
    camera_look_at(v3_add(centre, formation_to_world(&FORMATION, (vec3_t)CAM_OFFSET)),
                   v3_add(centre, formation_to_world(&FORMATION, (vec3_t)CAM_LOOK)), 0.0f);
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
}

scene_def_t const SCENE_WARP_OUT = {
    .name     = "warp_out",
    .duration = SCENE_SECS,
    .init     = warp_out_init,
    .shutdown = warp_out_shutdown,
    .enter    = warp_out_enter,
    .camera   = warp_out_camera,
    .submit   = warp_out_submit,
};
