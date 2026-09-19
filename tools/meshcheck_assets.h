// =====================================================================
//  Showreel  --  asset generators under the mesh check
// ---------------------------------------------------------------------
//  Included by tools/meshcheck.c. Each asset that builds a mesh exposes
//  a pure *_build_mesh() (engine-free, in assets/<name>_mesh.c); add it
//  here, and its source file to MESHCHECK_SRCS in the Makefile.
// =====================================================================

#include "craftminer/assets/miner_mesh.h"
#include "craftminer/voxel/voxel_mesh.h"
#include "craftminer/voxel/voxel_world.h"
#include "space/assets/asteroid_mesh.h"
#include "space/assets/marauder_mesh.h"
#include "space/assets/planet_base_mesh.h"
#include "space/assets/station_mesh.h"
#include "space/assets/title_text_mesh.h"
#include "space/objects/ship_model.h"

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

// --- The block mesher (CraftMiner) -----------------------------------------------------
//
// Its quads do not share vertices, and greedy rectangles meet in
// T-junctions, so the shared-edge test above does not apply. What must
// hold instead: the surface encloses exactly the solid cells (signed
// volume = cell count x step^3), no face is hidden (total area = the
// exposed unit faces), nothing is degenerate, and the counts per case.

#define VG 6  // test grids: 6 x 6 x 6 cells inside a border of air
static uint8_t s_vg[(VG + 2) * (VG + 2) * (VG + 2)];

static uint8_t* vg_cell(int x, int y, int z) {
    return &s_vg[((z + 1) * (VG + 2) + (x + 1)) * (VG + 2) + (y + 1)];
}

static float vg_volume(mesh_t const* m) {
    double v = 0.0;
    for (int i = 0; i < m->tn; i++) {
        vec3_t const a = m->v[m->t[i].a], b = m->v[m->t[i].b], c = m->v[m->t[i].c];
        v += (double)v3_dot(a, v3_cross(b, c)) / 6.0;
    }
    return (float)v;
}

static float vg_area(mesh_t const* m) {
    double s = 0.0;
    for (int i = 0; i < m->tn; i++) {
        vec3_t const a = m->v[m->t[i].a], b = m->v[m->t[i].b], c = m->v[m->t[i].c];
        s += 0.5 * (double)v3_len(v3_cross(v3_sub(b, a), v3_sub(c, a)));
    }
    return (float)s;
}

// Mesh the test grid; check volume and area (only for a lump in open
// air: `closed`) and the triangle count (-1: any).
static void vg_case(char const* name, vox_mesh_mode_t mode, int step, bool skirt, bool closed, int tris) {
    int solid = 0, exposed = 0;
    for (int z = -1; z <= VG; z++) {
        for (int x = -1; x <= VG; x++) {
            for (int y = -1; y <= VG; y++) {
                bool const in = x >= 0 && x < VG && y >= 0 && y < VG && z >= 0 && z < VG;
                uint8_t    b  = *vg_cell(x, y, z);
                if (!in || (b != VB_STONE && b != VB_GRASS && b != VB_LEAVES)) continue;
                solid++;
                int const nb[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                for (int k = 0; k < 6; k++) exposed += *vg_cell(x + nb[k][0], y + nb[k][1], z + nb[k][2]) == VB_AIR;
            }
        }
    }
    mesh_t m;
    mesh_init(&m);
    vox_grid_t const g = {s_vg, VG, VG, VG, 0, 0, step, skirt};
    voxel_mesh_build(&m, &g, mode);
    check_mesh(name, &m, false);
    float const st = (float)step, vol = vg_volume(&m), area = vg_area(&m);
    printf("  volume %.3f (cells %d), area %.3f (exposed faces %d)\n", vol, solid, area, exposed);
    if (closed) {
        CHECK(fabsf(vol - (float)solid * st * st * st) < 1e-3f, "%s: volume %g, expected %d cells", name, vol, solid);
        CHECK(fabsf(area - (float)exposed * st * st) < 1e-3f, "%s: area %g, expected %d faces", name, area, exposed);
    }
    if (tris >= 0) CHECK(m.tn == tris, "%s: %d triangles, expected %d", name, m.tn, tris);
    mesh_free(&m);
}

static void vg_clear(void) {
    memset(s_vg, VB_AIR, sizeof(s_vg));
}

static void vg_fill(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t b) {
    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) *vg_cell(x, y, z) = b;
        }
    }
}

static void check_voxel_mesher(void) {
    vg_clear();
    vg_fill(2, 2, 2, 2, 2, 2, VB_STONE);
    vg_case("voxel: one block", VOX_MESH_FAST, 1, false, true, 12);
    vg_fill(3, 2, 2, 3, 2, 2, VB_STONE);
    vg_case("voxel: two blocks (no inner face, merged)", VOX_MESH_FAST, 1, false, true, 12);
    vg_clear();
    vg_fill(1, 1, 1, 3, 3, 3, VB_STONE);
    vg_case("voxel: 3x3x3 (one quad a side)", VOX_MESH_FAST, 1, false, true, 12);
    vg_case("voxel: 3x3x3 in half-resolution cells", VOX_MESH_FAST, 2, false, true, 12);
    // A random lump: the counts vary, volume and area must not.
    vg_clear();
    unsigned seed = 7;
    for (int i = 0; i < 90; i++) {
        seed = seed * 1103515245u + 12345u;
        vg_fill((int)(seed >> 8) % VG, (int)(seed >> 12) % VG, (int)(seed >> 16) % VG, (int)(seed >> 8) % VG,
                (int)(seed >> 12) % VG, (int)(seed >> 16) % VG, VB_STONE);
    }
    vg_case("voxel: random lump", VOX_MESH_FAST, 1, false, true, -1);
    // Grass sides never stack: a 3-high column has 3 x 4 side quads.
    vg_clear();
    vg_fill(2, 1, 2, 2, 3, 2, VB_GRASS);
    vg_case("voxel: grass column (sides one block tall)", VOX_MESH_FAST, 1, false, true, 2 * (2 + 12));
    // Leaves: fancy shows the faces between two leaf blocks, fast does not.
    vg_clear();
    vg_fill(2, 2, 2, 3, 2, 2, VB_LEAVES);
    {
        mesh_t m;
        mesh_init(&m);
        vox_grid_t const g = {s_vg, VG, VG, VG, 0, 0, 1, false};
        voxel_mesh_build(&m, &g, VOX_MESH_FANCY);
        printf("voxel: two leaf blocks, fancy: %d tris\n", m.tn);
        // 6 outer quads (merged across both) + the 2 inner faces.
        CHECK(m.tn == 2 * (6 + 2), "voxel: fancy leaves: %d triangles, expected 16 (with the 2 inner faces)", m.tn);
        mesh_free(&m);
    }
    vg_case("voxel: two leaf blocks, fast", VOX_MESH_FAST, 1, false, true, 12);
    // A plant: two crossed quads, each from both sides; none when fast.
    vg_clear();
    *vg_cell(2, 0, 2) = VB_FLOWER_RED;
    {
        mesh_t m;
        mesh_init(&m);
        vox_grid_t const g = {s_vg, VG, VG, VG, 0, 0, 1, false};
        voxel_mesh_build(&m, &g, VOX_MESH_FANCY);
        check_mesh("voxel: a flower (fancy)", &m, false);
        CHECK(m.tn == 8, "voxel: flower: %d triangles, expected 8", m.tn);
        mesh_free(&m);
        mesh_init(&m);
        voxel_mesh_build(&m, &g, VOX_MESH_FAST);
        CHECK(m.tn == 0, "voxel: flower in a fast mesh: %d triangles, expected none", m.tn);
        mesh_free(&m);
    }
    // A skirt: a box filled wall to wall with a solid border all round
    // shows its top only -- with a skirt, its four sides as well.
    memset(s_vg, VB_STONE, sizeof(s_vg));
    for (int z = -1; z <= VG; z++) {
        for (int x = -1; x <= VG; x++) {
            for (int y = 3; y <= VG; y++) *vg_cell(x, y, z) = VB_AIR;
        }
    }
    vg_case("voxel: no skirt (top only)", VOX_MESH_FAST, 1, false, false, 2);
    vg_case("voxel: skirt (top and four sides)", VOX_MESH_FAST, 1, true, false, 10);
}

// The miner: every piece a set of closed, outward solids (the head's box
// is built face by face, the rest by mesh_box).
static void check_miner(void) {
    struct {
        char const* name;
        void (*build)(mesh_t*);
        int solids;
    } const PIECES[] = {
        {"miner head (head, hat, brim, lamp)", miner_build_head, 4},
        {"miner body (overalls, shirt, bib)", miner_build_body, 3},
        {"miner arm (sleeve, hand)", miner_build_arm, 2},
        {"miner leg (leg, boot)", miner_build_leg, 2},
        {"miner pickaxe (handle, head, two tips)", miner_build_pick, 4},
    };
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        mesh_t m;
        PIECES[i].build(&m);
        int const solids = check_mesh(PIECES[i].name, &m, true);
        CHECK(solids == PIECES[i].solids, "%s: %d solids, expected %d", PIECES[i].name, solids, PIECES[i].solids);
        mesh_free(&m);
    }
    mesh_t m;
    mesh_init(&m);
    voxel_build_cube(&m, 0.5f);
    check_mesh("voxel cube (items, pops)", &m, true);
    CHECK(fabsf(mesh_signed_volume(&m, 0) - 1.0f) < 1e-4f, "voxel cube: volume %g, expected 1", mesh_signed_volume(&m, 0));
    mesh_free(&m);
}

static void check_assets(void) {
    check_voxel_mesher();
    check_miner();
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
