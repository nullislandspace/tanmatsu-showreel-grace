// =====================================================================
//  CraftMiner  --  cm_building (scene 5)
// ---------------------------------------------------------------------
//  The cabin goes up (cm_cabin.h). First person: log in hand, the miner
//  sets down the four corner posts, looking from one corner to the next.
//  Then a time-lapse from outside, the camera circling: the walls rise
//  layer by layer, the windows go in, the roof steps up, and the torches
//  go in either side of the door -- while the miner, by the doorway,
//  keeps placing.
// =====================================================================

#include <math.h>
#include <stddef.h>
#include "camera.h"
#include "craftminer/assets/miner.h"
#include "craftminer/craftminer.h"
#include "craftminer/scenes/cm_cabin.h"

#define SCENE_SECS 16.0f
#define SHOT_LAPSE 4.2f  // first person -> the time-lapse
#define CORNER_T0  0.9f  // the corner posts pop in at 0.9, 1.7, 2.5, 3.3 s
#define CORNER_DT  0.8f
#define LAPSE_END  15.2f  // the last block
#define STROKE     0.55f  // seconds of a placing swing (its downstroke lands on the block)

static cm_place_t s_places[CABIN_MAX];
static vox_edit_t s_edits[CABIN_MAX];
static int        s_n;

static vec3_t const CENTRE = {VOX_PLOT_X + 0.5f, CABIN_FLOOR + 2.0f, VOX_PLOT_Z + 0.5f};
// The first-person eye: outside the south-west corner.
static vec3_t const EYE    = {CABIN_X0 - 2.3f, CABIN_FLOOR + MINER_EYE, CABIN_Z0 - 2.4f};
// The miner in the time-lapse: in front of the doorway, facing it.
static vec3_t const MINER  = {CABIN_DOOR_X + 0.5f, CABIN_FLOOR, CABIN_Z0 - 3.2f};

static void building_init(char const* asset_dir) {
    (void)asset_dir;
    cm_init();
    s_n = cm_cabin_blocks(s_places, CABIN_MAX);
    // The corner posts by hand, the rest in the time-lapse, evenly.
    for (int i = 0; i < s_n; i++) {
        s_places[i].t = i < 4 ? CORNER_T0 + CORNER_DT * (float)i
                              : SHOT_LAPSE + 0.2f + (LAPSE_END - SHOT_LAPSE - 0.2f) * (float)(i - 4) / (float)(s_n - 5);
    }
    cm_place_edits(s_places, s_n, s_edits);
}

static void building_shutdown(void) {
    cm_shutdown();
}

// The swing that lands a block at each placement time `tp`: its
// downstroke (miner_stroke's second half) ends on the block.
static float place_swing(float t) {
    for (int i = 0; i < 4; i++) {
        float const phase = (t - s_places[i].t) / STROKE + 0.5f;
        if (phase >= 0.0f && phase < 1.0f) return miner_stroke(phase);
    }
    return 0.0f;
}

// First person: the corner being placed (turning to the next before it).
static vec3_t aim_at(float t) {
    vec3_t aim = v3(s_places[0].x + 0.5f, s_places[0].y + 0.5f, s_places[0].z + 0.5f);
    for (int i = 1; i < 4; i++) {
        vec3_t const next = v3(s_places[i].x + 0.5f, s_places[i].y + 0.5f, s_places[i].z + 0.5f);
        aim               = v3_lerp(aim, next, smoothstep(s_places[i].t - 0.55f, s_places[i].t - 0.15f, t));
    }
    return aim;
}

static void building_camera(double td) {
    float const         t = (float)td;
    cm_daylight_t const d = cm_daylight(1.0f);
    cm_light(&d);
    if (t < SHOT_LAPSE) {
        camera_look_at(EYE, aim_at(t), 0.0f);
    } else {
        // Round the cabin from the south-west to the south-east.
        float const  a   = -0.75f + 1.5f * smoothstep(SHOT_LAPSE, SCENE_SECS, t);
        vec3_t const eye = v3_add(CENTRE, v3(12.5f * sinf(a), 4.5f, -12.5f * cosf(a)));
        camera_look_at(eye, CENTRE, 0.0f);
    }
}

static void building_submit(double td) {
    float const         t    = (float)td;
    cm_daylight_t const d    = cm_daylight(1.0f);
    vox_view_t const    view = cm_view(&d);
    cm_world_submit(t, &d, s_edits, s_n, &view);
    cm_place_submit(s_places, s_n, t);
    if (t < SHOT_LAPSE) {
        miner_submit_fp_arm(place_swing(t), MINER_HOLD_BLOCK, VB_LOG, 0.0f);
    } else {
        xform_t const      root = {mat3_rot_y(0.0f), MINER, 1.0f};  // facing north, at the cabin
        float const        busy = t < LAPSE_END ? 1.0f : 0.0f;
        miner_pose_t const pose = {
            .swing      = busy * miner_stroke(2.2f * t),
            .hold       = busy > 0.0f ? MINER_HOLD_BLOCK : MINER_HOLD_NONE,
            .hold_block = VB_PLANKS,
            .head_pitch = -0.25f,
        };
        miner_submit(&root, &pose);
    }
}

static char const* building_shot(double t) {
    return t < SHOT_LAPSE ? "corners" : "timelapse";
}

scene_def_t const SCENE_CM_BUILDING = {
    .name        = "cm_building",
    .duration    = SCENE_SECS,
    .init        = building_init,
    .shutdown    = building_shutdown,
    .camera      = building_camera,
    .submit      = building_submit,
    .shot        = building_shot,
    .backdrop    = {.sky_argb = VOX_SKY_ARGB, .ground_argb = VOX_SKY_ARGB, .ground = true},
    .depth_order = true,
    .quarter     = true,
};
