// =====================================================================
//  Showreel asset  --  the planet base (see planet_base.h)
// =====================================================================

#include "assets/planet_base.h"
#include <stddef.h>
#include "assets/flame.h"
#include "assets/planet_base_mesh.h"
#include "assets/texcache.h"
#include "mesh_render.h"
#include "synthengine3d.h"

#define FLARE_LEN 3.5f  // nominal flame length, units; flicker scales it

static mesh_t     s_structures, s_apron, s_ridge;
static backdrop_t s_backdrop = {.sky_argb = PLANET_SKY_ARGB, .ground_argb = PLANET_GROUND_ARGB, .ground = true};
static mesh_mat_t s_mats[BASE_MAT_COUNT];
static bool       s_ready;

void planet_base_init(void) {
    if (s_ready) return;
    planet_base_build_structures(&s_structures);
    planet_base_build_apron(&s_apron);
    planet_base_build_ridge(&s_ridge);
    flame_init();
    s_mats[BASE_MAT_PAD]       = (mesh_mat_t){texcache_get("pad.png"), 0xFF96948Eu, 0};
    s_mats[BASE_MAT_MARK]      = (mesh_mat_t){NULL, 0xFFE8C020u, SE_TRI_EMISSIVE};
    s_mats[BASE_MAT_WALL]      = (mesh_mat_t){texcache_get("industrial_wall.png"), 0xFF767C80u, 0};
    s_mats[BASE_MAT_TANK]      = (mesh_mat_t){texcache_get("station_hull.png"), 0xFFC8CACCu, 0};
    s_mats[BASE_MAT_METAL]     = (mesh_mat_t){texcache_get("plate_gunmetal.png"), 0xFF5C626Eu, 0};
    // Unlit, like the PPA ground it has to meet (planet_base.h).
    se_texture_t const* ground = texcache_get("ground.png");
    s_mats[BASE_MAT_APRON]     = (mesh_mat_t){ground, PLANET_GROUND_ARGB, SE_TRI_EMISSIVE};
    if (ground != NULL) s_backdrop.ground_argb = ground->mean_argb;
    // Hazy and unlit: the far hills are a silhouette a little darker than
    // the sky, lit or not.
    s_mats[BASE_MAT_RIDGE] = (mesh_mat_t){NULL, 0xFF8A7262u, SE_TRI_EMISSIVE};
    s_ready                = true;
}

void planet_base_shutdown(void) {
    if (!s_ready) return;
    mesh_free(&s_structures);
    mesh_free(&s_apron);
    mesh_free(&s_ridge);
    s_ready = false;
}

void planet_base_submit(double t) {
    if (!s_ready) return;
    xform_t const world = {mat3_rot_y(0.0f), v3(0.0f, 0.0f, 0.0f), 1.0f};
    mesh_submit(&s_ridge, &world, s_mats, BASE_MAT_COUNT);
    mesh_submit(&s_apron, &world, s_mats, BASE_MAT_COUNT);
    mesh_submit(&s_structures, &world, s_mats, BASE_MAT_COUNT);

    // Each flare burns straight up: a flame trails along its pose's -z,
    // so the pose's forward is world down.
    mat3_t const up_flame = mat3_from_fwd_up(v3(0.0f, -1.0f, 0.0f), v3(0.0f, 0.0f, 1.0f), 0.0f);
    for (int i = 0; i < BASE_FLARES; i++) {
        xform_t const x   = {up_flame, v3(BASE_FLARE_TOP[i][0], BASE_FLARE_TOP[i][1], BASE_FLARE_TOP[i][2]), 1.0f};
        float const   len = FLARE_LEN * flame_flicker(t, 40u + (unsigned)i);
        flame_submit(&x, v3(0.0f, 0.0f, 0.0f), BASE_FLARE_R * 0.9f, len, FLAME_RED);
    }
}

backdrop_t const* planet_base_backdrop(void) {
    return &s_backdrop;
}

vec3_t planet_base_pad_centre(void) {
    return v3(0.0f, BASE_PAD_TOP, 0.0f);
}

bool planet_base_raycast(vec3_t from, vec3_t dir, float max, float* dist) {
    if (!s_ready) return false;
    xform_t const world = {mat3_rot_y(0.0f), v3(0.0f, 0.0f, 0.0f), 1.0f};
    bool          hit   = false;
    // Each cast only looks nearer than the last hit.
    if (mesh_raycast(&s_structures, &world, from, dir, max, &max)) hit = true;
    if (mesh_raycast(&s_apron, &world, from, dir, max, &max)) hit = true;
    if (mesh_raycast(&s_ridge, &world, from, dir, max, &max)) hit = true;
    if (hit && dist) *dist = max;
    return hit;
}
