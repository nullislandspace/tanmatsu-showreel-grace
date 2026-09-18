// =====================================================================
//  Showreel dev scene  --  horizon (sky/ground backdrop test)
// ---------------------------------------------------------------------
//  Not in the playlist. Exercises the PPA sky/ground backdrop
//  (backdrop.h) under every camera attitude the reel may use:
//
//    0-3 s    level, panning             the horizon is flat: PPA only
//    3-9 s    one full roll              the wedge; upside down (ground
//                                        on top) past 90 degrees
//    9-12 s   pitching up and down       the horizon leaves the screen
//
//  Reference geometry stands on the ground (y = 0): a near ring of posts
//  and a far one 2 km out, whose bases must sit on the painted horizon
//  (0.06 degrees below it from 2 units up: under a pixel).
// =====================================================================

#include <math.h>
#include <stddef.h>
#include "camera.h"
#include "mesh.h"
#include "mesh_render.h"
#include "scenes/scenes.h"
#include "synthengine3d.h"

#define SCENE_SECS 12.0f
#define EYE_Y      2.0f

#define NEAR_POSTS 12
#define NEAR_R     40.0f
#define FAR_POSTS  24
#define FAR_R      2000.0f

enum {
    MAT_POST,
    MAT_FAR,
    MAT_COUNT
};

static mesh_t           s_posts;
static mesh_mat_t const MATS[MAT_COUNT] = {
    [MAT_POST] = {NULL, 0xFFD8D0C0u, 0},
    [MAT_FAR]  = {NULL, 0xFF303848u, SE_TRI_EMISSIVE},
};

static void ring(int n, float r, float half, float height, uint8_t mat) {
    for (int i = 0; i < n; i++) {
        float const   a     = 6.2831853f * (float)i / (float)n;
        int const     first = s_posts.vn;
        xform_t const x     = {mat3_rot_y(a), v3(r * sinf(a), 0.0f, r * cosf(a)), 1.0f};
        mesh_box(&s_posts, v3(-half, 0.0f, -half), v3(half, height, half), mat, 1.0f);
        mesh_transform_from(&s_posts, first, &x);
    }
}

static void horizon_init(char const* asset_dir) {
    (void)asset_dir;
    mesh_init(&s_posts);
    s_posts.name = "posts";
    ring(NEAR_POSTS, NEAR_R, 0.8f, 8.0f, MAT_POST);
    ring(FAR_POSTS, FAR_R, 30.0f, 200.0f, MAT_FAR);
}

static void horizon_shutdown(void) {
    mesh_free(&s_posts);
}

static void horizon_enter(void) {
    se_light_set(&(se_light_t){.x = -500.0f, .y = 400.0f, .z = -300.0f, .brightness = 0.7f, .two_sided = true});
}

static void horizon_camera(double td) {
    float const t     = (float)td;
    float const yaw   = 0.25f * t;
    float       pitch = 0.0f, roll = 0.0f;
    if (t >= 3.0f && t < 9.0f) {
        roll = 6.2831853f * smoothstep(3.0f, 9.0f, t);
    } else if (t >= 9.0f) {
        pitch = 1.05f * sinf(6.2831853f * (t - 9.0f) / 3.0f);  // +-60 degrees
    }
    render_set_camera_6dof(0.0f, EYE_Y, 0.0f, yaw, pitch, roll);
}

static void horizon_submit(double t) {
    (void)t;
    xform_t const x = {mat3_rot_y(0.0f), v3(0.0f, 0.0f, 0.0f), 1.0f};
    mesh_submit(&s_posts, &x, MATS, MAT_COUNT);
}

static char const* horizon_shot(double t) {
    return t < 3.0 ? "level" : t < 9.0 ? "roll" : "pitch";
}

scene_def_t const SCENE_HORIZON_TEST = {
    .name     = "horizon",
    .duration = SCENE_SECS,
    .init     = horizon_init,
    .shutdown = horizon_shutdown,
    .enter    = horizon_enter,
    .camera   = horizon_camera,
    .submit   = horizon_submit,
    .shot     = horizon_shot,
    .backdrop = {.sky_argb = 0xFF7FA8D8u, .ground_argb = 0xFF5A4630u, .ground = true},
};
