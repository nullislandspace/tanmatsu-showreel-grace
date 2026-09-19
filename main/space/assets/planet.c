// =====================================================================
//  Showreel asset  --  planets (see planet.h)
// =====================================================================

#include "space/assets/planet.h"
#include "common/texcache.h"
#include "mesh_render.h"

#define PLANET_SEGS  24
#define PLANET_RINGS 12

static mesh_t     s_sphere;
static mesh_mat_t s_mats[PLANET_KIND_COUNT];
static bool       s_ready;

static struct {
    char const* file;
    uint32_t    argb;  // flat, if the map is missing
} const MAPS[PLANET_KIND_COUNT] = {
    [PLANET_TERRAN] = {"space/planet_terran.png", 0xFF7A6A50u},
    [PLANET_GAS]    = {"space/planet_gas.png", 0xFFB08050u},
};

void planet_init(void) {
    if (s_ready) return;
    mesh_init(&s_sphere);
    s_sphere.name = "planet";
    mesh_sphere(&s_sphere, 1.0f, PLANET_SEGS, PLANET_RINGS, 0);
    for (int k = 0; k < PLANET_KIND_COUNT; k++) {
        s_mats[k] = (mesh_mat_t){texcache_get(MAPS[k].file), MAPS[k].argb, 0};
    }
    s_ready = true;
}

void planet_shutdown(void) {
    if (!s_ready) return;
    mesh_free(&s_sphere);
    s_ready = false;
}

void planet_submit(xform_t const* x, planet_kind_t kind) {
    if (!s_ready || kind < 0 || kind >= PLANET_KIND_COUNT) return;
    mesh_submit(&s_sphere, x, &s_mats[kind], 1);
}
