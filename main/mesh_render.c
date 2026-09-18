// =====================================================================
//  Showreel  --  submitting meshes to the scene (see mesh_render.h)
// =====================================================================

#include "mesh_render.h"
#include "camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static char const TAG[] = "mesh";

// World-space copies of the vertices, shared by every mesh_submit() and
// grown to the largest mesh seen.
static vec3_t* s_world;
static int     s_world_cap;

vec3_t const* mesh_last_world(void) {
    return s_world;
}

void mesh_submit(mesh_t const* m, xform_t const* x, mesh_mat_t const* mats, int mat_n) {
    if (m == NULL || m->vn == 0) return;
    if (m->vn > s_world_cap) {
        vec3_t* nw = heap_caps_realloc(s_world, (size_t)m->vn * sizeof(vec3_t), MALLOC_CAP_SPIRAM);
        if (nw == NULL) {
            ESP_LOGE(TAG, "no PSRAM for %d world vertices", m->vn);
            return;
        }
        s_world     = nw;
        s_world_cap = m->vn;
    }
    for (int i = 0; i < m->vn; i++) s_world[i] = xform_apply(x, m->v[i]);

    vec3_t const eye = camera_eye();
    for (int i = 0; i < m->tn; i++) {
        mesh_tri_t const* t = &m->t[i];
        if (t->mat >= mat_n) continue;
        vec3_t const a = s_world[t->a], b = s_world[t->b], c = s_world[t->c];
        if (!tri_faces_point(a, b, c, eye)) continue;
        mesh_mat_t const* mat = &mats[t->mat];
        if (mat->tex != NULL) {
            se_tex_vertex_t const tv[3] = {
                {a.x, a.y, a.z, t->uv[0][0], t->uv[0][1]},
                {b.x, b.y, b.z, t->uv[1][0], t->uv[1][1]},
                {c.x, c.y, c.z, t->uv[2][0], t->uv[2][1]},
            };
            scene_textured_tri(tv, mat->tex, mat->flags);
        } else {
            scene_tri(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, mat->argb, mat->flags);
        }
    }
}
