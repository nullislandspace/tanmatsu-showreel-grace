// =====================================================================
//  CraftMiner  --  cm_nightfall (scene 6, the last)
// ---------------------------------------------------------------------
//  The finished cabin at sunset. The sky turns orange, then dark blue;
//  the world darkens, the torches by the door flicker, the windows glow
//  warm from inside, the stars come out and the square moon rises. The
//  miner stands by his door and looks up at the sky while the camera
//  pulls slowly back and up.
// =====================================================================

#include <math.h>
#include <stddef.h>
#include "camera.h"
#include "common/starfield.h"
#include "craftminer/assets/miner.h"
#include "craftminer/craftminer.h"
#include "craftminer/scenes/cm_cabin.h"
#include "craftminer/voxel/voxel_fx.h"
#include "synthengine3d.h"

#define SCENE_SECS 12.0f
#define DUSK0      0.0f  // the day value falls from DAY0 at DUSK0 ...
#define DUSK1      8.5f  // ... to night at DUSK1
#define DAY0       0.62f
#define STARS_AT   0.3f  // the stars show once the light is below this
#define GLOW_ARGB  0xFFFFC060u

static cm_place_t s_places[CABIN_MAX];
static vox_edit_t s_edits[CABIN_MAX];
static int        s_n;

static vec3_t const MINER = {CABIN_DOOR_X - 0.9f, CABIN_FLOOR, CABIN_Z0 - 1.6f};

static void nightfall_init(char const* asset_dir) {
    (void)asset_dir;
    cm_init();
    starfield_init();
    s_n = cm_cabin_blocks(s_places, CABIN_MAX);
    for (int i = 0; i < s_n; i++) s_places[i].t = -1.0f;  // standing before the scene starts
    cm_place_edits(s_places, s_n, s_edits);
}

static void nightfall_shutdown(void) {
    cm_shutdown();
}

static cm_daylight_t daylight(float t) {
    return cm_daylight(DAY0 * (1.0f - smoothstep(DUSK0, DUSK1, t)));
}

static backdrop_t const* nightfall_backdrop(double td) {
    static backdrop_t   bd;
    cm_daylight_t const d = daylight((float)td);
    bd                    = (backdrop_t){.sky_argb = d.sky_argb, .ground_argb = d.fog_argb, .ground = true};
    return &bd;
}

static void nightfall_camera(double td) {
    float const         t = (float)td;
    cm_daylight_t const d = daylight(t);
    cm_light(&d);
    float const  s   = smoothstep(0.0f, SCENE_SECS, t);
    vec3_t const eye = v3(CABIN_DOOR_X + 0.5f + 1.5f * s, CABIN_FLOOR + 2.0f + 4.5f * s, CABIN_Z0 - 7.0f - 8.0f * s);
    camera_look_at(eye, v3(CABIN_DOOR_X + 0.5f, CABIN_FLOOR + 1.8f + 1.5f * s, CABIN_Z0 + 1.0f), 0.0f);
}

// A warm glow behind a window: a quad filling the glass's cell half a
// block inside the wall, facing out (the glass's holes show it).
static void window_glow(int x, int y, int z, int nx, int nz, uint32_t argb) {
    float const  cx = (float)x + 0.5f - 0.5f * (float)nx, cz = (float)z + 0.5f - 0.5f * (float)nz;
    vec3_t const across = v3(0.5f * (float)nz, 0.0f, 0.5f * (float)nx);
    vec3_t const lo = v3(cx - across.x, (float)y, cz - across.z), hi = v3(cx + across.x, (float)y, cz + across.z);
    scene_tri(lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, hi.x, hi.y + 1.0f, hi.z, argb, SE_TRI_EMISSIVE);
    scene_tri(lo.x, lo.y, lo.z, hi.x, hi.y + 1.0f, hi.z, lo.x, lo.y + 1.0f, lo.z, argb, SE_TRI_EMISSIVE);
}

static uint32_t dim(uint32_t c, float f) {
    uint32_t out = 0xFF000000u;
    for (int s = 0; s < 24; s += 8) out |= (uint32_t)((float)((c >> s) & 0xFF) * f) << s;
    return out;
}

static void nightfall_submit(double td) {
    float const         t    = (float)td;
    cm_daylight_t const d    = daylight(t);
    vox_view_t const    view = cm_view(&d);
    if (d.light < STARS_AT) starfield_submit_above(0.03f);
    cm_world_submit(t, &d, s_edits, s_n, &view);
    for (int i = 0; i < CABIN_TORCHES; i++)
        voxel_fx_flame(CABIN_TORCH[i][0], CABIN_TORCH[i][1], CABIN_TORCH[i][2], t, 90u + (unsigned)i);

    // The windows light up as it gets dark.
    float const    glow = 1.0f - smoothstep(0.25f, 0.6f, d.light);
    uint32_t const g    = dim(GLOW_ARGB, 0.25f + 0.75f * glow);
    window_glow(CABIN_DOOR_X, CABIN_FLOOR + 1, CABIN_Z1, 0, 1, g);
    window_glow(CABIN_X0, CABIN_FLOOR + 1, VOX_PLOT_Z, -1, 0, g);
    window_glow(CABIN_X1, CABIN_FLOOR + 1, VOX_PLOT_Z, 1, 0, g);

    // The miner by his door, facing out; he looks up at the sky.
    xform_t const      root = {mat3_rot_y(3.1415927f), MINER, 1.0f};
    miner_pose_t const pose = {
        .head_pitch = -0.55f * smoothstep(6.0f, 9.0f, t),
        .head_yaw   = 0.3f * smoothstep(6.0f, 9.0f, t),
    };
    miner_submit(&root, &pose);
}

static char const* nightfall_shot(double t) {
    return t < DUSK1 ? "dusk" : "night";
}

scene_def_t const SCENE_CM_NIGHTFALL = {
    .name        = "cm_nightfall",
    .duration    = SCENE_SECS,
    .init        = nightfall_init,
    .shutdown    = nightfall_shutdown,
    .camera      = nightfall_camera,
    .submit      = nightfall_submit,
    .shot        = nightfall_shot,
    .backdrop_at = nightfall_backdrop,
    .depth_order = true,
    .quarter     = true,
};
