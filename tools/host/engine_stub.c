// =====================================================================
//  Showreel  --  host stand-in for the engine (make scenecheck)
// ---------------------------------------------------------------------
//  Implements the scene, camera, light and texture calls the scene and
//  asset code makes, with the engine's own camera maths (M = Ry(yaw) *
//  Rx(pitch) * Rz(roll), the same expansion as camera_build_basis() and
//  mat3_from_ypr()) and projection (se_config.h). Nothing is drawn:
//  every primitive is handed to the checker (tools/scenecheck.c)
//  through the sc_* hooks in scenecheck.h.
// =====================================================================

#include <stdlib.h>
#include "scenecheck.h"
#include "synthengine3d.h"
#include "xform.h"

static render_camera_t s_cam;
static mat3_t          s_basis = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
static se_light_t      s_light;
static bool            s_light_on;

// --- Camera ---------------------------------------------------------------

void render_set_camera_6dof(float x, float y, float z, float yaw, float pitch, float roll) {
    s_cam   = (render_camera_t){x, y, z, yaw, pitch, roll};
    s_basis = mat3_from_ypr(yaw, pitch, roll);
}

void render_set_camera(float x, float y) {
    render_set_camera_6dof(x, y, 0.0f, 0.0f, 0.0f, 0.0f);
}

render_camera_t render_camera(void) {
    return s_cam;
}

vec3_t sc_to_camera(vec3_t p) {
    vec3_t const d = v3(p.x - s_cam.x, p.y - s_cam.y, p.z - s_cam.z);
    return v3(v3_dot(s_basis.right, d), v3_dot(s_basis.up, d), v3_dot(s_basis.fwd, d));
}

void sc_project(vec3_t c, float* sx, float* sy) {
    float const z = c.z < 0.01f ? 0.01f : c.z;
    *sx           = RENDER_HALF_W + RENDER_FOCAL_LEN * c.x / z;
    *sy           = RENDER_HORIZON_Y - RENDER_FOCAL_LEN * c.y / z;
}

void render_project(float x_w, float y_w, float z_w, float* out_sx, float* out_sy) {
    sc_project(sc_to_camera(v3(x_w, y_w, z_w)), out_sx, out_sy);
}

// --- Light ------------------------------------------------------------------

void se_light_set(se_light_t const* light) {
    s_light_on = light != NULL;
    if (light) s_light = *light;
}

bool se_light_get(se_light_t* out) {
    if (s_light_on && out) *out = s_light;
    return s_light_on;
}

// --- Textures -----------------------------------------------------------------
// Every load succeeds with a blank 64x64 texture, so the scene code takes
// its textured paths exactly as on the badge.

se_texture_t* se_texture_load(char const* path, uint32_t flags) {
    (void)path;
    (void)flags;
    se_texture_t* t = calloc(1, sizeof(*t));
    if (t == NULL) return NULL;
    t->texels = calloc(64 * 64, sizeof(uint16_t));
    t->w = t->h  = 64;
    t->w_log2    = 6;
    t->mean_argb = 0xFF808080u;
    return t;
}

void se_texture_unload(se_texture_t* tex) {
    if (tex == NULL) return;
    free(tex->texels);
    free(tex);
}

// --- Primitives -----------------------------------------------------------------

void scene_tri(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, uint32_t argb,
               uint32_t flags) {
    (void)argb;
    (void)flags;
    vec3_t const v[3] = {v3(x0, y0, z0), v3(x1, y1, z1), v3(x2, y2, z2)};
    sc_tri(v, false);
}

void scene_textured_tri(se_tex_vertex_t const v[3], se_texture_t const* tex, uint32_t flags) {
    (void)flags;
    if (v == NULL || tex == NULL || tex->texels == NULL) return;  // as the engine
    vec3_t const w[3] = {v3(v[0].x, v[0].y, v[0].z), v3(v[1].x, v[1].y, v[1].z), v3(v[2].x, v[2].y, v[2].z)};
    sc_tri(w, true);
}

void scene_line(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t argb) {
    sc_line(v3(x0, y0, z0), v3(x1, y1, z1), argb);
}

void scene_point(float x, float y, float z, uint32_t argb) {
    (void)argb;
    sc_point(v3(x, y, z));
}
