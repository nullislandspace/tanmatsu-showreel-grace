// =====================================================================
//  Showreel scene  --  marauder_approach (scene 3)
// ---------------------------------------------------------------------
//  Medium close-up: the two marauders in formation, flying towards the
//  industrial planet (D-28). The planet hangs large ahead of them, lit
//  from the side; the camera rides behind and to the left of the pair,
//  looking past them at it, so both ships sit well inside the frame (not
//  the pursuit's fill-the-screen framing).
// =====================================================================

#include <math.h>
#include "camera.h"
#include "space/assets/marauder.h"
#include "space/assets/planet.h"
#include "space/assets/starfield.h"
#include "space/scenes/formation.h"
#include "space/space.h"
#include "synthengine3d.h"

#define SCENE_SECS    6.0f
#define MARAUDER_SPAN 0.9f

// Along -z at 14 units/s, as elsewhere; banking as in the pursuit.
static formation_t const FORMATION = {
    .origin = {0.0f, 0.0f, 0.0f}, .vel = {0.0f, 0.0f, -14.0f}, .bank_per_acc = 0.06f, .bank_max = 0.6f};

// A looser formation than the pursuit's: side by side, the yellow one a
// length back.
static slot_t const SLOT_GREEN = {
    .offset = {-0.9f, 0.15f, 0.0f},
    .weave  = {0.2f, 0.29f, 0.7f, 0.1f, 0.41f, 0.2f, 0.08f, 0.7f, 1.2f},
};
static slot_t const SLOT_YELLOW = {
    .offset = {0.9f, -0.1f, -1.0f},
    .weave  = {-0.18f, 0.35f, 2.4f, 0.12f, 0.47f, 1.9f, 0.1f, 0.85f, 0.3f},
};

// The planet: far ahead and below, big in the frame (its disc spans
// ~40 degrees from here), turning slowly.
#define PLANET_POS \
    { 20.0f, -260.0f, -1500.0f }
#define PLANET_RADIUS 520.0f
#define PLANET_SPIN   0.01f  // rad/s

// Camera, in the formation's frame (x right, y up, z forward): behind
// and to the left of the pair, a little above, looking ahead past them.
#define CAM_OFFSET \
    { -2.4f, 0.9f, -3.6f }
#define CAM_LOOK \
    { 0.3f, -0.3f, 4.0f }

static void approach_init(char const* asset_dir) {
    (void)asset_dir;
    marauder_init();
    planet_init();
    starfield_init();
}

static void approach_shutdown(void) {
    marauder_shutdown();
    planet_shutdown();
}

static void approach_enter(void) {
    // The system's sun off to the right and a little ahead: the planet
    // shows a lit limb and a night side, the ships are rim-lit.
    se_light_set(&(se_light_t){.x = 1200.0f, .y = 300.0f, .z = -900.0f, .brightness = 0.85f, .two_sided = true});
}

static void approach_camera(double td) {
    float const  t      = (float)td;
    vec3_t const centre = formation_centre(&FORMATION, t);
    vec3_t const eye    = v3_add(centre, formation_to_world(&FORMATION, (vec3_t)CAM_OFFSET));
    vec3_t const look   = v3_add(centre, formation_to_world(&FORMATION, (vec3_t)CAM_LOOK));
    camera_look_at(eye, look, 0.0f);
}

static void approach_submit(double td) {
    float const t = (float)td;
    starfield_submit();
    // The planet moves with nothing: it is 1500 units off, so the 84 units
    // the formation covers barely change it -- which is the point.
    xform_t const planet = {mat3_rot_y(PLANET_SPIN * t), PLANET_POS, PLANET_RADIUS};
    planet_submit(&planet, PLANET_TERRAN);

    xform_t const green  = formation_pose(&FORMATION, &SLOT_GREEN, t, MARAUDER_SPAN);
    xform_t const yellow = formation_pose(&FORMATION, &SLOT_YELLOW, t, MARAUDER_SPAN);
    marauder_submit(&green, MARAUDER_GREEN, 1.0f, td, 1u);
    marauder_submit(&yellow, MARAUDER_YELLOW, 1.0f, td, 2u);
}

scene_def_t const SCENE_MARAUDER_APPROACH = {
    .name     = "marauder_approach",
    .duration = SCENE_SECS,
    .init     = approach_init,
    .shutdown = approach_shutdown,
    .enter    = approach_enter,
    .camera   = approach_camera,
    .submit   = approach_submit,
};
