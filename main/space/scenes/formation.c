// =====================================================================
//  Showreel  --  ships flying in formation (see formation.h)
// =====================================================================

#include "space/scenes/formation.h"
#include <math.h>

static float const TWO_PI = 6.2831853f;

// A unit vector by division rather than by multiplying with 1/len: a
// formation flying along an axis then gets an exact frame (-14 / 14 is
// exactly -1; -14 * (1/14) need not be), so slots and weave land on
// exactly the floats a world-axis formulation would give.
static vec3_t unit(vec3_t a) {
    float const l = v3_len(a);
    return l > 1e-12f ? v3(a.x / l, a.y / l, a.z / l) : v3(0.0f, 0.0f, 1.0f);
}

typedef struct {
    vec3_t right, up, fwd;
} frame_t;

static frame_t frame(formation_t const* f) {
    vec3_t const fwd   = unit(f->vel);
    vec3_t const right = unit(v3_cross(v3(0.0f, 1.0f, 0.0f), fwd));
    return (frame_t){right, v3_cross(fwd, right), fwd};
}

static vec3_t to_world(frame_t const* fr, vec3_t l) {
    return v3_add(v3_add(v3_scale(fr->right, l.x), v3_scale(fr->up, l.y)), v3_scale(fr->fwd, l.z));
}

vec3_t formation_centre(formation_t const* f, float t) {
    return v3_add(f->origin, v3_scale(f->vel, t));
}

vec3_t formation_to_world(formation_t const* f, vec3_t local) {
    frame_t const fr = frame(f);
    return to_world(&fr, local);
}

vec3_t formation_slot_pos(formation_t const* f, slot_t const* s, float t) {
    frame_t const fr = frame(f);
    return v3_add(formation_centre(f, t), to_world(&fr, s->offset));
}

void formation_motion(formation_t const* f, slot_t const* s, float t, vec3_t* p, vec3_t* v, vec3_t* a) {
    frame_t const  fr = frame(f);
    weave_t const* w  = &s->weave;
    float const    lw = TWO_PI * w->lat_hz, vw = TWO_PI * w->vert_hz;
    float const    lx = w->lat_amp * sinf(lw * t + w->lat_ph);
    float const    vy = w->vert_amp * sinf(vw * t + w->vert_ph);
    float const    dl = w->lat_amp * lw * cosf(lw * t + w->lat_ph);
    float const    dv = w->vert_amp * vw * cosf(vw * t + w->vert_ph);
    float const    al = -w->lat_amp * lw * lw * sinf(lw * t + w->lat_ph);
    float const    av = -w->vert_amp * vw * vw * sinf(vw * t + w->vert_ph);
    *p = v3_add(v3_add(formation_centre(f, t), to_world(&fr, s->offset)), to_world(&fr, v3(lx, vy, 0.0f)));
    *v = v3_add(f->vel, to_world(&fr, v3(dl, dv, 0.0f)));
    *a = to_world(&fr, v3(al, av, 0.0f));
}

xform_t formation_pose(formation_t const* f, slot_t const* s, float t, float scale) {
    vec3_t p, v, a;
    formation_motion(f, s, t, &p, &v, &a);
    vec3_t const fw    = v3_norm(v);
    vec3_t const right = v3_norm(v3_cross(v3(0.0f, 1.0f, 0.0f), fw));
    // Turning right dips the right wing: negative roll (xform.h).
    float const  bank  = clampf(-f->bank_per_acc * v3_dot(a, right), -f->bank_max, f->bank_max);
    float const  rock  = s->weave.rock_amp * sinf(TWO_PI * s->weave.rock_hz * t + s->weave.rock_ph);
    return (xform_t){mat3_from_fwd_up(fw, v3(0.0f, 1.0f, 0.0f), bank + rock), p, scale};
}
