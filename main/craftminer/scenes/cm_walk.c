// =====================================================================
//  CraftMiner  --  cm_walk (scene 3)
// ---------------------------------------------------------------------
//  The miner, pickaxe in hand, walks east across the meadow through the
//  flowers and the tall grass, looking about. The camera tracks beside
//  him from the south, a little ahead, then swings round behind him and
//  looks over his shoulder at where he is going: the cliff.
// =====================================================================

#include <math.h>
#include <stddef.h>
#include "camera.h"
#include "craftminer/assets/miner.h"
#include "craftminer/craftminer.h"
#include "craftminer/scenes/cm_common.h"

#define SCENE_SECS 10.0f
#define SPEED      1.4f  // blocks per second, east
#define STEP_RATE  5.2f  // walk cycle, radians per second
#define START_X    (VOX_MEADOW_X - 9.0f)
#define PATH_Z     (VOX_MEADOW_Z + 2.5f)
#define SWING0     5.0f  // the camera goes round behind him
#define SWING1     7.5f

static vec3_t miner_at(float t) {
    float const x = START_X + SPEED * t;
    return v3(x, cm_ground(x, PATH_Z), PATH_Z);
}

static void walk_init(char const* asset_dir) {
    (void)asset_dir;
    cm_init();
}

static void walk_shutdown(void) {
    cm_shutdown();
}

static void walk_camera(double td) {
    float const         t = (float)td;
    cm_daylight_t const d = cm_daylight(1.0f);
    cm_light(&d);
    // Round him from the south (a little ahead) to the west (behind):
    // the angle is from south, towards east.
    float const  s    = smoothstep(SWING0, SWING1, t);
    float const  a    = 0.3f + (-1.45f - 0.3f) * s;
    float const  r    = 4.6f + 0.6f * s;
    vec3_t const m    = miner_at(t);
    vec3_t const eye  = v3_add(m, v3(r * sinf(a), 1.9f + 0.4f * s, -r * cosf(a)));
    vec3_t const look = v3_add(m, v3(0.8f + 6.0f * s, 1.2f + 0.4f * s, 0.0f));
    camera_look_at(eye, look, 0.0f);
}

static void walk_submit(double td) {
    float const         t    = (float)td;
    cm_daylight_t const d    = cm_daylight(1.0f);
    vox_view_t const    view = cm_view(&d);
    cm_world_submit(t, &d, NULL, 0, &view);
    xform_t const      root = {mat3_rot_y(1.5707963f), miner_at(t), 1.0f};  // facing east (+x)
    miner_pose_t const pose = {
        .walk       = STEP_RATE * t,
        .stride     = 1.0f,
        .hold       = MINER_HOLD_PICK,
        .head_yaw   = 0.35f * sinf(0.7f * t),
        .head_pitch = 0.05f,
    };
    miner_submit(&root, &pose);
}

static char const* walk_shot(double t) {
    return t < SWING0 ? "beside" : "behind";
}

scene_def_t const SCENE_CM_WALK = {
    .name        = "cm_walk",
    .duration    = SCENE_SECS,
    .init        = walk_init,
    .shutdown    = walk_shutdown,
    .camera      = walk_camera,
    .submit      = walk_submit,
    .shot        = walk_shot,
    .backdrop    = {.sky_argb = VOX_SKY_ARGB, .ground_argb = VOX_SKY_ARGB, .ground = true},
    .depth_order = true,
    .quarter     = true,
};
