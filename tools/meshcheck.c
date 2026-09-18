// =====================================================================
//  Showreel  --  host-side check of the math and every mesh generator
// ---------------------------------------------------------------------
//  Built and run by `make meshcheck` with the host compiler (no badge
//  needed). Exit status 0 = all checks passed.
//
//  Mesh checks, per connected part of a mesh (triangles sharing
//  vertices):
//    closed      every edge is used by exactly two triangles
//    consistent  ...once in each direction, so neighbouring faces agree
//                on which side is outside
//    outward     the part's signed volume is positive
//    no slivers  no triangle with (near) zero area
//  Consistent winding plus positive volume means every face points out
//  of the solid -- including the inner wall of a ring, where a "normal
//  points away from the centre" test would be wrong. A face that points
//  inward is culled when it should be drawn, i.e. a hole in the model.
// =====================================================================

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mesh.h"
#include "xform.h"

static int s_fail = 0;

#define CHECK(cond, ...)                    \
    do {                                    \
        if (!(cond)) {                      \
            printf("  FAIL: " __VA_ARGS__); \
            printf("\n");                   \
            s_fail++;                       \
        }                                   \
    } while (0)

static float frand(unsigned* s) {
    *s = *s * 1664525u + 1013904223u;
    return (float)(*s >> 8) / 16777216.0f;
}

static bool near3(vec3_t a, vec3_t b, float eps) {
    return fabsf(a.x - b.x) < eps && fabsf(a.y - b.y) < eps && fabsf(a.z - b.z) < eps;
}

// --- Math ---------------------------------------------------------------

static void check_math(void) {
    printf("math\n");
    unsigned seed = 1;
    for (int i = 0; i < 1000; i++) {
        float const  yaw   = (frand(&seed) * 2.0f - 1.0f) * 3.1f;
        float const  pitch = (frand(&seed) * 2.0f - 1.0f) * 1.5f;
        float const  roll  = (frand(&seed) * 2.0f - 1.0f) * 3.1f;
        mat3_t const full  = mat3_from_ypr(yaw, pitch, roll);
        mat3_t const flat  = mat3_from_ypr(yaw, pitch, 0.0f);

        // Basis from forward + up + roll reproduces the engine's Ry*Rx*Rz.
        mat3_t const fu = mat3_from_fwd_up(flat.fwd, flat.up, roll);
        CHECK(near3(fu.right, full.right, 1e-4f) && near3(fu.up, full.up, 1e-4f) && near3(fu.fwd, full.fwd, 1e-4f),
              "mat3_from_fwd_up != mat3_from_ypr at yaw %.3f pitch %.3f roll %.3f", yaw, pitch, roll);

        // Orthonormal, right-handed in the engine's sense: right = up x fwd.
        CHECK(fabsf(v3_len(full.right) - 1) < 1e-4f && fabsf(v3_dot(full.right, full.up)) < 1e-4f,
              "basis not orthonormal");
        CHECK(near3(v3_cross(full.up, full.fwd), full.right, 1e-4f), "right != up x fwd");

        // look_at_angles inverts the forward vector.
        float y2, p2;
        look_at_angles(v3(1, 2, 3), v3_add(v3(1, 2, 3), v3_scale(full.fwd, 5.0f)), &y2, &p2);
        mat3_t const back = mat3_from_ypr(y2, p2, 0.0f);
        CHECK(near3(back.fwd, full.fwd, 1e-4f), "look_at_angles does not reproduce forward");

        // Composition.
        mat3_t const ry = mat3_rot_y(yaw), rx = mat3_rot_x(pitch), rz = mat3_rot_z(roll);
        mat3_t const a = mat3_mul(&ry, &rx);
        mat3_t const m = mat3_mul(&a, &rz);
        CHECK(near3(m.right, full.right, 1e-4f) && near3(m.fwd, full.fwd, 1e-4f), "Ry*Rx*Rz != mat3_from_ypr");
    }

    // Paths pass through their points, and continue straight past the ends.
    vec3_t const pts[] = {{0, 0, 0}, {1, 2, 0}, {3, 2, 1}, {4, 0, 5}};
    path_t const p     = {pts, 4, 2.0f, 0.5f};
    for (int i = 0; i < 4; i++) {
        CHECK(near3(path_pos(&p, 2.0f + 0.5f * (float)i), pts[i], 1e-5f), "path misses point %d", i);
    }
    vec3_t const v_end = path_vel(&p, 3.5f);
    CHECK(near3(path_pos(&p, 4.5f), v3_add(pts[3], v3_scale(v_end, 1.0f)), 1e-4f), "path extrapolation");
    // Velocity matches a finite difference.
    vec3_t const fd = v3_scale(v3_sub(path_pos(&p, 2.8f + 1e-3f), path_pos(&p, 2.8f - 1e-3f)), 1.0f / 2e-3f);
    CHECK(near3(fd, path_vel(&p, 2.8f), 2e-2f), "path_vel != finite difference");

    // Noise is deterministic and in range.
    for (int i = 0; i < 1000; i++) {
        float const h = hash01(i, 7);
        CHECK(h >= 0.0f && h < 1.0f && h == hash01(i, 7), "hash01 range/determinism");
    }
}

// --- Mesh topology ------------------------------------------------------

static int* s_parent;
static int  find(int x) {
    while (s_parent[x] != x) x = s_parent[x] = s_parent[s_parent[x]];
    return x;
}

// A directed edge `from` -> the other end, keyed by its undirected
// endpoints (lo, hi).
typedef struct {
    int lo, hi, from;
} dedge_t;

static int cmp_edge(void const* pa, void const* pb) {
    dedge_t const* a = pa;
    dedge_t const* b = pb;
    if (a->lo != b->lo) return a->lo - b->lo;
    return a->hi - b->hi;
}

// Returns the number of parts; prints and counts failures.
static int check_mesh(char const* name, mesh_t const* m, bool expect_closed) {
    printf("%s: %d verts, %d tris\n", name, m->vn, m->tn);
    CHECK(!m->failed, "%s: builder ran out of memory", name);
    if (m->tn == 0) {
        CHECK(false, "%s: empty", name);
        return 0;
    }

    // Parts: union vertices of each triangle.
    s_parent = malloc(sizeof(int) * (size_t)m->vn);
    for (int i = 0; i < m->vn; i++) s_parent[i] = i;
    for (int i = 0; i < m->tn; i++) {
        int const a = find(m->t[i].a), b = find(m->t[i].b), c = find(m->t[i].c);
        s_parent[b]       = a;
        s_parent[find(c)] = a;
    }

    // Slivers.
    int slivers = 0;
    for (int i = 0; i < m->tn; i++) {
        vec3_t const a = m->v[m->t[i].a], b = m->v[m->t[i].b], c = m->v[m->t[i].c];
        float const  area = 0.5f * v3_len(v3_cross(v3_sub(b, a), v3_sub(c, a)));
        if (area < 1e-7f) slivers++;
    }
    CHECK(slivers == 0, "%s: %d degenerate triangles", name, slivers);

    // Edges: sort directed edges by their undirected key.
    int const n = m->tn * 3;
    dedge_t*  e = malloc(sizeof(dedge_t) * (size_t)n);
    for (int i = 0; i < m->tn; i++) {
        int const v[3] = {m->t[i].a, m->t[i].b, m->t[i].c};
        for (int k = 0; k < 3; k++) {
            int const f = v[k], t = v[(k + 1) % 3];
            e[i * 3 + k] = (dedge_t){f < t ? f : t, f < t ? t : f, f};
        }
    }
    qsort(e, (size_t)n, sizeof(dedge_t), cmp_edge);
    int open = 0, nonmanifold = 0, flipped = 0;
    for (int i = 0; i < n;) {
        int j = i;
        while (j < n && cmp_edge(&e[i], &e[j]) == 0) j++;
        int const cnt = j - i;
        if (cnt == 1) {
            open++;
        } else if (cnt > 2) {
            nonmanifold++;
        } else if (e[i].from == e[i + 1].from) {
            flipped++;  // both uses run the same way: neighbours disagree
        }
        i = j;
    }
    free(e);
    if (expect_closed) CHECK(open == 0, "%s: %d open edges", name, open);
    CHECK(nonmanifold == 0, "%s: %d edges shared by more than two triangles", name, nonmanifold);
    CHECK(flipped == 0, "%s: %d edges with inconsistent winding", name, flipped);

    // Per-part signed volume.
    int     parts = 0;
    double* vol   = calloc((size_t)m->vn, sizeof(double));
    for (int i = 0; i < m->tn; i++) {
        vec3_t const a = m->v[m->t[i].a], b = m->v[m->t[i].b], c = m->v[m->t[i].c];
        vol[find(m->t[i].a)] += (double)v3_dot(a, v3_cross(b, c)) / 6.0;
    }
    for (int i = 0; i < m->vn; i++) {
        if (find(i) != i) continue;
        bool used = false;
        for (int k = 0; k < m->tn && !used; k++) used = find(m->t[k].a) == i;
        if (!used) continue;
        parts++;
        CHECK(vol[i] > 0.0, "%s: part at vertex %d has signed volume %g (faces point inward)", name, i, vol[i]);
    }
    free(vol);
    free(s_parent);
    printf("  %d part(s), %d open edges\n", parts, open);
    return parts;
}

// --- Primitives -----------------------------------------------------------

static void check_primitives(void) {
    mesh_t m;

    mesh_init(&m);
    mesh_box(&m, v3(-1, -2, -3), v3(2, 1, 0.5f), 0, 1.0f);
    check_mesh("box", &m, true);
    CHECK(fabsf(mesh_signed_volume(&m, 0) - 3.0f * 3.0f * 3.5f) < 1e-3f, "box volume %g", mesh_signed_volume(&m, 0));
    mesh_free(&m);

    mesh_init(&m);
    mesh_cylinder(&m, 2.0f, -1.0f, 3.0f, 16, true, true, 0, 1, 1.0f);
    check_mesh("cylinder", &m, true);
    float const cyl = 0.5f * 16 * 4.0f * sinf(2.0f * 3.14159265f / 16) * 4.0f;  // 16-gon area * height
    CHECK(fabsf(mesh_signed_volume(&m, 0) - cyl) < 1e-2f, "cylinder volume %g vs %g", mesh_signed_volume(&m, 0), cyl);
    mesh_free(&m);

    mesh_init(&m);
    mesh_ring(&m, 20.0f, 23.0f, -1.25f, 1.25f, 48, 0, 1, 2, 2.0f);
    check_mesh("ring", &m, true);
    mesh_free(&m);

    mesh_init(&m);
    mesh_cone(&m, 0.6f, 0.0f, -3.0f, 6, 0, 1, 1.0f);
    check_mesh("cone (apex -z)", &m, true);
    mesh_cone(&m, 0.6f, 0.0f, 3.0f, 6, 0, 1, 1.0f);
    check_mesh("cone (+ apex +z, 2 parts)", &m, true);
    mesh_free(&m);

    // The checker itself: one flipped face, and a whole box turned inside
    // out, must both be caught.
    int const before = s_fail;
    mesh_init(&m);
    mesh_box(&m, v3(0, 0, 0), v3(1, 1, 1), 0, 1.0f);
    uint16_t const tmp = m.t[3].b;
    m.t[3].b           = m.t[3].c;
    m.t[3].c           = tmp;
    printf("(expected to fail:) ");
    check_mesh("box with one flipped face", &m, true);
    int const caught_flip = s_fail - before;
    mesh_free(&m);
    mesh_box(&m, v3(0, 0, 0), v3(1, 1, 1), 0, 1.0f);
    for (int i = 0; i < m.tn; i++) {
        uint16_t const t = m.t[i].b;
        m.t[i].b         = m.t[i].c;
        m.t[i].c         = t;
    }
    printf("(expected to fail:) ");
    int const before_inv = s_fail;
    check_mesh("box inside out", &m, true);
    int const caught_inv = s_fail - before_inv;
    mesh_free(&m);
    s_fail = before;
    CHECK(caught_flip > 0, "checker missed a flipped face");
    CHECK(caught_inv > 0, "checker missed an inside-out solid");

    // A rotated, translated, scaled part keeps its winding.
    mesh_init(&m);
    mesh_box(&m, v3(0, -0.5f, -0.5f), v3(10, 0.5f, 0.5f), 0, 1.0f);
    xform_t const x = {mat3_from_ypr(0.7f, -0.3f, 1.1f), v3(5, 6, 7), 1.5f};
    mesh_transform_from(&m, 0, &x);
    check_mesh("box, transformed", &m, true);
    mesh_free(&m);
}

// Generators register here as they are written (see the asset headers).
#include "meshcheck_assets.h"

int main(void) {
    check_math();
    check_primitives();
    check_assets();
    if (s_fail) {
        printf("\nmeshcheck: %d FAILURE(S)\n", s_fail);
        return 1;
    }
    printf("\nmeshcheck: all checks passed\n");
    return 0;
}
