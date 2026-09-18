// =====================================================================
//  Showreel  --  host-side scene check (make scenecheck)
// ---------------------------------------------------------------------
//  Runs the REAL scene and asset code on the host, frame by frame at the
//  reel's 30 fps, against a stand-in engine (tools/host/engine_stub.c)
//  that records instead of drawing, and checks what the badge would
//  only show:
//
//    near plane   the nearest VISIBLE point of every mesh object: each
//                 of its (front-facing, submitted) triangles is clipped
//                 to the view frustum, and the part nearer than the near
//                 plane is what the engine cuts away (F-22). Nearer than
//                 RENDER_NEAR_CLIP_Z fails, unless the scene allows that
//                 object to cross; nearer than the plane + NEAR_MARGIN
//                 warns.
//    lists        emitted entries per frame (a triangle crossing the
//                 near plane can become two) against the engine caps,
//                 peak per shot. Over a cap fails, over 90% warns.
//    clearances   the closest approach between every two mesh objects
//                 (vertex-to-triangle, both ways). Touching (< CONTACT)
//                 fails unless the scene allows that pair.
//    framing      each object's largest on-screen extent per shot.
//
//  Mesh objects are the mesh_submit() calls of a frame, named after the
//  mesh (mesh_t.name) and numbered in submit order: marauder#0,
//  marauder#1. Flames, beams and stars are not objects; they only count
//  towards the lists.
//
//  Before the scenes, a self-test runs synthetic cases the checker must
//  catch (an eye 0.3 from a box, two boxes overlapping, one textured
//  triangle over the cap) and one it must pass, and the horizon
//  (horizon.c) is tested against the projection over random camera
//  poses, upside down included.
//
//  Usage: scenecheck [-v] [scene ...]    (default: every scene below)
//  Exit status 0 = no failures, 1 = a scene failed, 5 = usage or the
//  self-test failed.
// =====================================================================

#include "host/scenecheck.h"
#include <float.h>
#include <fnmatch.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "horizon.h"
#include "mesh_render.h"
#include "scenes/scenes.h"
#include "synthengine3d.h"

#define FPS         30
#define NEAR        RENDER_NEAR_CLIP_Z
#define NEAR_MARGIN 0.1f
#define CONTACT     0.02f
#define TRI_CAP     4096  // se_scene.c SCENE_TRI_CAP (private)
#define LINE_CAP    4096  // se_scene.c SCENE_LINE_CAP (private)

// --- What to check ----------------------------------------------------------

typedef struct {
    scene_def_t const* scene;
    float              secs;        // 0: the scene's duration
    char const*        near_ok;     // objects allowed through the near plane, "a,b" (globs)
    char const*        contact_ok;  // object pairs allowed to touch, "a-b,c-d" (globs)
    char const*        why;         // why those are allowed
} check_t;

static check_t const CHECKS[] = {
    {&SCENE_TITLE, 0.0f, NULL, NULL, NULL},
    {&SCENE_MARAUDER_APPROACH, 0.0f, NULL, NULL, NULL},
    {&SCENE_PLANET_LANDING, 0.0f, NULL, "apron*-base*,player_ship*-base*",
     "the base stands on the apron; the ship lands on the pad (part of the base)"},
    {&SCENE_MARAUDER_PURSUIT, 0.0f, NULL, NULL, NULL},
    {&SCENE_SPACESTATION_FLYBY, 0.0f, NULL, NULL, NULL},
    {&SCENE_TURNTABLE, 10.0f, NULL, NULL, NULL},
    {&SCENE_ASSET_VIEWER, 0.0f, NULL, "marauder/p*-marauder/p*,marauder/p*-fireball*,fireball*-fireball*,apron*-base*",
     "an exploding ship's parts start out touching each other and the fireball; the base stands on the apron"},
    {&SCENE_HORIZON_TEST, 0.0f, NULL, NULL, NULL},
};
#define CHECK_N ((int)(sizeof(CHECKS) / sizeof(CHECKS[0])))

// --- Per-scene statistics -----------------------------------------------------

#define MAX_OBJ    16  // mesh objects per frame
#define MAX_LABELS 32  // distinct objects per scene
#define MAX_SHOTS  16
#define MAX_SPANS  8
#define LABEL_LEN  48

typedef struct {
    float t0, t1;
} span_t;

typedef struct {
    char   label[LABEL_LEN];
    int    frames;  // frames it was submitted in
    float  min_z;   // nearest visible point
    float  min_z_t;
    char   min_z_shot[24];
    int    clipped_frames;    // visible geometry nearer than NEAR
    int    margin_frames;     // ... nearer than NEAR + NEAR_MARGIN (not clipped)
    span_t spans[MAX_SPANS];  // time ranges with clipping
    int    span_n;
    int    last_clip_frame;
} label_stat_t;

typedef struct {
    char  name[24];
    int   frames;
    int   tri, ttri, line, point;                // peaks
    float max_w[MAX_LABELS], max_h[MAX_LABELS];  // largest on-screen extent per label
    float x0[MAX_LABELS], x1[MAX_LABELS];        // leftmost / rightmost screen x reached
} shot_stat_t;

typedef struct {
    int   a, b;  // label indices, a < b
    float d, t;
} pair_stat_t;

static label_stat_t s_labels[MAX_LABELS];
static int          s_label_n;
static shot_stat_t  s_shots[MAX_SHOTS];
static int          s_shot_n;
static pair_stat_t  s_pairs[MAX_LABELS * MAX_LABELS / 2];
static int          s_pair_n;
static bool         s_verbose;
static FILE*        s_out;  // report output (the self-test silences it)

// --- The current frame --------------------------------------------------------

typedef struct {
    int           label;     // index into s_labels
    char          base[32];  // the label without the #k
    mesh_t const* mesh;
    int           v0, vn, t0, tn;  // what was submitted: all of it or one part
    xform_t       x;
    float         min_z;               // nearest visible point this frame
    float         sx0, sy0, sx1, sy1;  // on-screen bounds of what is drawn
    bool          drawn;
} obj_t;

static obj_t s_obj[MAX_OBJ];
static int   s_obj_n;
static int   s_cur = -1;  // object being submitted, -1 = none
static int   s_n_tri, s_n_ttri, s_n_line, s_n_point;

static int label_index(char const* label) {
    for (int i = 0; i < s_label_n; i++) {
        if (strcmp(s_labels[i].label, label) == 0) return i;
    }
    if (s_label_n == MAX_LABELS) {
        fprintf(stderr, "scenecheck: too many objects\n");
        exit(5);
    }
    label_stat_t* l = &s_labels[s_label_n];
    memset(l, 0, sizeof(*l));
    snprintf(l->label, sizeof(l->label), "%s", label);
    l->min_z           = FLT_MAX;
    l->last_clip_frame = -2;
    return s_label_n++;
}

// The wrappers around the real mesh_submit / mesh_submit_part
// (mesh_render.c is compiled with them renamed to *_real): they name the
// object, then let the real code transform, back-face cull and submit
// it. A whole mesh is "name#k", one part of it "name/pN#k", k counting
// the same object submitted earlier in the frame.
void mesh_submit_real(mesh_t const* m, xform_t const* x, mesh_mat_t const* mats, int mat_n);
void mesh_submit_part_real(mesh_t const* m, int part, xform_t const* x, mesh_mat_t const* mats, int mat_n);

static void obj_begin(mesh_t const* m, int part, xform_t const* x) {
    if (s_obj_n == MAX_OBJ) {
        fprintf(stderr, "scenecheck: too many objects in one frame\n");
        exit(5);
    }
    obj_t* o         = &s_obj[s_obj_n];
    *o               = (obj_t){.mesh  = m,
                               .x     = *x,
                               .v0    = 0,
                               .vn    = m->vn,
                               .t0    = 0,
                               .tn    = m->tn,
                               .min_z = FLT_MAX,
                               .sx0   = FLT_MAX,
                               .sy0   = FLT_MAX,
                               .sx1   = -FLT_MAX,
                               .sy1   = -FLT_MAX};
    char const* name = m->name ? m->name : "mesh";
    if (part >= 0) {
        mesh_part_t const* p = &m->parts[part];
        o->v0                = p->v0;
        o->vn                = p->vn;
        o->t0                = p->t0;
        o->tn                = p->tn;
        snprintf(o->base, sizeof(o->base), "%s/p%d", name, part);
    } else {
        snprintf(o->base, sizeof(o->base), "%s", name);
    }
    int k = 0;
    for (int i = 0; i < s_obj_n; i++) k += strcmp(s_obj[i].base, o->base) == 0;
    char label[LABEL_LEN];
    snprintf(label, sizeof(label), "%s#%d", o->base, k);
    o->label = label_index(label);
    s_cur    = s_obj_n++;
}

void mesh_submit(mesh_t const* m, xform_t const* x, mesh_mat_t const* mats, int mat_n) {
    obj_begin(m, -1, x);
    mesh_submit_real(m, x, mats, mat_n);
    s_cur = -1;
}

void mesh_submit_part(mesh_t const* m, int part, xform_t const* x, mesh_mat_t const* mats, int mat_n) {
    if (m == NULL || part < 0 || part >= m->pn) return;  // as the real one
    obj_begin(m, part, x);
    mesh_submit_part_real(m, part, x, mats, mat_n);
    s_cur = -1;
}

// --- Clipping -------------------------------------------------------------------

#define POLY_MAX 16

// Keep the part of polygon `p` (n vertices) where dot(nrm, v) + d >= 0.
static int clip_plane(vec3_t* p, int n, vec3_t nrm, float d) {
    vec3_t out[POLY_MAX];
    int    m = 0;
    for (int i = 0; i < n; i++) {
        vec3_t const a = p[i], b = p[(i + 1) % n];
        float const  da = v3_dot(nrm, a) + d, db = v3_dot(nrm, b) + d;
        if (da >= 0.0f && m < POLY_MAX) out[m++] = a;
        if ((da >= 0.0f) != (db >= 0.0f) && m < POLY_MAX) out[m++] = v3_lerp(a, b, da / (da - db));
    }
    memcpy(p, out, (size_t)m * sizeof(vec3_t));
    return m;
}

// Clip a camera-space polygon to the view frustum's sides (the planes
// through the eye and the screen edges) and to z >= z_min.
static int clip_frustum(vec3_t* p, int n, float z_min) {
    float const xl = -RENDER_HALF_W / RENDER_FOCAL_LEN;
    float const xr = ((float)DISPLAY_LOG_W - RENDER_HALF_W) / RENDER_FOCAL_LEN;
    float const yt = RENDER_HORIZON_Y / RENDER_FOCAL_LEN;
    float const yb = (RENDER_HORIZON_Y - (float)DISPLAY_LOG_H) / RENDER_FOCAL_LEN;
    n              = clip_plane(p, n, v3(0.0f, 0.0f, 1.0f), -z_min);
    if (n) n = clip_plane(p, n, v3(1.0f, 0.0f, -xl), 0.0f);
    if (n) n = clip_plane(p, n, v3(-1.0f, 0.0f, xr), 0.0f);
    if (n) n = clip_plane(p, n, v3(0.0f, 1.0f, -yb), 0.0f);
    if (n) n = clip_plane(p, n, v3(0.0f, -1.0f, yt), 0.0f);
    return n;
}

// --- Hooks from the engine stand-in -------------------------------------------------

void sc_tri(vec3_t const v[3], bool textured) {
    vec3_t c[3];
    int    behind = 0;
    for (int i = 0; i < 3; i++) {
        c[i]    = sc_to_camera(v[i]);
        behind += c[i].z < NEAR;
    }
    // Entries the engine appends: none when all three are behind, two
    // when clipping leaves a quad.
    int const entries = behind == 3 ? 0 : behind == 1 ? 2 : 1;
    if (textured)
        s_n_ttri += entries;
    else
        s_n_tri += entries;
    if (s_cur < 0) return;

    obj_t* o = &s_obj[s_cur];
    vec3_t p[POLY_MAX];
    memcpy(p, c, sizeof(c));
    int const n = clip_frustum(p, 3, 1e-4f);  // everything in view, however near
    if (n == 0) return;
    for (int i = 0; i < n; i++) {
        if (p[i].z < o->min_z) o->min_z = p[i].z;
    }
    // What is actually drawn: the part beyond the near plane.
    int const m = clip_plane(p, n, v3(0.0f, 0.0f, 1.0f), -NEAR);
    for (int i = 0; i < m; i++) {
        float sx, sy;
        sc_project(p[i], &sx, &sy);
        if (sx < o->sx0) o->sx0 = sx;
        if (sx > o->sx1) o->sx1 = sx;
        if (sy < o->sy0) o->sy0 = sy;
        if (sy > o->sy1) o->sy1 = sy;
        o->drawn = true;
    }
}

void sc_line(vec3_t a, vec3_t b) {
    vec3_t const ca = sc_to_camera(a), cb = sc_to_camera(b);
    if (ca.z < NEAR && cb.z < NEAR) return;
    s_n_line++;
}

void sc_point(vec3_t p) {
    vec3_t const c = sc_to_camera(p);
    if (c.z < NEAR) return;
    float sx, sy;
    sc_project(c, &sx, &sy);
    int const px = (int)lroundf(sx), py = (int)lroundf(sy);
    if (px < 0 || px > DISPLAY_LOG_W - 1 || py < 0 || py > DISPLAY_LOG_H - 1) return;
    s_n_point++;
}

// --- Clearances ---------------------------------------------------------------------

// Closest point on triangle abc to p (Ericson, Real-Time Collision
// Detection, 5.1.5), returned as the distance.
static float point_tri_dist(vec3_t p, vec3_t a, vec3_t b, vec3_t c) {
    vec3_t const ab = v3_sub(b, a), ac = v3_sub(c, a), ap = v3_sub(p, a);
    float const  d1 = v3_dot(ab, ap), d2 = v3_dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return v3_len(ap);
    vec3_t const bp = v3_sub(p, b);
    float const  d3 = v3_dot(ab, bp), d4 = v3_dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return v3_len(bp);
    float const vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) return v3_len(v3_sub(p, v3_add(a, v3_scale(ab, d1 / (d1 - d3)))));
    vec3_t const cp = v3_sub(p, c);
    float const  d5 = v3_dot(ab, cp), d6 = v3_dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return v3_len(cp);
    float const vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) return v3_len(v3_sub(p, v3_add(a, v3_scale(ac, d2 / (d2 - d6)))));
    float const va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        vec3_t const bc = v3_sub(c, b);
        return v3_len(v3_sub(p, v3_add(b, v3_scale(bc, (d4 - d3) / ((d4 - d3) + (d5 - d6))))));
    }
    float const  denom = 1.0f / (va + vb + vc);
    vec3_t const q     = v3_add(a, v3_add(v3_scale(ab, vb * denom), v3_scale(ac, vc * denom)));
    return v3_len(v3_sub(p, q));
}

typedef struct {
    vec3_t* v;  // world vertices (indexed as the mesh's; only the object's range is set)
    int     v0, vn, t0, tn;
    vec3_t  centre;  // bounding sphere
    float   radius;
    vec3_t* tc;  // per-triangle bounding spheres
    float*  tr;
} world_t;

static void world_build(world_t* w, obj_t const* o) {
    mesh_t const* m = o->mesh;
    w->v0           = o->v0;
    w->vn           = o->vn;
    w->t0           = o->t0;
    w->tn           = o->tn;
    w->v            = malloc((size_t)m->vn * sizeof(vec3_t));
    w->tc           = malloc((size_t)m->tn * sizeof(vec3_t));
    w->tr           = malloc((size_t)m->tn * sizeof(float));
    vec3_t sum      = v3(0.0f, 0.0f, 0.0f);
    for (int i = o->v0; i < o->v0 + o->vn; i++) {
        w->v[i] = xform_apply(&o->x, m->v[i]);
        sum     = v3_add(sum, w->v[i]);
    }
    w->centre = v3_scale(sum, 1.0f / (float)(o->vn ? o->vn : 1));
    w->radius = 0.0f;
    for (int i = o->v0; i < o->v0 + o->vn; i++) {
        float const r = v3_len(v3_sub(w->v[i], w->centre));
        if (r > w->radius) w->radius = r;
    }
    for (int i = o->t0; i < o->t0 + o->tn; i++) {
        vec3_t const a = w->v[m->t[i].a], b = w->v[m->t[i].b], c = w->v[m->t[i].c];
        vec3_t const k = v3_scale(v3_add(v3_add(a, b), c), 1.0f / 3.0f);
        float        r = v3_len(v3_sub(a, k));
        if (v3_len(v3_sub(b, k)) > r) r = v3_len(v3_sub(b, k));
        if (v3_len(v3_sub(c, k)) > r) r = v3_len(v3_sub(c, k));
        w->tc[i] = k;
        w->tr[i] = r;
    }
}

static void world_free(world_t* w) {
    free(w->v);
    free(w->tc);
    free(w->tr);
}

// Smallest distance from any vertex of `a` to any triangle of `b`, or
// `best` if nothing is nearer.
static float verts_to_tris(world_t const* a, world_t const* b, mesh_t const* bm, float best) {
    for (int i = a->v0; i < a->v0 + a->vn; i++) {
        vec3_t const p = a->v[i];
        if (v3_len(v3_sub(p, b->centre)) - b->radius >= best) continue;
        for (int j = b->t0; j < b->t0 + b->tn; j++) {
            if (v3_len(v3_sub(p, b->tc[j])) - b->tr[j] >= best) continue;
            float const d = point_tri_dist(p, b->v[bm->t[j].a], b->v[bm->t[j].b], b->v[bm->t[j].c]);
            if (d < best) best = d;
        }
    }
    return best;
}

static pair_stat_t* pair_stat(int a, int b) {
    if (a > b) {
        int const t = a;
        a           = b;
        b           = t;
    }
    for (int i = 0; i < s_pair_n; i++) {
        if (s_pairs[i].a == a && s_pairs[i].b == b) return &s_pairs[i];
    }
    s_pairs[s_pair_n] = (pair_stat_t){a, b, FLT_MAX, 0.0f};
    return &s_pairs[s_pair_n++];
}

static void clearances(float t) {
    world_t w[MAX_OBJ];
    bool    built[MAX_OBJ] = {false};
    for (int i = 0; i < s_obj_n; i++) {
        for (int j = i + 1; j < s_obj_n; j++) {
            pair_stat_t* ps = pair_stat(s_obj[i].label, s_obj[j].label);
            if (!built[i]) world_build(&w[i], &s_obj[i]), built[i] = true;
            if (!built[j]) world_build(&w[j], &s_obj[j]), built[j] = true;
            // Bounding spheres first: most frames cannot beat the
            // closest approach already found.
            float const lb = v3_len(v3_sub(w[i].centre, w[j].centre)) - w[i].radius - w[j].radius;
            if (lb >= ps->d) continue;
            float d = verts_to_tris(&w[i], &w[j], s_obj[j].mesh, ps->d);
            d       = verts_to_tris(&w[j], &w[i], s_obj[i].mesh, d);
            if (d < ps->d) {
                ps->d = d;
                ps->t = t;
            }
        }
    }
    for (int i = 0; i < s_obj_n; i++) {
        if (built[i]) world_free(&w[i]);
    }
}

// --- Frame bookkeeping ------------------------------------------------------------------

static shot_stat_t* shot_stat(char const* name) {
    for (int i = 0; i < s_shot_n; i++) {
        if (strcmp(s_shots[i].name, name) == 0) return &s_shots[i];
    }
    if (s_shot_n == MAX_SHOTS) return &s_shots[MAX_SHOTS - 1];
    shot_stat_t* s = &s_shots[s_shot_n++];
    memset(s, 0, sizeof(*s));
    snprintf(s->name, sizeof(s->name), "%s", name);
    return s;
}

static void frame_end(int frame, float t, char const* shot) {
    shot_stat_t* s = shot_stat(shot);
    s->frames++;
    if (s_n_tri > s->tri) s->tri = s_n_tri;
    if (s_n_ttri > s->ttri) s->ttri = s_n_ttri;
    if (s_n_line > s->line) s->line = s_n_line;
    if (s_n_point > s->point) s->point = s_n_point;

    for (int i = 0; i < s_obj_n; i++) {
        obj_t const*  o = &s_obj[i];
        label_stat_t* l = &s_labels[o->label];
        l->frames++;
        if (o->drawn) {
            float const w = o->sx1 - o->sx0, h = o->sy1 - o->sy0;
            if (s->max_w[o->label] == 0.0f && s->max_h[o->label] == 0.0f) {
                s->x0[o->label] = o->sx0;
                s->x1[o->label] = o->sx1;
            }
            if (w > s->max_w[o->label]) s->max_w[o->label] = w;
            if (h > s->max_h[o->label]) s->max_h[o->label] = h;
            if (o->sx0 < s->x0[o->label]) s->x0[o->label] = o->sx0;
            if (o->sx1 > s->x1[o->label]) s->x1[o->label] = o->sx1;
        }
        if (o->min_z == FLT_MAX) continue;  // not in view
        if (o->min_z < l->min_z) {
            l->min_z   = o->min_z;
            l->min_z_t = t;
            snprintf(l->min_z_shot, sizeof(l->min_z_shot), "%s", shot);
        }
        if (o->min_z < NEAR) {
            l->clipped_frames++;
            if (l->last_clip_frame == frame - 1 && l->span_n > 0) {
                l->spans[l->span_n - 1].t1 = t;
            } else if (l->span_n < MAX_SPANS) {
                l->spans[l->span_n++] = (span_t){t, t};
            }
            l->last_clip_frame = frame;
            if (s_verbose) fprintf(s_out, "    %6.2f s  %-14s visible down to z %.3f\n", t, l->label, o->min_z);
        } else if (o->min_z < NEAR + NEAR_MARGIN) {
            l->margin_frames++;
        }
    }
    clearances(t);
}

// "a,b,c" contains `item`?
// "a,b,c" has a pattern matching `item`? Patterns are shell globs
// (fnmatch): "marauder/p*" is any part of a marauder.
static bool listed(char const* list, char const* item) {
    if (list == NULL) return false;
    for (char const* p = list; *p;) {
        char const*  e   = strchr(p, ',');
        size_t const len = e ? (size_t)(e - p) : strlen(p);
        char         pat[96];
        snprintf(pat, sizeof(pat), "%.*s", (int)len, p);
        if (fnmatch(pat, item, 0) == 0) return true;
        if (!e) break;
        p = e + 1;
    }
    return false;
}

// --- One scene ----------------------------------------------------------------------------

static int check_scene(check_t const* c) {
    scene_def_t const* sc   = c->scene;
    float const        secs = c->secs > 0.0f ? c->secs : sc->duration;
    s_label_n = s_shot_n = s_pair_n = 0;

    if (sc->init) sc->init("textures");
    if (sc->enter) sc->enter();
    int frames = 0;
    for (int f = 0; (float)f < secs * FPS - 1e-3f; f++) {
        double const t = (double)f / FPS;
        s_obj_n        = 0;
        s_n_tri = s_n_ttri = s_n_line = s_n_point = 0;
        if (sc->camera) sc->camera(t);
        if (sc->submit) sc->submit(t);
        char const* shot = sc->shot ? sc->shot(t) : NULL;
        frame_end(f, (float)t, shot && shot[0] ? shot : "-");
        frames++;
    }
    if (sc->shutdown) sc->shutdown();

    int fails = 0, warns = 0;
    fprintf(s_out, "== %s (%.1f s, %d frames)\n", sc->name, (double)secs, frames);

    fprintf(s_out, "  lists (peak entries per frame; caps %d / %d / %d / %d)\n", TRI_CAP, SE_SCENE_TEXTURED_TRI_CAP,
            LINE_CAP, SE_SCENE_POINT_CAP);
    fprintf(s_out, "    %-12s %6s %6s %6s %6s %6s\n", "shot", "frames", "tris", "ttris", "lines", "points");
    for (int i = 0; i < s_shot_n; i++) {
        shot_stat_t const* s = &s_shots[i];
        fprintf(s_out, "    %-12s %6d %6d %6d %6d %6d\n", s->name, s->frames, s->tri, s->ttri, s->line, s->point);
        int const   v[4]   = {s->tri, s->ttri, s->line, s->point};
        int const   cap[4] = {TRI_CAP, SE_SCENE_TEXTURED_TRI_CAP, LINE_CAP, SE_SCENE_POINT_CAP};
        char const* nm[4]  = {"triangle", "textured triangle", "line", "point"};
        for (int k = 0; k < 4; k++) {
            if (v[k] > cap[k]) {
                fprintf(s_out, "    FAIL: %s list over its cap in shot %s (%d > %d)\n", nm[k], s->name, v[k], cap[k]);
                fails++;
            } else if (v[k] * 10 > cap[k] * 9) {
                fprintf(s_out, "    WARN: %s list above 90%% of its cap in shot %s (%d / %d)\n", nm[k], s->name, v[k],
                        cap[k]);
                warns++;
            }
        }
    }

    fprintf(s_out, "  near plane (%.2f, margin %.2f): nearest visible point per object\n", (double)NEAR,
            (double)NEAR_MARGIN);
    for (int i = 0; i < s_label_n; i++) {
        label_stat_t const* l = &s_labels[i];
        if (l->min_z == FLT_MAX) {
            fprintf(s_out, "    %-16s never in view\n", l->label);
            continue;
        }
        fprintf(s_out, "    %-16s %6.3f at %5.2f s (%s)", l->label, (double)l->min_z, (double)l->min_z_t,
                l->min_z_shot);
        bool const ok = listed(c->near_ok, l->label);
        if (l->clipped_frames) {
            fprintf(s_out, "  clipped in %d frames:", l->clipped_frames);
            for (int k = 0; k < l->span_n; k++)
                fprintf(s_out, " %.2f-%.2f", (double)l->spans[k].t0, (double)l->spans[k].t1);
            if (l->span_n == MAX_SPANS) fprintf(s_out, " ...");
            if (ok) {
                fprintf(s_out, "  (allowed)\n");
            } else {
                fprintf(s_out, "\n    FAIL: %s crosses the near plane\n", l->label);
                fails++;
            }
        } else if (l->margin_frames && !ok) {
            fprintf(s_out, "  WARN: within the margin in %d frames\n", l->margin_frames);
            warns++;
        } else {
            fprintf(s_out, "\n");
        }
    }
    if (c->near_ok) fprintf(s_out, "    allowed through the near plane: %s -- %s\n", c->near_ok, c->why ? c->why : "");

    if (s_pair_n) {
        fprintf(s_out, "  clearances (closest approach, world units)\n");
        for (int i = 0; i < s_pair_n; i++) {
            pair_stat_t const* p = &s_pairs[i];
            char               pair[2 * LABEL_LEN + 2];
            snprintf(pair, sizeof(pair), "%s-%s", s_labels[p->a].label, s_labels[p->b].label);
            fprintf(s_out, "    %-30s %8.3f at %5.2f s", pair, (double)p->d, (double)p->t);
            char rev[2 * LABEL_LEN + 2];
            snprintf(rev, sizeof(rev), "%s-%s", s_labels[p->b].label, s_labels[p->a].label);
            if (p->d < CONTACT) {
                if (listed(c->contact_ok, pair) || listed(c->contact_ok, rev)) {
                    fprintf(s_out, "  touching (allowed)\n");
                } else {
                    fprintf(s_out, "\n    FAIL: %s touch or intersect\n", pair);
                    fails++;
                }
            } else {
                fprintf(s_out, "\n");
            }
        }
    }

    if (c->contact_ok) fprintf(s_out, "    allowed to touch: %s -- %s\n", c->contact_ok, c->why ? c->why : "");
    fprintf(s_out, "  framing (largest on-screen extent per shot, px, w x h; screen x reached)\n");
    for (int i = 0; i < s_shot_n; i++) {
        shot_stat_t const* s = &s_shots[i];
        fprintf(s_out, "    %-12s", s->name);
        for (int k = 0; k < s_label_n; k++) {
            if (s->max_w[k] > 0.0f)
                fprintf(s_out, "  %s %.0fx%.0f [x %.0f..%.0f]", s_labels[k].label, (double)s->max_w[k],
                        (double)s->max_h[k], (double)s->x0[k], (double)s->x1[k]);
        }
        fprintf(s_out, "\n");
    }
    fprintf(s_out, "  %s (%d failure%s, %d warning%s)\n\n", fails ? "FAIL" : "OK", fails, fails == 1 ? "" : "s", warns,
            warns == 1 ? "" : "s");
    return fails;
}

// --- Self-test ----------------------------------------------------------------------------
// Synthetic scenes the checker must pass or fail, run before the real
// ones (as meshcheck's negative tests): a checker that cannot fail
// proves nothing.

static mesh_t s_box;
static int    s_st_case;

enum {
    ST_CLEAN,
    ST_NEAR,
    ST_CONTACT,
    ST_CAP,
    ST_COUNT
};

static void st_init(char const* dir) {
    (void)dir;
    mesh_init(&s_box);
    s_box.name = "box";
    mesh_box(&s_box, v3(-0.5f, -0.5f, -0.5f), v3(0.5f, 0.5f, 0.5f), 0, 1.0f);
}

static void st_shutdown(void) {
    mesh_free(&s_box);
}

static void st_submit(double t) {
    (void)t;
    mesh_mat_t const mat = {NULL, 0xFF808080u, 0};
    // Clean: eye 5 units in front of the box. Near: 0.3 from its face.
    render_set_camera_6dof(0.0f, 0.0f, s_st_case == ST_NEAR ? -0.8f : -5.0f, 0.0f, 0.0f, 0.0f);
    xform_t a = {mat3_from_ypr(0.0f, 0.0f, 0.0f), v3(0.0f, 0.0f, 0.0f), 1.0f};
    mesh_submit(&s_box, &a, &mat, 1);
    // A second box: 0.5 clear of the first, or overlapping it. Submitted
    // as a part (the box's only one), so the part path is tested too.
    a.pos = v3(s_st_case == ST_CONTACT ? 0.8f : 1.5f, 0.0f, 0.0f);
    mesh_submit_part(&s_box, 0, &a, &mat, 1);
    if (s_st_case == ST_CAP) {
        static se_texture_t* tex;
        if (tex == NULL) tex = se_texture_load("selftest", 0);
        se_tex_vertex_t const v[3] = {{0, 0, 2, 0, 0}, {0.1f, 0, 2, 1, 0}, {0, 0.1f, 2, 0, 1}};
        for (int i = 0; i <= SE_SCENE_TEXTURED_TRI_CAP; i++) scene_textured_tri(v, tex, 0);
    }
}

// The horizon (horizon.c) against the projection itself: for random
// camera poses -- rolled past 90 degrees too -- far points below eye
// height must land on the ground side of the line, far points above it
// on the sky side.
static bool horizon_test(void) {
    unsigned seed = 7;
    int      bad = 0, tested = 0;
    for (int i = 0; i < 2000; i++) {
        seed              = seed * 1664525u + 1013904223u;
        float const yaw   = (float)(seed >> 8) / 16777216.0f * 6.2831853f;
        seed              = seed * 1664525u + 1013904223u;
        float const pitch = ((float)(seed >> 8) / 16777216.0f - 0.5f) * 2.6f;  // +-75 degrees
        seed              = seed * 1664525u + 1013904223u;
        float const roll  = ((float)(seed >> 8) / 16777216.0f - 0.5f) * 6.2831853f;
        render_set_camera_6dof(3.0f, 5.0f, -2.0f, yaw, pitch, roll);
        horizon_t const hz = horizon_current();
        for (int k = 0; k < 16; k++) {
            float const a = yaw + ((float)k - 7.5f) * 0.2f;  // across the view
            for (int s = -1; s <= 1; s += 2) {
                vec3_t const p = v3(3.0f + 5000.0f * sinf(a), 5.0f + (float)s * 100.0f, -2.0f + 5000.0f * cosf(a));
                vec3_t const c = sc_to_camera(p);
                if (c.z < 1.0f) continue;
                float sx, sy;
                sc_project(c, &sx, &sy);
                if (sx < 0.0f || sx > DISPLAY_LOG_W || sy < 0.0f || sy > DISPLAY_LOG_H) continue;
                bool const below  = sy > horizon_y(&hz, sx);
                bool const ground = s < 0;
                tested++;
                if (below != (ground == hz.ground_below)) bad++;
            }
        }
    }
    if (bad || tested < 1000) {
        fprintf(stderr, "scenecheck self-test: horizon wrong for %d of %d points\n", bad, tested);
        return false;
    }
    return true;
}

static bool self_test(void) {
    static char const* const NAME[ST_COUNT] = {"clean", "near plane", "contact", "list cap"};
    scene_def_t const        st             = {
                           .name = "selftest", .duration = 0.2f, .init = st_init, .shutdown = st_shutdown, .submit = st_submit};
    check_t const c    = {&st, 0.0f, NULL, NULL, NULL};
    FILE* const   keep = s_out;
    bool          ok   = true;
    s_out              = fopen("/dev/null", "w");
    for (s_st_case = 0; s_st_case < ST_COUNT; s_st_case++) {
        int const  fails = check_scene(&c);
        bool const want  = s_st_case != ST_CLEAN;
        if ((fails > 0) != want) {
            fprintf(stderr, "scenecheck self-test: the %s case %s\n", NAME[s_st_case],
                    want ? "was not caught" : "failed");
            ok = false;
        }
    }
    fclose(s_out);
    s_out = keep;
    return ok && horizon_test();
}

int main(int argc, char** argv) {
    int  fails = 0, ran = 0;
    bool any = false;
    s_out    = stdout;
    if (!self_test()) return 5;
    fprintf(s_out, "self-test: clean, near plane, contact and list-cap cases behave; horizon sides OK\n\n");
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0)
            s_verbose = true;
        else
            any = true;
    }
    for (int k = 0; k < CHECK_N; k++) {
        bool want = !any;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], CHECKS[k].scene->name) == 0) want = true;
        }
        if (!want) continue;
        fails += check_scene(&CHECKS[k]);
        ran++;
    }
    if (ran == 0) {
        fprintf(stderr, "scenecheck: no such scene\n");
        return 5;
    }
    fprintf(s_out, "scenecheck: %d scene%s, %s\n", ran, ran == 1 ? "" : "s", fails ? "FAILED" : "all OK");
    return fails ? 1 : 0;
}
