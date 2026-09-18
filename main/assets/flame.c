// =====================================================================
//  Showreel asset  --  Frontier-style thruster flame (see flame.h)
// =====================================================================

#include "assets/flame.h"
#include <stdbool.h>
#include <stddef.h>
#include "assets/texcache.h"
#include "camera.h"
#include "synthengine3d.h"

// Texture u at the tip. Short of 1.0 so no pixel near the tip rounds up
// into the next repeat and wraps back to the white-hot nozzle colour.
// v stops short of 0 and 1 across a side for the same reason.
#define FLAME_TIP_U 0.97f

// Flicker: the length wanders within [MIN, MAX] x nominal, taking a new
// random value FLICKER_RATE times a second and moving linearly between
// them, so it shimmers rather than strobing.
#define FLICKER_MIN  0.88f
#define FLICKER_MAX  1.10f
#define FLICKER_RATE 25.0f

typedef struct {
    char const* file;
    uint32_t    fallback_argb;  // mid-flame colour, if the texture is missing
} flame_style_def_t;

static flame_style_def_t const STYLES[FLAME_STYLE_COUNT] = {
    [FLAME_BLUE] = {"flame.png", 0xFF3C84FFu},
    [FLAME_RED]  = {"flame_red.png", 0xFFFF4020u},
};

static se_texture_t const* s_tex[FLAME_STYLE_COUNT];
static bool                s_ready;

void flame_init(void) {
    if (s_ready) return;
    for (int i = 0; i < FLAME_STYLE_COUNT; i++) s_tex[i] = texcache_get(STYLES[i].file);
    s_ready = true;
}

void flame_shutdown(void) {
    // The textures belong to the cache (texcache_shutdown).
    for (int i = 0; i < FLAME_STYLE_COUNT; i++) s_tex[i] = NULL;
    s_ready = false;
}

float flame_flicker(double t, unsigned seed) {
    // Wrap the time so the float noise argument keeps its precision on a
    // long run; 1000 s of flicker never visibly repeats.
    float const tw = (float)fmod(t, 1000.0);
    return FLICKER_MIN + (FLICKER_MAX - FLICKER_MIN) * value_noise(tw, FLICKER_RATE, seed);
}

void flame_submit(xform_t const* x, vec3_t nozzle, float r, float len, flame_style_t style) {
    vec3_t rim[FLAME_SIDES];
    for (int i = 0; i < FLAME_SIDES; i++) {
        float const ang = (float)i * (2.0f * (float)M_PI / FLAME_SIDES);
        rim[i]          = xform_apply(x, v3(nozzle.x + r * cosf(ang), nozzle.y + r * sinf(ang), nozzle.z));
    }
    vec3_t const tip = xform_apply(x, v3(nozzle.x, nozzle.y, nozzle.z - len));
    vec3_t const eye = camera_eye();

    se_texture_t const* tex = s_tex[style];
    for (int i = 0; i < FLAME_SIDES; i++) {
        // (rim[i+1], rim[i], tip) is outward for a flame trailing along
        // -z. No base cap: it would sit flush against the nozzle.
        vec3_t const a = rim[(i + 1) % FLAME_SIDES];
        vec3_t const b = rim[i];
        if (!tri_faces_point(a, b, tip, eye)) continue;
        if (tex != NULL) {
            se_tex_vertex_t const tv[3] = {
                {a.x, a.y, a.z, 0.0f, 0.98f},
                {b.x, b.y, b.z, 0.0f, 0.02f},
                {tip.x, tip.y, tip.z, FLAME_TIP_U, 0.5f},
            };
            scene_textured_tri(tv, tex, SE_TRI_EMISSIVE);
        } else {
            scene_tri(a.x, a.y, a.z, b.x, b.y, b.z, tip.x, tip.y, tip.z, STYLES[style].fallback_argb, SE_TRI_EMISSIVE);
        }
    }
}
