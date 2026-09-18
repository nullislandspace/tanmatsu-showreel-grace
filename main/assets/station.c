// =====================================================================
//  Showreel asset  --  the 2001-style wheel station (see station.h)
// =====================================================================

#include "assets/station.h"
#include <stdbool.h>
#include "assets/texcache.h"
#include "esp_log.h"
#include "mesh_render.h"

static char const TAG[] = "station";

static mesh_t     s_mesh;
static mesh_mat_t s_mats[STATION_MAT_COUNT];
static bool       s_ready;

void station_init(void) {
    if (s_ready) return;
    station_build_mesh(&s_mesh);
    if (s_mesh.failed) ESP_LOGE(TAG, "out of memory building the station mesh");
    ESP_LOGI(TAG, "mesh: %d verts, %d tris", s_mesh.vn, s_mesh.tn);
    s_mats[STATION_MAT_HULL]  = (mesh_mat_t){texcache_get("station_hull.png"), 0xFFC8CACCu, 0};
    s_mats[STATION_MAT_RING]  = (mesh_mat_t){texcache_get("station_ring.png"), 0xFFC6C8CAu, 0};
    s_mats[STATION_MAT_SPOKE] = (mesh_mat_t){texcache_get("plate_gunmetal.png"), 0xFF5C626Eu, 0};
    s_mats[STATION_MAT_DOCK]  = (mesh_mat_t){NULL, 0xFF0C1016u, 0};
    s_ready                   = true;
}

void station_shutdown(void) {
    mesh_free(&s_mesh);
    s_ready = false;
}

void station_submit(xform_t const* x) {
    if (s_ready) mesh_submit(&s_mesh, x, s_mats, STATION_MAT_COUNT);
}
