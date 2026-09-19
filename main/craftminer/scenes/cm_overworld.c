// =====================================================================
//  CraftMiner  --  cm_overworld (scene 2)
// ---------------------------------------------------------------------
//  The establishing shot: a long, low flight over the block world in the
//  afternoon -- in from the south-east over the lake and its sandy
//  shore, over the wooded hills, past the flat plot where the cabin will
//  stand, out over the meadow, turning at the end to face the cliff
//  where the miner will dig. Clouds drift over, the sun is behind the
//  camera.
// =====================================================================

#include <math.h>
#include <stddef.h>
#include "camera.h"
#include "craftminer/craftminer.h"
#include "craftminer/scenes/cm_common.h"

#define SCENE_SECS 12.0f
#define LOOK_AHEAD 3.5f  // seconds: the camera looks where it will be
#define LOOK_DOWN  9.0f  // ... this far below that
#define TURN0      8.0f  // then it turns to the cliff
#define TURN1      11.5f

// The flight, a point every 3 s.
static vec3_t const PTS[] = {
    {112.0f, 31.0f, 14.0f}, {92.0f, 28.0f, 28.0f}, {72.0f, 26.0f, 44.0f}, {52.0f, 24.0f, 56.0f}, {38.0f, 22.0f, 70.0f},
};
static path_t const PATH = {PTS, 5, 0.0f, 3.0f};

static void overworld_init(char const* asset_dir) {
    (void)asset_dir;
    cm_init();
}

static void overworld_shutdown(void) {
    cm_shutdown();
}

static void overworld_camera(double td) {
    float const         t = (float)td;
    cm_daylight_t const d = cm_daylight(1.0f);
    cm_light(&d);
    vec3_t const eye    = path_pos(&PATH, t);
    vec3_t       ahead  = path_pos(&PATH, t + LOOK_AHEAD);
    ahead.y            -= LOOK_DOWN;
    vec3_t const cliff  = v3((float)VOX_CLIFF_X + 2.0f, (float)VOX_MEADOW_Y + 4.0f, (float)VOX_CLIFF_Z);
    camera_look_at(eye, v3_lerp(ahead, cliff, smoothstep(TURN0, TURN1, t)), 0.0f);
}

static void overworld_submit(double td) {
    float const         t    = (float)td;
    cm_daylight_t const d    = cm_daylight(1.0f);
    vox_view_t const    view = cm_view(&d);
    cm_world_submit(t, &d, NULL, 0, &view);
}

scene_def_t const SCENE_CM_OVERWORLD = {
    .name     = "cm_overworld",
    .duration = SCENE_SECS,
    .init     = overworld_init,
    .shutdown = overworld_shutdown,
    .camera   = overworld_camera,
    .submit   = overworld_submit,
    .backdrop = {.sky_argb = VOX_SKY_ARGB, .ground_argb = VOX_SKY_ARGB, .ground = true},
    .quarter  = true,
};
