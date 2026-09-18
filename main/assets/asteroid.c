// =====================================================================
//  Showreel asset  --  asteroids (see asteroid.h)
// =====================================================================

#include "assets/asteroid.h"
#include "assets/asteroid_mesh.h"
#include "assets/texcache.h"
#include "mesh_render.h"

// Shape 0 is the big hiding rock: the most battered. The rest are the
// small ones round it.
static struct {
    unsigned seed;
    float    lumpiness;
} const SHAPES[ASTEROID_SHAPES] = {{0xA57Eu, 0.24f}, {0x51Du, 0.18f}, {0x77Au, 0.2f}, {0x3C1u, 0.15f}};

static mesh_t     s_mesh[ASTEROID_SHAPES];
static mesh_mat_t s_mat;
static bool       s_ready;

void asteroid_init(void) {
    if (s_ready) return;
    for (int i = 0; i < ASTEROID_SHAPES; i++) asteroid_build_mesh(&s_mesh[i], SHAPES[i].seed, SHAPES[i].lumpiness);
    s_mat   = (mesh_mat_t){texcache_get("rock.png"), 0xFF706860u, 0};
    s_ready = true;
}

void asteroid_shutdown(void) {
    if (!s_ready) return;
    for (int i = 0; i < ASTEROID_SHAPES; i++) mesh_free(&s_mesh[i]);
    s_ready = false;
}

void asteroid_submit(int shape, xform_t const* x) {
    if (!s_ready || shape < 0 || shape >= ASTEROID_SHAPES) return;
    mesh_submit(&s_mesh[shape], x, &s_mat, 1);
}
