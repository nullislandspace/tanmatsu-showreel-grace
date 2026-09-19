// =====================================================================
//  Showreel scene  --  marauder_pursuit
// ---------------------------------------------------------------------
//  The opening: the two marauders in formation, seen from close behind
//  and just right of the right one, weaving a little and firing straight
//  ahead at a target out of frame -- the player's ship. It is there to establish
//  that they are not going to give up before spacestation_flyby shows
//  the chase itself.
//
//  A pure function of scene time t, like every scene (scene.h). The
//  formation flies straight along -z; each ship adds its own gentle
//  weave (sideways and up/down) and banks into it, and the camera rides
//  along with the formation.
// =====================================================================

#include <math.h>
#include "camera.h"
#include "space/assets/laser.h"
#include "space/assets/marauder.h"
#include "space/assets/starfield.h"
#include "space/scenes/formation.h"
#include "space/space.h"
#include "synthengine3d.h"

#define SCENE_SECS 6.0f

// --- Formation (scenes/formation.h) ----------------------------------------
#define MARAUDER_SPAN 0.9f

// Along -z at 14 units/s, as in the flyby; banking 0.06 rad per unit of
// sideways acceleration (as in the flyby), capped at 0.6.
static formation_t const FORMATION = {
    .origin = {0.0f, 0.0f, 0.0f}, .vel = {0.0f, 0.0f, -14.0f}, .bank_per_acc = 0.06f, .bank_max = 0.6f};

// Slots in the formation's frame (x right, y up, z forward): the green
// ship on the left, the yellow one -- the right marauder -- on the right,
// a little lower and 0.6 behind. Each weaves with its own frequencies
// and phases so they never move in step (the negative sideways
// amplitudes keep the sways as they were before formation.c).
static slot_t const SLOT_GREEN = {
    .offset = {-0.75f, 0.1f, 0.0f},
    .weave  = {-0.22f, 0.31f, 0.0f, 0.12f, 0.43f, 1.1f, 0.10f, 0.9f, 0.4f},
};
static slot_t const SLOT_YELLOW = {
    .offset = {0.75f, -0.1f, -0.6f},
    .weave  = {-0.18f, 0.37f, 2.0f, 0.10f, 0.51f, 2.6f, 0.12f, 0.8f, 1.9f},
};

// --- Guns ------------------------------------------------------------------
#define FIRE_INTERVAL 0.2f  // per ship, guns alternating

// --- Camera -----------------------------------------------------------------
// Close behind and right of the right marauder, a little above it, and
// looking diagonally across at the pair: ~50 degrees off the flight
// direction, more than half the horizontal field of view (~42 degrees),
// so the beams, aimed far ahead, run off the edge of the screen instead
// of converging in it. From ~1.5 units the right ship is up to ~370 px
// wide. Keep the camera this far out: the engine's near plane is
// RENDER_NEAR_CLIP_Z = 0.5 in camera-space depth, and 20% closer the
// right ship's wingtip crossed it (clipped away at t ~ 1.5 and 3.3-4.4 s).
// Now its nearest point stays >= 0.63 deep.
#define CAM_RIGHT 0.90f  // beyond the right ship, further right (formation frame)
#define CAM_UP    0.42f
#define CAM_BACK  1.08f
#define CAM_BOB   0.04f  // a slight drift, so the ride does not look locked

// The shot lit at t from one ship, if any: a shot every FIRE_INTERVAL
// (guns alternating), each a beam straight along the ship's nose -- the
// weave swings it about -- out of the frame. `offset` staggers the two
// ships' salvos.
static void submit_lasers(slot_t const* slot, float t, float offset) {
    if (t < offset) return;
    int const   k  = (int)floorf((t - offset) / FIRE_INTERVAL);
    float const tf = offset + (float)k * FIRE_INTERVAL;
    if (!laser_lit(t, tf, &LASER_STYLE_MARAUDER)) return;
    xform_t const pose = formation_pose(&FORMATION, slot, t, MARAUDER_SPAN);
    vec3_t const  fwd  = mat3_apply(&pose.r, v3(0.0f, 0.0f, 1.0f));
    laser_submit_ray(marauder_gun(&pose, k & 1), fwd, t, tf, &LASER_STYLE_MARAUDER);
}

static void pursuit_init(char const* asset_dir) {
    (void)asset_dir;
    marauder_init();
    starfield_init();
}

static void pursuit_shutdown(void) {
    marauder_shutdown();
}

static void pursuit_enter(void) {
    // The flyby's sun: up and to the left, side-lighting the formation.
    se_light_set(&(se_light_t){.x = -500.0f, .y = 350.0f, .z = -60.0f, .brightness = 0.8f, .two_sided = true});
}

// Riding with the formation (the right ship's slot, not its weave, so
// the ships move in the frame), aimed between the two ships, with a slow
// drift.
static void pursuit_camera(double td) {
    float const  t      = (float)td;
    vec3_t const yellow = formation_slot_pos(&FORMATION, &SLOT_YELLOW, t);
    vec3_t const green  = formation_slot_pos(&FORMATION, &SLOT_GREEN, t);
    vec3_t const bob    = v3(CAM_BOB * sinf(0.7f * t), CAM_BOB * sinf(0.9f * t + 1.0f), 0.0f);
    vec3_t const eye    = v3_add(v3_add(yellow, formation_to_world(&FORMATION, v3(CAM_RIGHT, CAM_UP, -CAM_BACK))), bob);
    camera_look_at(eye, v3_scale(v3_add(green, yellow), 0.5f), 0.0f);
}

static void pursuit_submit(double td) {
    float const t = (float)td;
    starfield_submit();

    xform_t const green  = formation_pose(&FORMATION, &SLOT_GREEN, t, MARAUDER_SPAN);
    xform_t const yellow = formation_pose(&FORMATION, &SLOT_YELLOW, t, MARAUDER_SPAN);
    marauder_submit(&green, MARAUDER_GREEN, 1.0f, td, 1u);
    marauder_submit(&yellow, MARAUDER_YELLOW, 1.0f, td, 2u);

    submit_lasers(&SLOT_GREEN, t, 0.4f);
    submit_lasers(&SLOT_YELLOW, t, 0.5f);
}

scene_def_t const SCENE_MARAUDER_PURSUIT = {
    .name     = "marauder_pursuit",
    .duration = SCENE_SECS,
    .init     = pursuit_init,
    .shutdown = pursuit_shutdown,
    .enter    = pursuit_enter,
    .camera   = pursuit_camera,
    .submit   = pursuit_submit,
};
