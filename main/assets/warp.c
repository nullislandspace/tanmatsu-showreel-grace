// =====================================================================
//  Showreel asset  --  the warp effect (see warp.h)
// =====================================================================

#include "assets/warp.h"
#include <math.h>
#include "synthengine3d.h"

#define STRETCH_LONG 6.0f   // length factor at the end of the stretch
#define STRETCH_THIN 0.3f   // width / height factor
#define RUN_SPANS    30.0f  // how far it races while stretching, in ship lengths
#define FLASH_RAYS   16

// How far into the stretch, 0..1, measured from the normal end: 0 is the
// ship as it is, 1 the moment it vanishes (or appears).
static float stretch_u(float t, float t_warp, warp_dir_t dir) {
    return dir == WARP_OUT ? (t - t_warp) / WARP_STRETCH_SECS : (t_warp - t) / WARP_STRETCH_SECS;
}

bool warp_pose(xform_t const* base, float t, float t_warp, warp_dir_t dir, xform_t* out) {
    float const u = stretch_u(t, t_warp, dir);
    if (u >= 1.0f) return false;
    if (u <= 0.0f) {
        *out = *base;
        return true;
    }
    float const  s    = smoothstep(0.0f, 1.0f, u);
    // Along its own forward axis: accelerating away (out) or still closing
    // in (in); u squared, so the speed ramps up from the ship's own.
    vec3_t const fwd  = v3_norm(base->r.fwd);
    float const  run  = RUN_SPANS * base->scale * u * u * (dir == WARP_OUT ? 1.0f : -1.0f);
    float const  thin = 1.0f + (STRETCH_THIN - 1.0f) * s;
    *out              = *base;
    out->r            = mat3_stretch(&base->r, v3(thin, thin, 1.0f + (STRETCH_LONG - 1.0f) * s));
    out->pos          = v3_add(base->pos, v3_scale(fwd, run));
    return true;
}

vec3_t warp_point(xform_t const* base_at_flash, warp_dir_t dir) {
    vec3_t const fwd = v3_norm(base_at_flash->r.fwd);
    float const  run = RUN_SPANS * base_at_flash->scale * (dir == WARP_OUT ? 1.0f : -1.0f);
    return v3_add(base_at_flash->pos, v3_scale(fwd, run));
}

void warp_flash_window(float t_warp, warp_dir_t dir, float* start, float* end) {
    if (dir == WARP_OUT) {
        *start = t_warp + WARP_STRETCH_SECS;
    } else {
        *start = t_warp - WARP_STRETCH_SECS - WARP_FLASH_SECS;
    }
    *end = *start + WARP_FLASH_SECS;
}

// White through pale blue to the dark: 0 at the flash's start, 1 at its end.
static uint32_t flash_colour(float f) {
    float const k = 1.0f - f;  // brightness
    float const r = 255.0f * k * k, g = 255.0f * k * (0.6f + 0.4f * k), b = 255.0f * k;
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void warp_submit_flash(vec3_t at, float size, float t, float t_warp, warp_dir_t dir, unsigned seed) {
    float start, end;
    warp_flash_window(t_warp, dir, &start, &end);
    if (t < start || t >= end) return;
    // f runs 0 -> 1 through the flash: expanding and fading. Warping in it
    // runs backwards, so the burst gathers, brightening, into the point
    // the ship streaks out of.
    float f = (t - start) / WARP_FLASH_SECS;
    if (dir == WARP_IN) f = 1.0f - f;
    uint32_t const col   = flash_colour(f);
    float const    outer = size * (0.5f + 3.0f * f);
    float const    inner = outer * (0.35f + 0.4f * f);
    for (int i = 0; i < FLASH_RAYS; i++) {
        // A direction spread over the sphere, seeded.
        float const  z  = 2.0f * hash01(i, seed) - 1.0f;
        float const  ph = 6.2831853f * hash01(i, seed + 1u);
        float const  s  = sqrtf(1.0f - z * z);
        vec3_t const d  = v3(s * cosf(ph), s * sinf(ph), z);
        float const  l  = 0.7f + 0.6f * hash01(i, seed + 2u);  // ray lengths vary
        vec3_t const a  = v3_add(at, v3_scale(d, inner * l));
        vec3_t const b  = v3_add(at, v3_scale(d, outer * l));
        scene_line(a.x, a.y, a.z, b.x, b.y, b.z, col);
    }
    scene_point(at.x, at.y, at.z, col);
}
