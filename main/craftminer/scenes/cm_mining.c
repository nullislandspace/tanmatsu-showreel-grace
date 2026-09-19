// =====================================================================
//  CraftMiner  --  cm_mining (scene 4)
// ---------------------------------------------------------------------
//  At the foot of the cliff. Third person first: the miner swings his
//  pickaxe at the stone face -- the aimed block outlined, cracks
//  spreading, the block bursting into chips and dropping a little copy
//  of itself that flies to him. Two blocks out, a doorway's worth.
//  Then first person: behind the stone, the coal seam; the coal comes
//  out, then the stone under it, a step into the new tunnel, and two
//  more blocks deeper in, the coal seam's black flecks all round.
// =====================================================================

#include <math.h>
#include <stddef.h>
#include "camera.h"
#include "craftminer/assets/miner.h"
#include "craftminer/craftminer.h"
#include "craftminer/scenes/cm_common.h"

#define SCENE_SECS 14.0f
#define SHOT_FP    5.5f  // third person -> first person
#define STEP0      9.6f  // the step into the tunnel
#define STEP1      10.6f
#define AIM_TURN   0.45f  // seconds to turn from one block to the next

#define FACE_X VOX_CLIFF_X
#define FEET_Y (VOX_MEADOW_Y + 1)
#define ROW_Z  (VOX_CLIFF_Z + 1)

static cm_dig_t const DIGS[] = {
    {0.6f, 2.0f, FACE_X, FEET_Y + 1, ROW_Z, VB_STONE, 0.6f},
    {2.4f, 3.8f, FACE_X, FEET_Y, ROW_Z, VB_STONE, 0.6f},
    {6.0f, 7.4f, FACE_X + 1, FEET_Y + 1, ROW_Z, VB_COAL, 0.5f},
    {7.8f, 9.2f, FACE_X + 1, FEET_Y, ROW_Z, VB_STONE, 0.5f},
    {10.8f, 12.2f, FACE_X + 2, FEET_Y + 1, ROW_Z, VB_STONE, 0.5f},
    {12.4f, 13.8f, FACE_X + 2, FEET_Y, ROW_Z, VB_STONE, 0.5f},
};
#define DIG_N ((int)(sizeof(DIGS) / sizeof(DIGS[0])))
static vox_edit_t s_edits[DIG_N];

// The miner (third person) and the eye (first person), both facing east.
static vec3_t const MINER = {FACE_X - 1.4f, FEET_Y, ROW_Z + 0.5f};

static vec3_t eye_at(float t) {
    float const x = FACE_X - 2.2f + 1.0f * smoothstep(STEP0, STEP1, t);
    return v3(x, FEET_Y + MINER_EYE, ROW_Z + 0.5f);
}

// Where the miner looks: the middle of the block he is on, turning to
// the next one just before he starts on it.
static vec3_t aim_at(float t) {
    vec3_t aim = v3(DIGS[0].x + 0.5f, DIGS[0].y + 0.5f, DIGS[0].z + 0.5f);
    for (int i = 1; i < DIG_N; i++) {
        vec3_t const next = v3(DIGS[i].x + 0.5f, DIGS[i].y + 0.5f, DIGS[i].z + 0.5f);
        aim               = v3_lerp(aim, next, smoothstep(DIGS[i].start - AIM_TURN, DIGS[i].start, t));
    }
    return aim;
}

static void mining_init(char const* asset_dir) {
    (void)asset_dir;
    cm_init();
    cm_dig_edits(DIGS, DIG_N, s_edits);
}

static void mining_shutdown(void) {
    cm_shutdown();
}

static void mining_camera(double td) {
    float const         t = (float)td;
    cm_daylight_t const d = cm_daylight(1.0f);
    cm_light(&d);
    if (t < SHOT_FP) {
        // From the south-west, level with his shoulders: him, the face,
        // the hole he makes.
        camera_look_at(v3(FACE_X - 3.6f, FEET_Y + 2.4f, ROW_Z - 3.6f), v3(FACE_X - 0.4f, FEET_Y + 1.1f, ROW_Z + 0.6f),
                       0.0f);
    } else {
        camera_look_at(eye_at(t), aim_at(t), 0.0f);
    }
}

static void mining_submit(double td) {
    float const         t    = (float)td;
    cm_daylight_t const d    = cm_daylight(1.0f);
    vox_view_t const    view = cm_view(&d);
    cm_world_submit(t, &d, s_edits, DIG_N, &view);
    float const swing = cm_dig_swing(DIGS, DIG_N, t);
    if (t < SHOT_FP) {
        vec3_t const       aim   = aim_at(t);
        vec3_t const       head  = v3_add(MINER, v3(0.0f, MINER_EYE, 0.0f));
        float const        pitch = atan2f(head.y - aim.y, aim.x - head.x);
        xform_t const      root  = {mat3_rot_y(1.5707963f), MINER, 1.0f};
        miner_pose_t const pose  = {.swing = swing, .hold = MINER_HOLD_PICK, .head_pitch = pitch};
        miner_submit(&root, &pose);
        cm_dig_submit(DIGS, DIG_N, t, v3_add(MINER, v3(0.0f, 1.0f, 0.0f)));
    } else {
        vec3_t const eye = eye_at(t);
        cm_dig_submit(DIGS, DIG_N, t, v3_add(eye, v3(0.3f, -0.7f, 0.0f)));
        float const walking = smoothstep(STEP0, STEP0 + 0.2f, t) * (1.0f - smoothstep(STEP1 - 0.2f, STEP1, t));
        miner_submit_fp_arm(swing, MINER_HOLD_PICK, 0, 0.04f * walking * fabsf(sinf(9.0f * t)));
    }
}

static char const* mining_shot(double t) {
    return t < SHOT_FP ? "dig" : "tunnel";
}

scene_def_t const SCENE_CM_MINING = {
    .name        = "cm_mining",
    .duration    = SCENE_SECS,
    .init        = mining_init,
    .shutdown    = mining_shutdown,
    .camera      = mining_camera,
    .submit      = mining_submit,
    .shot        = mining_shot,
    .backdrop    = {.sky_argb = VOX_SKY_ARGB, .ground_argb = VOX_SKY_ARGB, .ground = true},
    .depth_order = true,
    .quarter     = true,
};
