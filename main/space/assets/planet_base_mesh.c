// =====================================================================
//  Showreel asset  --  the planet base's geometry (see planet_base_mesh.h)
// =====================================================================

#include "space/assets/planet_base_mesh.h"
#include <math.h>

#define WALL_REPEAT  4.0f  // one texture repeat per 4 units of wall
#define METAL_REPEAT 2.0f
#define APRON_REPEAT 3.0f

float const BASE_FLARE_TOP[BASE_FLARES][3] = {
    {30.0f, 22.0f, 34.0f},
    {-31.0f, 17.0f, 30.0f},
};

// A vertical cylinder standing on the ground at (x, z): mesh_cylinder
// builds along +z, so turn it up.
static void tower(mesh_t* m, float x, float z, float r, float h, int sides, uint8_t mat_side, uint8_t mat_cap,
                  float rep) {
    int const first = m->vn;
    mesh_cylinder(m, r, 0.0f, h, sides, true, true, mat_side, mat_cap, rep);
    // Model +z (the axis) to world +y: a pitch of -90 degrees (xform.h).
    xform_t const up = {mat3_rot_x(-1.5707963f), v3(x, 0.0f, z), 1.0f};
    mesh_transform_from(m, first, &up);
}

void planet_base_build_structures(mesh_t* m) {
    mesh_init(m);
    m->name = "base";

    // The pad, and a yellow frame painted just above its surface.
    float const h = BASE_PAD_HALF;
    mesh_box(m, v3(-h, 0.0f, -h), v3(h, BASE_PAD_TOP, h), BASE_MAT_PAD, 3.0f);
    float const in = h - 0.5f, wd = 0.2f, y0 = BASE_PAD_TOP, y1 = BASE_PAD_TOP + 0.02f;
    mesh_box(m, v3(-in, y0, -in), v3(in, y1, -in + wd), BASE_MAT_MARK, 1.0f);
    mesh_box(m, v3(-in, y0, in - wd), v3(in, y1, in), BASE_MAT_MARK, 1.0f);
    mesh_box(m, v3(-in, y0, -in + wd), v3(-in + wd, y1, in - wd), BASE_MAT_MARK, 1.0f);
    mesh_box(m, v3(in - wd, y0, -in + wd), v3(in, y1, in - wd), BASE_MAT_MARK, 1.0f);

    // Two halls behind the pad.
    mesh_box(m, v3(-18.0f, 0.0f, 16.0f), v3(-4.0f, 9.0f, 28.0f), BASE_MAT_WALL, WALL_REPEAT);
    mesh_box(m, v3(6.0f, 0.0f, 20.0f), v3(14.0f, 6.0f, 30.0f), BASE_MAT_WALL, WALL_REPEAT);

    // Storage tanks to the right, chimneys rising from the big hall.
    tower(m, 19.0f, 13.0f, 2.2f, 7.0f, 14, BASE_MAT_TANK, BASE_MAT_METAL, WALL_REPEAT);
    tower(m, 24.0f, 16.0f, 2.2f, 7.0f, 14, BASE_MAT_TANK, BASE_MAT_METAL, WALL_REPEAT);
    tower(m, 21.0f, 21.0f, 2.8f, 9.0f, 14, BASE_MAT_TANK, BASE_MAT_METAL, WALL_REPEAT);
    tower(m, -14.0f, 25.0f, 0.7f, 17.0f, 8, BASE_MAT_METAL, BASE_MAT_METAL, METAL_REPEAT);
    tower(m, -10.0f, 25.0f, 0.7f, 15.0f, 8, BASE_MAT_METAL, BASE_MAT_METAL, METAL_REPEAT);

    // A pipe rack from the big hall to the small one: two pipes on posts.
    mesh_box(m, v3(-4.0f, 3.6f, 18.4f), v3(6.0f, 3.9f, 18.7f), BASE_MAT_METAL, METAL_REPEAT);
    mesh_box(m, v3(-4.0f, 3.6f, 19.1f), v3(6.0f, 3.9f, 19.4f), BASE_MAT_METAL, METAL_REPEAT);
    mesh_box(m, v3(0.8f, 0.0f, 18.5f), v3(1.2f, 3.6f, 19.3f), BASE_MAT_METAL, METAL_REPEAT);

    // Flare stacks, well apart from the works, each with a small platform
    // near the top.
    for (int i = 0; i < BASE_FLARES; i++) {
        float const x = BASE_FLARE_TOP[i][0], top = BASE_FLARE_TOP[i][1], z = BASE_FLARE_TOP[i][2];
        tower(m, x, z, BASE_FLARE_R, top, 8, BASE_MAT_METAL, BASE_MAT_METAL, METAL_REPEAT);
        mesh_box(m, v3(x - 1.0f, top - 2.5f, z - 1.0f), v3(x + 1.0f, top - 2.2f, z + 1.0f), BASE_MAT_METAL,
                 METAL_REPEAT);
    }
}

// The apron: GRID x GRID quads of CELL units, facing up (+y), round the
// pad and under the works.
#define APRON_GRID 8
#define APRON_CELL 7.0f
#define APRON_X0   (-28.0f)
#define APRON_Z0   (-20.0f)

void planet_base_build_apron(mesh_t* m) {
    mesh_init(m);
    m->name        = "apron";
    int const base = m->vn;
    for (int j = 0; j <= APRON_GRID; j++) {
        for (int i = 0; i <= APRON_GRID; i++) {
            mesh_vert(m, v3(APRON_X0 + (float)i * APRON_CELL, 0.0f, APRON_Z0 + (float)j * APRON_CELL));
        }
    }
    int const row = APRON_GRID + 1;
    for (int j = 0; j < APRON_GRID; j++) {
        for (int i = 0; i < APRON_GRID; i++) {
            int const a = base + j * row + i, b = a + 1, c = a + row + 1, d = a + row;
            float     uv[4][2];
            int const q[4] = {a, b, c, d};
            for (int k = 0; k < 4; k++) {
                uv[k][0] = m->v[q[k]].x / APRON_REPEAT;
                uv[k][1] = m->v[q[k]].z / APRON_REPEAT;
            }
            // (a, d, c, b) winds counter-clockwise seen from above: up.
            float const r[4][2] = {
                {uv[0][0], uv[0][1]}, {uv[3][0], uv[3][1]}, {uv[2][0], uv[2][1]}, {uv[1][0], uv[1][1]}};
            mesh_quad(m, a, d, c, b, BASE_MAT_APRON, r);
        }
    }
}

// The ridge: RIDGE_SEGS panels on a circle of RIDGE_R, each from below
// the ground up to a seeded height, facing the centre.
#define RIDGE_SEGS 36
#define RIDGE_R    900.0f

void planet_base_build_ridge(mesh_t* m) {
    mesh_init(m);
    m->name               = "ridge";
    int const   base      = m->vn;
    float const uv0[4][2] = {{0}};
    for (int k = 0; k < RIDGE_SEGS; k++) {
        float const a   = 6.2831853f * (float)k / (float)RIDGE_SEGS;
        // Two octaves of a seeded wobble: 25..95 units high, i.e. about
        // 1.5 to 6 degrees above the horizon from 900 units.
        float const hgt = 60.0f + 25.0f * sinf(3.0f * a + 1.3f) + 10.0f * sinf(11.0f * a + 0.4f);
        mesh_vert(m, v3(RIDGE_R * sinf(a), -5.0f, RIDGE_R * cosf(a)));
        mesh_vert(m, v3(RIDGE_R * sinf(a), hgt, RIDGE_R * cosf(a)));
    }
    for (int k = 0; k < RIDGE_SEGS; k++) {
        int const a = base + 2 * k, b = base + 2 * ((k + 1) % RIDGE_SEGS);
        // Walking round with increasing angle (x = sin, z = cos), the
        // inward-facing winding is (a_low, a_high, b_high, b_low).
        mesh_quad(m, a, a + 1, b + 1, b, BASE_MAT_RIDGE, uv0);
    }
}
