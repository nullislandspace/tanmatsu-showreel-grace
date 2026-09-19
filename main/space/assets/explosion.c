// =====================================================================
//  Showreel asset  --  explosions and laser impacts (see explosion.h)
// =====================================================================

#include "space/assets/explosion.h"
#include <math.h>
#include <stddef.h>
#include "mesh_render.h"
#include "space/assets/asteroid_mesh.h"
#include "space/assets/flame.h"

#define SPARKS        28
#define SPARK_SECS    1.4f
#define IMPACT_RAYS   8
#define FIREBALL_SECS 1.1f

// The fireball's two shells: lumpy blobs (the asteroid's builder), in
// flat emissive colours set per frame.
static mesh_t s_core, s_shell;
static bool   s_ready;

// Each blob's faces get one of FIRE_SHADES materials at random, drawn in
// neighbouring heats: mottled fire rather than one flat colour.
#define FIRE_SHADES 3

static void mottle(mesh_t* m, unsigned seed) {
    for (int i = 0; i < m->tn; i++) m->t[i].mat = (uint8_t)(hash01(i, seed) * FIRE_SHADES * 0.9999f);
}

void explosion_init(void) {
    if (s_ready) return;
    asteroid_build_mesh(&s_core, 0xF1AEu, 0.28f);
    asteroid_build_mesh(&s_shell, 0xB0B0u, 0.3f);
    mottle(&s_core, 0xC0DEu);
    mottle(&s_shell, 0x5E11u);
    s_core.name  = "fireball";
    s_shell.name = "fireball";
    s_ready      = true;
}

// The three shades round heat h: a little cooler and a little hotter.
static void shades(mesh_mat_t m[FIRE_SHADES], float h);

void explosion_shutdown(void) {
    if (!s_ready) return;
    mesh_free(&s_core);
    mesh_free(&s_shell);
    s_ready = false;
}

static uint32_t rgb(float r, float g, float b) {
    r = clampf(r, 0.0f, 255.0f), g = clampf(g, 0.0f, 255.0f), b = clampf(b, 0.0f, 255.0f);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// Heat 1 (white-hot) -> 0 (cold, black): white, yellow, orange, red, dark.
static uint32_t heat(float h) {
    h = clampf(h, 0.0f, 1.0f);
    return rgb(255.0f * fminf(1.0f, h * 2.2f), 255.0f * clampf(h * 1.6f - 0.35f, 0.0f, 1.0f),
               255.0f * clampf(h * 3.0f - 2.0f, 0.0f, 1.0f));
}

static void shades(mesh_mat_t m[FIRE_SHADES], float h) {
    static float const D[FIRE_SHADES] = {-0.12f, 0.0f, 0.1f};
    for (int k = 0; k < FIRE_SHADES; k++) m[k] = (mesh_mat_t){NULL, heat(h + D[k]), SE_TRI_EMISSIVE};
}

// A seeded unit direction.
static vec3_t dir(int i, unsigned seed) {
    float const z  = 2.0f * hash01(i, seed) - 1.0f;
    float const ph = 6.2831853f * hash01(i, seed + 1u);
    float const s  = sqrtf(1.0f - z * z);
    return v3(s * cosf(ph), s * sinf(ph), z);
}

void explosion_submit(vec3_t centre, vec3_t drift, float size, float t, float t_explode, unsigned seed) {
    float const u = t - t_explode;
    if (!s_ready || u < 0.0f || u >= EXPLOSION_SECS) return;
    vec3_t const c = v3_add(centre, v3_scale(drift, u));

    // Fireball: swells fast, then burns down. The shell outlives the core.
    if (u < FIREBALL_SECS) {
        float const  f     = u / FIREBALL_SECS;
        float const  grow  = smoothstep(0.0f, 0.18f, f);
        float const  fl    = flame_flicker(t, seed);
        float const  shell = size * (0.3f + 0.9f * grow) * (1.0f - 0.35f * f * f) * fl;
        // Both blobs are lumpy (radius 0.7..1.3 of their scale), so a core
        // nearly the shell's size shows through the shell's dents as
        // white-hot patches -- no transparency needed -- and sinks as it
        // cools.
        float const  core  = shell * (1.0f - 0.6f * f);
        mat3_t const spin  = mat3_axis_angle(dir(0, seed + 7u), 1.3f * u + 6.2831853f * hash01(1, seed));
        mesh_mat_t   m[FIRE_SHADES];
        shades(m, 0.85f - 0.8f * f);
        xform_t x = {spin, c, shell};
        mesh_submit(&s_shell, &x, m, FIRE_SHADES);
        if (core > 0.02f * size) {
            shades(m, 1.0f - 0.5f * f);
            x.r     = mat3_axis_angle(dir(1, seed + 7u), -1.7f * u);
            x.scale = core;
            mesh_submit(&s_core, &x, m, FIRE_SHADES);
        }
    }

    // Sparks: fly out at their own speeds, slowing a little, cooling.
    if (u < SPARK_SECS) {
        float const f = u / SPARK_SECS;
        for (int i = 0; i < SPARKS; i++) {
            vec3_t const   d     = dir(i, seed + 11u);
            float const    speed = size * (3.0f + 5.0f * hash01(i, seed + 13u));
            float const    run   = speed * (u - 0.25f * u * u);  // decelerating
            vec3_t const   head  = v3_add(c, v3_scale(d, run));
            vec3_t const   tail  = v3_add(c, v3_scale(d, run - 0.35f * size * (1.0f - f)));
            uint32_t const col   = heat(1.0f - f * (0.8f + 0.4f * hash01(i, seed + 17u)));
            scene_line(tail.x, tail.y, tail.z, head.x, head.y, head.z, col);
        }
    }
}

void impact_submit(vec3_t at, vec3_t normal, float size, float t, float t_hit, unsigned seed) {
    float const u = t - t_hit;
    if (u < 0.0f || u >= IMPACT_SECS) return;
    float const  f = u / IMPACT_SECS;
    vec3_t const n = v3_norm(normal);
    for (int i = 0; i < IMPACT_RAYS; i++) {
        // Into the hemisphere off the surface: flip directions that point in.
        vec3_t d = dir(i, seed);
        if (v3_dot(d, n) < 0.0f) d = v3_sub(d, v3_scale(n, 2.0f * v3_dot(d, n)));
        d                = v3_norm(v3_add(d, v3_scale(n, 0.6f)));  // mostly outward
        float const  len = size * (0.4f + 0.6f * hash01(i, seed + 3u));
        vec3_t const a   = v3_add(at, v3_scale(d, len * f));
        vec3_t const b   = v3_add(at, v3_scale(d, len * (0.3f + f)));
        scene_line(a.x, a.y, a.z, b.x, b.y, b.z, heat(1.0f - 0.8f * f));
    }
    scene_point(at.x, at.y, at.z, heat(1.0f - f));
}
