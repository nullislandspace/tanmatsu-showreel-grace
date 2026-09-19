// =====================================================================
//  CraftMiner  --  the miner (see miner.h)
// =====================================================================

#include "craftminer/assets/miner.h"
#include <math.h>
#include <stddef.h>
#include "camera.h"
#include "common/texcache.h"
#include "craftminer/assets/miner_mesh.h"
#include "craftminer/voxel/voxel_mesh.h"
#include "craftminer/voxel/voxel_render.h"
#include "mesh_render.h"

static int        s_users;
static mesh_t     s_head, s_body, s_arm, s_leg, s_pick, s_block;
static mesh_mat_t s_mats[MM_COUNT];

void miner_init(void) {
    if (s_users++ > 0) return;
    s_mats[MM_SKIN]     = (mesh_mat_t){NULL, 0xFFDEAA80u, 0};
    s_mats[MM_SHIRT]    = (mesh_mat_t){NULL, 0xFFB8302Au, 0};
    s_mats[MM_OVERALLS] = (mesh_mat_t){NULL, 0xFF2E4C8Cu, 0};
    s_mats[MM_BOOTS]    = (mesh_mat_t){NULL, 0xFF4A3020u, 0};
    s_mats[MM_HAT]      = (mesh_mat_t){NULL, 0xFFF0C020u, 0};
    s_mats[MM_LAMP]     = (mesh_mat_t){NULL, 0xFFFFF8D0u, SE_TRI_EMISSIVE};
    s_mats[MM_HAIR]     = (mesh_mat_t){NULL, 0xFF60402Au, 0};
    s_mats[MM_FACE]     = (mesh_mat_t){texcache_get("craftminer/miner_face.png"), 0xFFDEAA80u, 0};
    s_mats[MM_WOOD]     = (mesh_mat_t){NULL, 0xFF7A5230u, 0};
    s_mats[MM_IRON]     = (mesh_mat_t){NULL, 0xFF9AA0A8u, 0};
    miner_build_head(&s_head);
    miner_build_body(&s_body);
    miner_build_arm(&s_arm);
    miner_build_leg(&s_leg);
    miner_build_pick(&s_pick);
    // A block in the hand: a small one, in the block's textures.
    mesh_init(&s_block);
    s_block.name = "miner_block";
    voxel_build_cube(&s_block, 0.14f);
}

void miner_shutdown(void) {
    if (s_users == 0 || --s_users > 0) return;
    mesh_free(&s_head);
    mesh_free(&s_body);
    mesh_free(&s_arm);
    mesh_free(&s_leg);
    mesh_free(&s_pick);
    mesh_free(&s_block);
}

float miner_stroke(float phase) {
    float const p = phase - floorf(phase);
    if (p < 0.35f) return smoothstep(0.0f, 0.35f, p);        // up
    if (p < 0.5f) return 1.0f - smoothstep(0.35f, 0.5f, p);  // down, hard
    return 0.0f;                                             // rest
}

// A joint: `parent` then a turn `r` about the point `at` (in the parent).
static xform_t joint(xform_t const* parent, vec3_t at, mat3_t r) {
    xform_t const local = {r, at, 1.0f};
    return xform_mul(parent, &local);
}

static mat3_t ident(void) {
    return mat3_rot_x(0.0f);
}

// What the fist holds, in the arm's frame.
static void submit_held(xform_t const* arm, miner_hold_t hold, int hold_block) {
    if (hold == MINER_HOLD_PICK) {
        xform_t const fist = joint(arm, v3(0.0f, MINER_FIST_Y, 0.0f), ident());
        mesh_submit(&s_pick, &fist, s_mats, MM_COUNT);
    } else if (hold == MINER_HOLD_BLOCK) {
        xform_t const at = joint(arm, v3(0.0f, MINER_FIST_Y, 0.16f), mat3_rot_y(0.5f));
        mesh_mat_t    mats[3];
        voxel_cube_mats((uint8_t)hold_block, mats);
        mesh_submit(&s_block, &at, mats, 3);
    }
}

void miner_submit(xform_t const* root, miner_pose_t const* p) {
    xform_t const scaled = {root->r, root->pos, root->scale * MINER_SCALE};
    float const   leg    = 0.55f * p->stride * sinf(p->walk);
    float const   bob    = 0.04f * p->stride * fabsf(sinf(p->walk));
    xform_t const hips   = joint(&scaled, v3(0.0f, MINER_LEG_H + bob, 0.0f), ident());

    // Legs: the right one (on -x) forward when sin(walk) > 0.
    xform_t const leg_r = joint(&hips, v3(-MINER_HIP_X, 0.0f, 0.0f), mat3_rot_x(-leg));
    xform_t const leg_l = joint(&hips, v3(MINER_HIP_X, 0.0f, 0.0f), mat3_rot_x(leg));
    mesh_submit(&s_leg, &leg_r, s_mats, MM_COUNT);
    mesh_submit(&s_leg, &leg_l, s_mats, MM_COUNT);
    mesh_submit(&s_body, &hips, s_mats, MM_COUNT);

    // Arms swing against the legs; the right one also lifts the tool
    // (a turn about x of -angle swings the hand forward and up).
    float const   raise  = p->hold != MINER_HOLD_NONE ? 0.35f + 1.9f * p->swing : 0.0f;
    float const   arm_sw = 0.45f * p->stride * sinf(p->walk);
    vec3_t const  sh_y   = v3(0.0f, MINER_BODY_H - 0.06f, 0.0f);
    xform_t const arm_r  = joint(&hips, v3_add(sh_y, v3(-MINER_SHOULDER_X, 0, 0)), mat3_rot_x(arm_sw - raise));
    xform_t const arm_l  = joint(&hips, v3_add(sh_y, v3(MINER_SHOULDER_X, 0, 0)), mat3_rot_x(-arm_sw));
    mesh_submit(&s_arm, &arm_r, s_mats, MM_COUNT);
    mesh_submit(&s_arm, &arm_l, s_mats, MM_COUNT);
    submit_held(&arm_r, p->hold, p->hold_block);

    mat3_t const  yaw   = mat3_rot_y(p->head_yaw);
    mat3_t const  pitch = mat3_rot_x(p->head_pitch);
    xform_t const head  = joint(&hips, v3(0.0f, MINER_BODY_H, 0.0f), mat3_mul(&yaw, &pitch));
    mesh_submit(&s_head, &head, s_mats, MM_COUNT);
}

void miner_submit_fp_arm(float swing, miner_hold_t hold, int hold_block, float bob) {
    // In the camera's frame (x right, y up, z forward): the shoulder just
    // beyond the near plane at the lower right, the arm reaching forward
    // and in towards the middle; a swing lifts it and brings it down.
    xform_t const cam  = {camera_basis(), camera_eye(), 1.0f};
    mat3_t const  in   = mat3_rot_y(-0.3f);
    mat3_t const  up   = mat3_rot_x(-1.45f - 0.7f * swing);
    xform_t const arm0 = joint(&cam, v3(0.6f, -0.5f + bob, 0.9f), mat3_mul(&in, &up));
    xform_t const arm  = {arm0.r, arm0.pos, 0.75f};  // smaller than life: it is right in front of the lens
    mesh_submit(&s_arm, &arm, s_mats, MM_COUNT);
    submit_held(&arm, hold, hold_block);
}
