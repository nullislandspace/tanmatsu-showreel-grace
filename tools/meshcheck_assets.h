// =====================================================================
//  Showreel  --  asset generators under the mesh check
// ---------------------------------------------------------------------
//  Included by tools/meshcheck.c. Each asset that builds a mesh exposes
//  a pure *_build_mesh() (engine-free, in assets/<name>_mesh.c); add it
//  here, and its source file to MESHCHECK_SRCS in the Makefile.
// =====================================================================

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
}
