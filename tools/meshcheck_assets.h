// =====================================================================
//  Showreel  --  asset generators under the mesh check
// ---------------------------------------------------------------------
//  Included by tools/meshcheck.c. Each asset that builds a mesh exposes
//  a pure *_build_mesh() (engine-free, in assets/<name>_mesh.c); add it
//  here, and its source file to MESHCHECK_SRCS in the Makefile.
// =====================================================================

#include "assets/asteroid_mesh.h"
#include "assets/marauder_mesh.h"
#include "assets/planet_base_mesh.h"
#include "assets/station_mesh.h"
#include "assets/title_text_mesh.h"
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

    // The planet base: its structures are closed solids; the apron and
    // the ridge are open surfaces that must face the way they claim.
    planet_base_build_structures(&m);
    // Pad + 4 markings, 2 halls, 3 tanks, 2 chimneys, 3 rack pieces,
    // 2 x (stack + platform).
    int const base_solids = check_mesh("planet base structures", &m, true);
    CHECK(base_solids == 5 + 2 + 3 + 2 + 3 + 2 * BASE_FLARES, "planet base: expected %d solids",
          5 + 2 + 3 + 2 + 3 + 2 * BASE_FLARES);
    check_recorded_parts("planet base structures", &m, base_solids);
    mesh_free(&m);
    planet_base_build_apron(&m);
    check_mesh("planet base apron (open)", &m, false);
    {
        int down = 0;
        for (int i = 0; i < m.tn; i++) {
            vec3_t const a = m.v[m.t[i].a], b = m.v[m.t[i].b], c = m.v[m.t[i].c];
            down += v3_cross(v3_sub(b, a), v3_sub(c, a)).y <= 0.0f;
        }
        CHECK(down == 0, "apron: %d triangles not facing up", down);
    }
    mesh_free(&m);
    planet_base_build_ridge(&m);
    check_mesh("planet base ridge (open)", &m, false);
    {
        int away = 0;
        for (int i = 0; i < m.tn; i++) {
            vec3_t const a = m.v[m.t[i].a], b = m.v[m.t[i].b], c = m.v[m.t[i].c];
            vec3_t const n  = v3_cross(v3_sub(b, a), v3_sub(c, a));
            away           += v3_dot(n, v3(-a.x, 0.0f, -a.z)) <= 0.0f;  // towards the centre
        }
        CHECK(away == 0, "ridge: %d triangles not facing the base", away);
    }
    mesh_free(&m);

    // Asteroids: the four shapes the asset builds, and 50 more seeds at
    // the lumpiest setting, so no seed folds a face.
    {
        static unsigned const SEEDS[4] = {0xA57Eu, 0x51Du, 0x77Au, 0x3C1u};
        for (int i = 0; i < 4; i++) {
            asteroid_build_mesh(&m, SEEDS[i], 0.24f);
            char name[40];
            snprintf(name, sizeof(name), "asteroid %d", i);
            CHECK(check_mesh(name, &m, true) == 1, "%s: expected one solid", name);
            mesh_free(&m);
        }
        int const before_a = s_fail;
        for (unsigned seed = 1000; seed < 1050; seed++) {
            asteroid_build_mesh(&m, seed, 0.3f);
            if (check_mesh("asteroid (random seed)", &m, true) != 1) s_fail++;
            mesh_free(&m);
        }
        CHECK(s_fail == before_a, "asteroid: %d of 50 random seeds failed", s_fail - before_a);
    }

    // The title: every stroke of every glyph its own closed solid.
    static struct {
        char const* text;
        int         strokes;
    } const LINES[] = {{"Borderworlds:", 21}, {"Superior", 13}};
    for (int i = 0; i < 2; i++) {
        mesh_init(&m);
        float const w = title_text_build(&m, LINES[i].text, 7.0f, 1.5f);
        char        name[48];
        snprintf(name, sizeof(name), "title \"%s\" (width %.1f)", LINES[i].text, (double)w);
        int const solids = check_mesh(name, &m, true);
        CHECK(solids == LINES[i].strokes, "%s: expected %d strokes", name, LINES[i].strokes);
        check_recorded_parts(name, &m, solids);
        CHECK(fabsf(w - title_text_width(LINES[i].text, 7.0f)) < 1e-4f, "%s: width and build disagree", name);
        mesh_free(&m);
    }
}
