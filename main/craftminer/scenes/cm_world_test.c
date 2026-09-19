// =====================================================================
//  CraftMiner  --  cm_world_test (development, not in the playlist)
// ---------------------------------------------------------------------
//  The block world on its own, to tune the level of detail (voxel_
//  render.h) on the badge before any scene is built on it. Three shots:
//
//    overview  high over the meadow, looking across the world to the
//              cliff and the lake: the far, flat chunks and the fog;
//    walk      at eye height over the meadow towards the cliff: the
//              near, textured chunks, the plants;
//    tree      round a tree close up: the cut-out leaves (the sky and
//              the trunk through the gaps);
//    props     the miner walking over the meadow, a block breaking by
//              him (cracks, chips, the dropped item), the sun and clouds;
//    fp        first person at the cliff: the arm swinging the pickaxe,
//              the aimed block's outline and cracks.
// =====================================================================

#include <math.h>
#include <stddef.h>
#include "camera.h"
#include "craftminer/assets/miner.h"
#include "craftminer/craftminer.h"
#include "craftminer/voxel/voxel_fx.h"
#include "craftminer/voxel/voxel_render.h"
#include "craftminer/voxel/voxel_sky.h"
#include "synthengine3d.h"

#define SCENE_SECS 28.0f
#define SHOT_WALK  6.0f
#define SHOT_TREE  12.0f
#define SHOT_PROPS 18.0f
#define SHOT_FP    24.0f

// The sun, where the light comes from.
#define SUN_X -700.0f
#define SUN_Y 1100.0f
#define SUN_Z -300.0f
#define EYE   1.62f  // eye height over the ground, blocks

static vox_view_t const VIEW = VOX_VIEW_DEFAULT;

// The props shot's stone block: placed from the start, mined at 2.5 s
// into the shot (the edit and re-mesh path).
#define BLOCK_X     (VOX_MEADOW_X + 2)
#define BLOCK_Y     (VOX_MEADOW_Y + 1)
#define BLOCK_Z     (VOX_MEADOW_Z + 2)
#define BLOCK_BREAK (SHOT_PROPS + 2.5f)
static vox_edit_t const EDITS[] = {
    {0.0f, BLOCK_X, BLOCK_Y, BLOCK_Z, VB_STONE},
    {BLOCK_BREAK, BLOCK_X, BLOCK_Y, BLOCK_Z, VB_AIR},
};

static int s_tree_x, s_tree_z, s_tree_y;  // a trunk near the meadow, found at init

// The trunk nearest the meadow's north-east edge: the trees are placed
// by hash, so look for one instead of guessing.
static void find_tree(void) {
    int best = 1 << 30;
    for (int z = 0; z < VOX_D; z++) {
        for (int x = 0; x < VOX_W; x++) {
            int const g = voxel_ground(x, z);
            if (g <= 0 || voxel_block(x, g - 1, z) != VB_LEAVES) continue;
            for (int y = g - 1; y > 0; y--) {
                if (voxel_block(x, y, z) == VB_LOG && voxel_block(x, y - 1, z) != VB_LOG) {
                    int const dx = x - (VOX_MEADOW_X + 16), dz = z - (VOX_MEADOW_Z + 4);
                    if (dx * dx + dz * dz < best) {
                        best     = dx * dx + dz * dz;
                        s_tree_x = x, s_tree_z = z, s_tree_y = y;
                    }
                    break;
                }
            }
        }
    }
}

static void world_test_init(char const* asset_dir) {
    (void)asset_dir;
    if (voxel_render_init()) find_tree();
    miner_init();
    voxel_fx_init();
}

static void world_test_shutdown(void) {
    voxel_fx_shutdown();
    miner_shutdown();
    voxel_render_shutdown();
}

static void world_test_enter(void) {
    // A high sun from the west-south-west, behind the cameras that look
    // east at the cliff: tops bright, faces turned away from it darker --
    // one-sided, so each face keeps its own shade.
    se_light_set(&(se_light_t){.x = SUN_X, .y = SUN_Y, .z = SUN_Z, .brightness = 0.55f, .two_sided = false});
}

static float ground_at(float x, float z) {
    return (float)voxel_ground((int)floorf(x), (int)floorf(z));
}

static void world_test_camera(double td) {
    float const t = (float)td;
    if (t < SHOT_WALK) {
        float const  a   = 0.6f + 0.05f * t;
        vec3_t const eye = v3(VOX_MEADOW_X - 26.0f * cosf(a), 34.0f, VOX_MEADOW_Z - 26.0f * sinf(a));
        camera_look_at(eye, v3(VOX_CLIFF_X + 6.0f, 12.0f, VOX_CLIFF_Z - 4.0f), 0.0f);
    } else if (t < SHOT_TREE) {
        float const  s = (t - SHOT_WALK) / (SHOT_TREE - SHOT_WALK);
        float const  x = VOX_MEADOW_X - 8.0f + 18.0f * s, z = VOX_MEADOW_Z - 2.0f + 6.0f * s;
        vec3_t const eye = v3(x, ground_at(x, z) + EYE, z);
        camera_look_at(eye, v3(VOX_CLIFF_X, VOX_MEADOW_Y + 3.0f, VOX_CLIFF_Z), 0.0f);
    } else if (t >= SHOT_FP) {
        // Facing the cliff face at arm's length, a little to its left.
        vec3_t const eye = v3(VOX_CLIFF_X - 1.6f, VOX_MEADOW_Y + 1.0f + EYE, VOX_CLIFF_Z + 1.5f);
        camera_look_at(eye, v3(VOX_CLIFF_X + 0.5f, VOX_MEADOW_Y + 2.4f, VOX_CLIFF_Z + 1.9f), 0.0f);
    } else if (t >= SHOT_PROPS) {
        // Beside the miner, who walks east over the meadow.
        float const  s   = t - SHOT_PROPS;
        vec3_t const m   = v3(VOX_MEADOW_X - 3.0f + 1.4f * s, VOX_MEADOW_Y + 1.0f, VOX_MEADOW_Z);
        vec3_t const eye = v3(m.x + 1.5f, m.y + 2.0f, m.z - 5.0f);
        camera_look_at(eye, v3(m.x + 1.2f, m.y + 1.1f, m.z), 0.0f);
    } else {
        float const  a = 0.4f * (t - SHOT_TREE);
        float const  x = (float)s_tree_x + 0.5f + 4.5f * cosf(a), z = (float)s_tree_z + 0.5f + 4.5f * sinf(a);
        vec3_t const eye = v3(x, ground_at(x, z) + 2.4f, z);
        camera_look_at(eye, v3((float)s_tree_x + 0.5f, (float)s_tree_y + 4.0f, (float)s_tree_z + 0.5f), 0.0f);
    }
}

static void world_test_submit(double td) {
    float const t = (float)td;
    voxel_sky_submit(t, v3_norm(v3(SUN_X, SUN_Y, SUN_Z)), VOX_SKY_ARGB, 1.0f);
    voxel_render_submit(EDITS, 2, t, &VIEW);
    if (t >= SHOT_FP) {
        float const phase = 1.1f * (t - SHOT_FP);
        voxel_fx_outline(VOX_CLIFF_X, VOX_MEADOW_Y + 2, VOX_CLIFF_Z + 1);
        voxel_fx_cracks(VOX_CLIFF_X, VOX_MEADOW_Y + 2, VOX_CLIFF_Z + 1, fminf(0.3f * (t - SHOT_FP), 1.0f), 7u);
        miner_submit_fp_arm(miner_stroke(phase), MINER_HOLD_PICK, 0, 0.0f);
    } else if (t >= SHOT_PROPS) {
        float const        s    = t - SHOT_PROPS;
        vec3_t const       m    = v3(VOX_MEADOW_X - 3.0f + 1.4f * s, VOX_MEADOW_Y + 1.0f, VOX_MEADOW_Z);
        xform_t const      root = {mat3_rot_y(1.5707963f), m, 1.0f};  // facing +x
        miner_pose_t const pose = {.walk = 5.0f * s, .stride = 1.0f, .hold = MINER_HOLD_PICK, .head_pitch = 0.1f};
        miner_submit(&root, &pose);
        // The stone block ahead of him cracks, breaks, drops its item,
        // which he picks up 2 s later.
        float const since = t - BLOCK_BREAK;
        if (since < 0.0f) voxel_fx_cracks(BLOCK_X, BLOCK_Y, BLOCK_Z, s / 2.5f, 3u);
        voxel_fx_break(BLOCK_X, BLOCK_Y, BLOCK_Z, VB_STONE, since, 3u);
        voxel_fx_item(BLOCK_X, BLOCK_Y, BLOCK_Z, VB_STONE, since, (float)BLOCK_Y, 2.0f, v3_add(m, v3(0, 1.0f, 0)));
    }
}

static char const* world_test_shot(double t) {
    return t < SHOT_WALK ? "overview" : t < SHOT_TREE ? "walk" : t < SHOT_PROPS ? "tree" : t < SHOT_FP ? "props" : "fp";
}

scene_def_t const SCENE_CM_WORLD_TEST = {
    .name        = "cm_world_test",
    .duration    = SCENE_SECS,
    .init        = world_test_init,
    .shutdown    = world_test_shutdown,
    .enter       = world_test_enter,
    .camera      = world_test_camera,
    .submit      = world_test_submit,
    .shot        = world_test_shot,
    .backdrop    = {.sky_argb = VOX_SKY_ARGB, .ground_argb = VOX_SKY_ARGB, .ground = true},
    .depth_order = true,
    .quarter     = true,
};
