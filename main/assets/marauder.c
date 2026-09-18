// =====================================================================
//  Showreel asset  --  the marauder (see marauder.h)
// =====================================================================

#include "assets/marauder.h"
#include <stdbool.h>
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

vec3_t marauder_gun(xform_t const* x, int side) {
    float const s = side ? 1.0f : -1.0f;
    return xform_apply(x, v3(s * MARAUDER_GUN_X, MARAUDER_GUN_Y, MARAUDER_GUN_Z));
}
