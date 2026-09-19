// =====================================================================
//  Showreel asset  --  asteroid geometry (see asteroid_mesh.h)
// =====================================================================

#include "space/assets/asteroid_mesh.h"
#include <math.h>

#define WAVES       9
#define SUBDIV      2
#define ROCK_REPEAT 0.7f  // one rock texture repeat per 0.7 radii

// The radius field: WAVES plane waves over the sphere, sin(f * d.w + p),
// with seeded unit directions w, frequencies f, phases p and weights
// falling with frequency (big lumps, smaller dents).
typedef struct {
    vec3_t w[WAVES];
    float  f[WAVES], p[WAVES], a[WAVES];
} field_t;

static float radius(vec3_t d, void* user) {
    field_t const* fl = user;
    float          r  = 1.0f;
    for (int i = 0; i < WAVES; i++) r += fl->a[i] * sinf(fl->f[i] * v3_dot(d, fl->w[i]) + fl->p[i]);
    return r;
}

void asteroid_build_mesh(mesh_t* m, unsigned seed, float lumpiness) {
    field_t fl;
    float   total = 0.0f;
    for (int i = 0; i < WAVES; i++) {
        // A direction uniform on the sphere from two hashes.
        float const z   = 2.0f * hash01(i, seed) - 1.0f;
        float const ph  = 6.2831853f * hash01(i, seed + 1u);
        float const s   = sqrtf(1.0f - z * z);
        fl.w[i]         = v3(s * cosf(ph), s * sinf(ph), z);
        fl.f[i]         = 1.5f + 3.5f * hash01(i, seed + 2u);
        fl.p[i]         = 6.2831853f * hash01(i, seed + 3u);
        fl.a[i]         = 1.0f / fl.f[i];
        total          += fl.a[i];
    }
    // Scale the weights so the waves together stray at most `lumpiness`.
    for (int i = 0; i < WAVES; i++) fl.a[i] *= lumpiness / total;

    mesh_init(m);
    m->name = "asteroid";
    mesh_blob(m, SUBDIV, radius, &fl, 0, ROCK_REPEAT);
}
