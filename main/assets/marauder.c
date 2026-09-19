// =====================================================================
//  Showreel asset  --  the marauder (see marauder.h)
// =====================================================================

#include "assets/marauder.h"
#include <stdbool.h>
#include <stddef.h>
#include "assets/flame.h"
#include "assets/marauder_mesh.h"
#include "assets/texcache.h"
#include "esp_log.h"
#include "mesh_render.h"

static char const TAG[] = "marauder";

// Nominal flame length (model units); flicker and throttle scale it.
#define FLAME_LEN 0.30f

static char const* const LIVERY_FILE[MARAUDER_LIVERY_COUNT] = {
    [MARAUDER_GREEN]  = "marauder_green.png",
    [MARAUDER_YELLOW] = "marauder_yellow.png",
};
// Flat stand-ins if a livery texture is missing.
static uint32_t const LIVERY_ARGB[MARAUDER_LIVERY_COUNT] = {
    [MARAUDER_GREEN]  = 0xFF465E38u,
    [MARAUDER_YELLOW] = 0xFF967C26u,
};

static mesh_t     s_mesh;
static mesh_mat_t s_mats[MARAUDER_LIVERY_COUNT][MARAUDER_MAT_COUNT];
static bool       s_ready;

void marauder_init(void) {
    if (s_ready) return;
    marauder_build_mesh(&s_mesh);
    if (s_mesh.failed) ESP_LOGE(TAG, "out of memory building the marauder mesh");
    ESP_LOGI(TAG, "mesh: %d verts, %d tris", s_mesh.vn, s_mesh.tn);
    flame_init();
    se_texture_t const* metal = texcache_get("plate_gunmetal.png");
    for (int l = 0; l < MARAUDER_LIVERY_COUNT; l++) {
        s_mats[l][MARAUDER_MAT_PAINT]  = (mesh_mat_t){texcache_get(LIVERY_FILE[l]), LIVERY_ARGB[l], 0};
        s_mats[l][MARAUDER_MAT_METAL]  = (mesh_mat_t){metal, 0xFF5C626Eu, 0};
        s_mats[l][MARAUDER_MAT_CANOPY] = (mesh_mat_t){NULL, 0xFF1C2E44u, 0};
        s_mats[l][MARAUDER_MAT_NOZZLE] = (mesh_mat_t){NULL, 0xFF141414u, 0};
    }
    s_ready = true;
}

void marauder_shutdown(void) {
    mesh_free(&s_mesh);
    s_ready = false;
}

void marauder_submit(xform_t const* x, marauder_livery_t livery, float throttle, double t, unsigned flicker_seed) {
    if (!s_ready || (unsigned)livery >= MARAUDER_LIVERY_COUNT) return;
    mesh_submit(&s_mesh, x, s_mats[livery], MARAUDER_MAT_COUNT);
    if (throttle <= 0.0f) return;
    float const len = FLAME_LEN * throttle;
    for (int side = 0; side < 2; side++) {
        float const  s      = side ? 1.0f : -1.0f;
        vec3_t const nozzle = v3(s * MARAUDER_NOZZLE_X, MARAUDER_NOZZLE_Y, MARAUDER_NOZZLE_Z);
        flame_submit(x, nozzle, MARAUDER_NOZZLE_R, len * flame_flicker(t, flicker_seed * 2u + (unsigned)side),
                     FLAME_RED);
    }
}

// Debris: how fast parts leave the centre (spans per second), and how
// fast they tumble (radians per second), each seeded within these.
#define DEBRIS_SPEED_MIN 0.8f
#define DEBRIS_SPEED_MAX 2.4f
#define DEBRIS_SPIN_MAX  9.0f

void marauder_submit_debris(xform_t const* at, vec3_t vel, marauder_livery_t livery, float t, float t_explode,
                            unsigned seed) {
    if (!s_ready || (unsigned)livery >= MARAUDER_LIVERY_COUNT || t < t_explode) return;
    float const u = t - t_explode;
    for (int i = 0; i < s_mesh.pn; i++) {
        vec3_t const  c     = mesh_part_centre(&s_mesh, i);  // model space
        // Out from the ship's centre through the part's own, jittered.
        vec3_t const  j     = v3(hash01(i, seed) - 0.5f, hash01(i, seed + 1u) - 0.5f, hash01(i, seed + 2u) - 0.5f);
        vec3_t const  out   = v3_norm(v3_add(c, v3_scale(j, 0.6f)));
        float const   sp    = DEBRIS_SPEED_MIN + (DEBRIS_SPEED_MAX - DEBRIS_SPEED_MIN) * hash01(i, seed + 3u);
        vec3_t const  k     = v3_norm(v3(hash01(i, seed + 4u) - 0.5f, hash01(i, seed + 5u) - 0.5f, 0.3f));
        mat3_t const  rot   = mat3_axis_angle(k, DEBRIS_SPIN_MAX * (hash01(i, seed + 6u) - 0.5f) * u);
        // world(p) = at(c + R (p - c)) + motion: as an xform, r = at.r R and
        // the translation carries at.r (c - R c).
        mat3_t const  r     = mat3_mul(&at->r, &rot);
        vec3_t const  pivot = v3_scale(v3_sub(c, mat3_apply(&rot, c)), at->scale);
        vec3_t const  fly   = v3_add(v3_scale(vel, u), v3_scale(mat3_apply(&at->r, out), sp * at->scale * u));
        xform_t const x     = {r, v3_add(v3_add(at->pos, mat3_apply(&at->r, pivot)), fly), at->scale};
        mesh_submit_part(&s_mesh, i, &x, s_mats[livery], MARAUDER_MAT_COUNT);
    }
}

vec3_t marauder_gun(xform_t const* x, int side) {
    float const s = side ? 1.0f : -1.0f;
    return xform_apply(x, v3(s * MARAUDER_GUN_X, MARAUDER_GUN_Y, MARAUDER_GUN_Z));
}

bool marauder_raycast(xform_t const* x, vec3_t from, vec3_t dir, float max, float* dist) {
    return s_ready && mesh_raycast(&s_mesh, x, from, dir, max, dist);
}
