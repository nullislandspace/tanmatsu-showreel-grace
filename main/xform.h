#pragma once
// =====================================================================
//  Showreel  --  vectors, transforms and paths
// ---------------------------------------------------------------------
//  Plain math, no engine calls (the host-side mesh check compiles it).
//
//  Axes follow the engine's camera (se_scene.h): +x right, +y up, +z
//  forward. Models use the same convention -- nose along +z, roof along
//  +y -- so a model transform and the camera basis are built the same
//  way. Rotations match render_set_camera_6dof():
//
//      R = Ry(yaw) * Rx(pitch) * Rz(roll)
//      forward = (sin yaw cos pitch, -sin pitch, cos yaw cos pitch)
//
//  so positive pitch tips the nose DOWN and positive roll turns the
//  right wing up.
//
//  Face orientation: a triangle (a, b, c) faces the direction of
//  (b - a) x (c - a). Every mesh here is built with that normal pointing
//  out of the solid ("outward"); tri_faces_point() is the back-face test.
// =====================================================================

#include <math.h>
#include <stdbool.h>

typedef struct {
    float x, y, z;
} vec3_t;

static inline vec3_t v3(float x, float y, float z) {
    return (vec3_t){x, y, z};
}
static inline vec3_t v3_add(vec3_t a, vec3_t b) {
    return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}
static inline vec3_t v3_sub(vec3_t a, vec3_t b) {
    return v3(a.x - b.x, a.y - b.y, a.z - b.z);
}
static inline vec3_t v3_scale(vec3_t a, float s) {
    return v3(a.x * s, a.y * s, a.z * s);
}
static inline vec3_t v3_lerp(vec3_t a, vec3_t b, float t) {
    return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}
static inline float v3_dot(vec3_t a, vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline vec3_t v3_cross(vec3_t a, vec3_t b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float v3_len(vec3_t a) {
    return sqrtf(v3_dot(a, a));
}
static inline vec3_t v3_norm(vec3_t a) {
    float const l = v3_len(a);
    return (l > 1e-12f) ? v3_scale(a, 1.0f / l) : v3(0.0f, 0.0f, 1.0f);
}

// A 3x3 rotation, stored as its three columns: where the model's +x
// (right), +y (up) and +z (forward) axes point in the world.
typedef struct {
    vec3_t right, up, fwd;
} mat3_t;

// R = Ry(yaw) * Rx(pitch) * Rz(roll): the engine camera's convention.
mat3_t               mat3_from_ypr(float yaw, float pitch, float roll);
// Rotation about a single world axis.
mat3_t               mat3_rot_x(float a);
mat3_t               mat3_rot_y(float a);
mat3_t               mat3_rot_z(float a);
// a * b (apply b first, then a).
mat3_t               mat3_mul(mat3_t const* a, mat3_t const* b);
// Rotation whose forward is `fwd` and whose up is as close to `up_hint`
// as the forward allows, then rolled by `roll` about the forward axis.
// If fwd and up_hint are (nearly) parallel, a fallback up is used.
mat3_t               mat3_from_fwd_up(vec3_t fwd, vec3_t up_hint, float roll);
static inline vec3_t mat3_apply(mat3_t const* m, vec3_t p) {
    return v3(m->right.x * p.x + m->up.x * p.y + m->fwd.x * p.z, m->right.y * p.x + m->up.y * p.y + m->fwd.y * p.z,
              m->right.z * p.x + m->up.z * p.y + m->fwd.z * p.z);
}

// Model -> world: scale, rotate, then translate.
typedef struct {
    mat3_t r;
    vec3_t pos;
    float  scale;
} xform_t;

static inline vec3_t xform_apply(xform_t const* x, vec3_t p) {
    return v3_add(x->pos, mat3_apply(&x->r, v3_scale(p, x->scale)));
}

// Camera angles that look from `eye` towards `target`, for
// render_set_camera_6dof() (roll is passed through unchanged).
void look_at_angles(vec3_t eye, vec3_t target, float* yaw, float* pitch);

// Back-face test: true if the outward face (a, b, c) is turned towards
// the point `eye`.
static inline bool tri_faces_point(vec3_t a, vec3_t b, vec3_t c, vec3_t eye) {
    vec3_t const n  = v3_cross(v3_sub(b, a), v3_sub(c, a));
    vec3_t const fc = v3_scale(v3_add(v3_add(a, b), c), 1.0f / 3.0f);
    return v3_dot(n, v3_sub(eye, fc)) > 0.0f;
}

// --- Easing and paths -------------------------------------------------

static inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
// 0 at e0, 1 at e1, smooth (zero slope) at both ends.
static inline float smoothstep(float e0, float e1, float x) {
    float const t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// A path through `n` points, reached at times t0, t0 + dt, t0 + 2 dt ...
// (uniform Catmull-Rom: it passes through every point, with a
// continuous tangent). Before t0 / after the last point the path
// continues in a straight line along its end tangent, so a ship can fly
// in from off screen or out of it without a special case.
typedef struct {
    vec3_t const* pts;
    int           n;   // >= 2
    float         t0;  // time at pts[0]
    float         dt;  // time between consecutive points
} path_t;

vec3_t path_pos(path_t const* p, float t);
// Velocity (world units per second) at time t.
vec3_t path_vel(path_t const* p, float t);

// Deterministic pseudo-random value in [0, 1) for an integer key and a
// seed -- the "random" in anything that must be a pure function of time.
float hash01(int key, unsigned seed);
// Smooth value noise in [0, 1): hash01 at integer steps of t * rate,
// interpolated between them.
float value_noise(float t, float rate, unsigned seed);
