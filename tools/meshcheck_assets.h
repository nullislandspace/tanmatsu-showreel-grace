// =====================================================================
//  Showreel  --  asset generators under the mesh check
// ---------------------------------------------------------------------
//  Included by tools/meshcheck.c. Each asset that builds a mesh exposes
//  a pure *_build_mesh() (engine-free, in assets/<name>_mesh.c); add it
//  here, and its source file to MESHCHECK_SRCS in the Makefile.
// =====================================================================

#include "assets/marauder_mesh.h"
#include "assets/station_mesh.h"
#include "objects/ship_model.h"

// The vendored player ship, straight from its header. Informational
// only: it is an export from synthracer's 3MF, not built here, so a
// failure is reported but does not fail the run.
static void check_player_ship_model(void) {
    mesh_t m;
    mesh_init(&m);
    for (size_t i = 0; i < SHIP_MODEL_VERT_COUNT; i++) {
        mesh_vert(&m, v3(SHIP_MODEL_VERTS[i].x, SHIP_MODEL_VERTS[i].y, SHIP_MODEL_VERTS[i].z));
    }
    float const uv[3][2] = {{0}};
    for (size_t i = 0; i < SHIP_MODEL_TRI_COUNT; i++) {
        mesh_tri(&m, SHIP_MODEL_TRIS[i].a, SHIP_MODEL_TRIS[i].b, SHIP_MODEL_TRIS[i].c, 0, uv);
    }
    int const before = s_fail;
    check_mesh("player ship model (vendored, informational)", &m, true);
    if (s_fail != before) {
        printf("  (not counted: fix in tanmatsu-synthracer-grace's export, not here)\n");
        s_fail = before;
    }
    mesh_free(&m);
}

static void check_assets(void) {
    check_player_ship_model();

    mesh_t m;
    station_build_mesh(&m);
    // Hub, docking port, 8 spokes, ring: 11 closed parts.
    int const station_solids = check_mesh("station", &m, true);
    CHECK(station_solids == 3 + STATION_SPOKES, "station: expected %d parts", 3 + STATION_SPOKES);
    check_recorded_parts("station", &m, station_solids);
    mesh_free(&m);

    marauder_build_mesh(&m);
    // Fuselage, canopy, and per side: wing, fin, nacelle, gun.
    int const marauder_solids = check_mesh("marauder", &m, true);
    CHECK(marauder_solids == 2 + 2 * 4, "marauder: expected 10 parts");
    check_recorded_parts("marauder", &m, marauder_solids);
    mesh_free(&m);
}
