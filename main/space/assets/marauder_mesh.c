// =====================================================================
//  Showreel asset  --  the marauder's geometry (see marauder_mesh.h)
// =====================================================================

#include "space/assets/marauder_mesh.h"

// Model units per repeat of the 64-texel paint texture.
#define PAINT_REPEAT 0.3f

// A flattened hexagonal cross-section: half-width w at height y0, roof
// ht above it, keel hb below. Same point order for every section, so the
// loft joins them without twisting.
static void hexsec(float out[MESH_LOFT_MAX_PTS][2], float w, float ht, float hb, float y0) {
    float const p[6][2] = {
        {w, y0}, {0.6f * w, y0 + ht}, {-0.6f * w, y0 + ht}, {-w, y0}, {-0.6f * w, y0 - hb}, {0.6f * w, y0 - hb},
    };
    for (int i = 0; i < 6; i++) {
        out[i][0] = p[i][0];
        out[i][1] = p[i][1];
    }
}

// A thin four-cornered section from (x0, y0) to (x1, y1), thickness 2t:
// a wing chord seen end-on, or (swapped axes) a fin. `s` mirrors x.
static void slab(float out[MESH_LOFT_MAX_PTS][2], float x0, float y0, float x1, float y1, float t, float s) {
    float const p[4][2] = {{x0, y0 - t}, {x1, y1 - t}, {x1, y1 + t}, {x0, y0 + t}};
    for (int i = 0; i < 4; i++) {
        out[i][0] = s * p[i][0];
        out[i][1] = p[i][1];
    }
}

// A fin: a thin wall standing on (x, y0), leaning `lean` outward over its
// height h, thickness 2t. `s` mirrors x.
static void finsec(float out[MESH_LOFT_MAX_PTS][2], float x, float y0, float h, float lean, float t, float s) {
    float const p[4][2] = {{x - t, y0}, {x + t, y0}, {x + t + lean, y0 + h}, {x - t + lean, y0 + h}};
    for (int i = 0; i < 4; i++) {
        out[i][0] = s * p[i][0];
        out[i][1] = p[i][1];
    }
}

void marauder_build_mesh(mesh_t* m) {
    mesh_init(m);
    m->name = "marauder";
    float sec[5][MESH_LOFT_MAX_PTS][2];

    // Fuselage: from a near-point nose to a blunt tail, bulging at the
    // cockpit.
    {
        float const z[5] = {-0.50f, -0.25f, 0.05f, 0.35f, 0.62f};
        hexsec(sec[0], 0.12f, 0.05f, 0.06f, 0.0f);
        hexsec(sec[1], 0.15f, 0.07f, 0.07f, 0.0f);
        hexsec(sec[2], 0.14f, 0.09f, 0.06f, 0.0f);
        hexsec(sec[3], 0.09f, 0.05f, 0.04f, 0.0f);
        hexsec(sec[4], 0.015f, 0.012f, 0.010f, -0.01f);
        mesh_loft(m, 5, 6, z, (float const(*)[MESH_LOFT_MAX_PTS][2])sec, MARAUDER_MAT_PAINT, MARAUDER_MAT_PAINT,
                  PAINT_REPEAT);
    }

    // Cockpit canopy, sunk into the hump.
    mesh_box(m, v3(-0.05f, 0.05f, -0.02f), v3(0.05f, 0.105f, 0.20f), MARAUDER_MAT_CANOPY, PAINT_REPEAT);

    for (int side = 0; side < 2; side++) {
        float const s = side ? 1.0f : -1.0f;

        // Swept delta wing, its tip drooping (anhedral).
        {
            float const z[3] = {-0.50f, -0.35f, 0.15f};
            slab(sec[0], 0.10f, -0.04f, 0.46f, -0.08f, 0.015f, s);
            slab(sec[1], 0.10f, -0.04f, 0.50f, -0.08f, 0.020f, s);
            slab(sec[2], 0.10f, -0.02f, 0.13f, -0.02f, 0.012f, s);
            mesh_loft(m, 3, 4, z, (float const(*)[MESH_LOFT_MAX_PTS][2])sec, MARAUDER_MAT_PAINT, MARAUDER_MAT_PAINT,
                      PAINT_REPEAT);
        }

        // Wingtip fin, leaning outward.
        {
            float const z[2] = {-0.48f, -0.20f};
            finsec(sec[0], 0.47f, -0.07f, 0.16f, 0.03f, 0.01f, s);
            finsec(sec[1], 0.47f, -0.07f, 0.03f, 0.005f, 0.008f, s);
            mesh_loft(m, 2, 4, z, (float const(*)[MESH_LOFT_MAX_PTS][2])sec, MARAUDER_MAT_PAINT, MARAUDER_MAT_PAINT,
                      PAINT_REPEAT);
        }

        // Engine nacelle: a drum along z, its rear mouth dark.
        int first = m->vn;
        mesh_cylinder(m, 0.055f, MARAUDER_NOZZLE_Z, -0.15f, 8, true, true, MARAUDER_MAT_METAL, MARAUDER_MAT_NOZZLE,
                      PAINT_REPEAT);
        xform_t const at_nacelle = {mat3_rot_z(0.0f), v3(s * MARAUDER_NOZZLE_X, MARAUDER_NOZZLE_Y, 0.0f), 1.0f};
        mesh_transform_from(m, first, &at_nacelle);

        // Gun under the wing root, muzzle forward.
        mesh_box(m, v3(s * MARAUDER_GUN_X - 0.012f, MARAUDER_GUN_Y - 0.012f, 0.02f),
                 v3(s * MARAUDER_GUN_X + 0.012f, MARAUDER_GUN_Y + 0.012f, MARAUDER_GUN_Z), MARAUDER_MAT_METAL,
                 PAINT_REPEAT);
    }
}
