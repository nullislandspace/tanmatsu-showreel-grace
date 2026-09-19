// =====================================================================
//  Showreel scene  --  asteroid_ambush (scene 9)
// ---------------------------------------------------------------------
//  Another solar system (system2.h). The hero ship drops out of warp,
//  brakes hard and tucks in behind a big asteroid. A few seconds later
//  the marauders warp in on its trail, fly on and pass the rock on the
//  other side. The hero slides out behind them, falls in on their tail
//  and opens fire with its blue guns (D-28).
//
//  World: the big asteroid at the origin; the marauders fly a straight
//  line along -z, 12 units to its right (+x); the hero hides 9.5 units
//  to its left.
//
//  Every beam runs along the hero's nose. Most pass close by a marauder
//  -- the hero's aim circles each in turn -- and three strike one,
//  sparking off its hull; a beam never runs on through a ship or a rock.
//
//  Four shots: out on the hiding side of the rock (the arrival and the
//  hiding); low by the rock on the marauders' line as they drop in and
//  roar past; riding behind the hero as it comes out after them; ahead
//  of the pair looking back, as it opens fire -- its beams come at the
//  lens and leave the frame past it (a beam fired away from a camera
//  behind the guns would shrink to a point in mid-screen).
// =====================================================================

#include <math.h>
#include "camera.h"
#include "space/assets/asteroid.h"
#include "space/assets/explosion.h"
#include "space/assets/laser.h"
#include "space/assets/marauder.h"
#include "space/assets/player_ship.h"
#include "space/assets/warp.h"
#include "space/scenes/formation.h"
#include "space/scenes/system2.h"
#include "space/space.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define SCENE_SECS    16.0f
#define T_HERO_IN     1.0f  // the hero's warp-in ends: normal pose from here
#define T_HIDE        5.0f  // at rest behind the rock
#define T_GREEN_IN    5.0f  // the marauders' warp-ins end
#define T_YELLOW_IN   5.4f
#define T_PASS        7.5f   // the formation passes the rock (z = 0)
#define T_EMERGE      9.0f   // the hero moves out ...
#define T_ON_TAIL     13.0f  // ... and sits on their tail from here
#define FIRE0         12.8f  // the hero's guns, alternating
#define FIRE1         15.6f
#define FIRE_INTERVAL 0.15f
#define AIM_TURN      0.4f   // seconds to bring the nose on / off the aim
#define T_TO_YELLOW0  14.1f  // the aim moves from the green ship to the yellow one
#define T_TO_YELLOW1  14.45f

#define SHOT_PASS   4.6f  // before the marauders drop in, so their flashes are in it
#define SHOT_AMBUSH 9.0f
#define SHOT_FIRE   12.4f  // ahead of the pair, looking back, for the guns

// --- Ships ------------------------------------------------------------------
#define HERO_SPAN     1.0f
#define MARAUDER_SPAN 0.9f

// The hero's aim circles its target this far off the line of fire, clear
// of the hull (half-span 0.45) ...
#define MISS       1.15f
#define MISS_TURNS 0.55f  // ... going round this many times a second
// ... except for the hits, where it closes onto the ship for a moment.
#define HIT_CLOSE  0.08f  // seconds: the aim's dip onto the hull, around the lit beam

// The hero's arrival: out of warp at 2x path speed, braking to rest at
// the last point (path time 0..4 mapped onto T_HERO_IN..T_HIDE).
static vec3_t const ARRIVE_PTS[] = {
    {-3.0f, 3.5f, 40.0f}, {-6.0f, 2.5f, 24.0f}, {-9.0f, 1.2f, 11.0f}, {-9.8f, 0.6f, 4.5f}, {-9.5f, 0.5f, 2.0f},
};
static path_t const ARRIVE = {ARRIVE_PTS, 5, 0.0f, 1.0f};
#define ARRIVE_SPAN 4.0f  // path time of the last point

// The marauders: through (12, 1, 0) at T_PASS, along -z at 14 units/s.
static formation_t const FORMATION = {
    .origin = {12.0f, 1.0f, 14.0f * T_PASS}, .vel = {0.0f, 0.0f, -14.0f}, .bank_per_acc = 0.06f, .bank_max = 0.6f};
static slot_t const SLOT_GREEN = {
    .offset = {-0.9f, 0.15f, 0.0f},
    .weave  = {0.2f, 0.29f, 0.7f, 0.1f, 0.41f, 0.2f, 0.08f, 0.7f, 1.2f},
};
static slot_t const SLOT_YELLOW = {
    .offset = {0.9f, -0.1f, -0.8f},
    .weave  = {-0.18f, 0.35f, 2.4f, 0.12f, 0.47f, 1.9f, 0.1f, 0.85f, 0.3f},
};
// The shots that strike (fired at FIRE0 + k * FIRE_INTERVAL): the green
// ship first, then twice the yellow one.
typedef struct {
    int  k;
    bool yellow;
} hit_t;
static hit_t const HITS[] = {{5, false}, {13, true}, {17, true}};
#define HIT_N ((int)(sizeof(HITS) / sizeof(HITS[0])))

// Where the hero sits on their tail, in the formation's frame.
#define TAIL_SLOT \
    { 0.2f, 0.9f, -5.5f }
// On the way out it swings wide of the rock: this far at the half-way point.
#define EMERGE_SWING \
    { -2.0f, 0.0f, -9.0f }

// --- The rocks ------------------------------------------------------------------
#define ROCK_RADIUS 6.0f
typedef struct {
    int    shape;
    vec3_t pos;
    float  radius;
    vec3_t axis;
    float  spin;
} rock_t;
static rock_t const ROCKS[] = {
    {0, {0.0f, 0.0f, 0.0f}, ROCK_RADIUS, {0.3f, 1.0f, 0.2f}, 0.05f},
    {1, {-18.0f, 6.0f, -25.0f}, 1.2f, {1.0f, 0.4f, 0.1f}, 0.3f},
    {3, {-6.0f, -9.0f, 30.0f}, 1.5f, {0.6f, 1.0f, 0.5f}, 0.2f},
    {1, {14.0f, 8.0f, -40.0f}, 1.0f, {0.1f, 1.0f, 0.9f}, 0.35f},
};
#define ROCK_N ((int)(sizeof(ROCKS) / sizeof(ROCKS[0])))

// --- The hero ---------------------------------------------------------------------

static vec3_t hide_pos(float t) {
    return v3_add(ARRIVE_PTS[4], v3(0.0f, 0.08f * sinf(1.3f * t), 0.0f));
}

static vec3_t tail_pos(float t) {
    return v3_add(formation_centre(&FORMATION, t), formation_to_world(&FORMATION, (vec3_t)TAIL_SLOT));
}

// The arrival path's time at scene time t: 2x speed at T_HERO_IN,
// braking to rest at T_HIDE; before T_HERO_IN, straight on backwards
// (the warp-in draws that part).
static float arrive_tau(float t) {
    if (t < T_HERO_IN) return 2.0f * (t - T_HERO_IN);
    float const f = fminf((t - T_HERO_IN) / (T_HIDE - T_HERO_IN), 1.0f);
    return ARRIVE_SPAN * (1.0f - (1.0f - f) * (1.0f - f));
}

static vec3_t hero_pos(float t) {
    if (t < T_HIDE) return path_pos(&ARRIVE, arrive_tau(t));
    if (t < T_EMERGE) return hide_pos(t);
    // Out from behind the rock onto their tail: a blend from rest to the
    // moving slot, swung wide of the rock on the way.
    float const  w     = smoothstep(T_EMERGE, T_ON_TAIL, t);
    vec3_t const blend = v3_lerp(hide_pos(t), tail_pos(t), w);
    return v3_add(blend, v3_scale((vec3_t)EMERGE_SWING, sinf(3.1415927f * w)));
}

static vec3_t hero_vel(float t) {
    float const h = 0.02f;
    return v3_scale(v3_sub(hero_pos(t + h), hero_pos(t - h)), 0.5f / h);
}

// Which way the hero faces with no aim: along the arrival path; at rest,
// the way it came to rest; moving off, turning onto its velocity.
static vec3_t hero_heading(float t) {
    vec3_t const rest = v3_norm(path_vel(&ARRIVE, ARRIVE_SPAN));
    if (t < T_HIDE) return v3_norm(path_vel(&ARRIVE, arrive_tau(t)));
    if (t < T_EMERGE) return rest;
    vec3_t const v = hero_vel(t);
    float const  k = clampf(v3_len(v) / 5.0f, 0.0f, 1.0f);
    return v3_norm(v3_lerp(rest, v3_norm(v), k));
}

static xform_t green_base(float t) {
    return formation_pose(&FORMATION, &SLOT_GREEN, t, MARAUDER_SPAN);
}

static xform_t yellow_base(float t) {
    return formation_pose(&FORMATION, &SLOT_YELLOW, t, MARAUDER_SPAN);
}

static float hit_time(int i) {
    return FIRE0 + (float)HITS[i].k * FIRE_INTERVAL;
}

// How far the aim has closed onto the hull for a hit: 1 while the hit's
// beam is lit, falling off before the shot before and after the one
// after (FIRE_INTERVAL away) -- those still pass clear.
static float hit_weight(float t) {
    float w = 0.0f;
    for (int i = 0; i < HIT_N; i++) {
        float const d = (t - hit_time(i) - 0.5f * LASER_STYLE_PLAYER.duration) / HIT_CLOSE;
        w             = fmaxf(w, expf(-d * d));
    }
    return w;
}

// The hero's aim while firing: circling the green ship, then the yellow
// one, MISS off the line of fire -- close misses -- and onto the hull
// for the hits.
static vec3_t hero_aim(float t) {
    float const  k      = smoothstep(T_TO_YELLOW0, T_TO_YELLOW1, t);
    vec3_t const target = v3_lerp(green_base(t).pos, yellow_base(t).pos, k);
    vec3_t const los    = v3_norm(v3_sub(target, hero_pos(t)));
    vec3_t const side   = v3_norm(v3_cross(los, v3(0.0f, 1.0f, 0.0f)));
    vec3_t const up     = v3_cross(side, los);
    float const  a      = 1.3f + 6.2831853f * MISS_TURNS * (t - FIRE0);
    float const  r      = MISS * (1.0f - hit_weight(t));
    return v3_add(target, v3_add(v3_scale(side, r * cosf(a)), v3_scale(up, r * sinf(a))));
}

static xform_t hero_base(float t) {
    vec3_t       fwd = hero_heading(t);
    float const  a   = smoothstep(FIRE0 - AIM_TURN, FIRE0, t) * (1.0f - smoothstep(FIRE1, FIRE1 + AIM_TURN, t));
    vec3_t const pos = hero_pos(t);
    if (a > 0.0f) fwd = v3_norm(v3_lerp(fwd, v3_norm(v3_sub(hero_aim(t), pos)), a));
    // Bank into the turn out from behind the rock.
    float const  h     = 0.05f;
    vec3_t const acc   = v3_scale(v3_sub(hero_vel(t + h), hero_vel(t - h)), 0.5f / h);
    vec3_t const right = v3_norm(v3_cross(v3(0.0f, 1.0f, 0.0f), fwd));
    float const  bank  = t < T_EMERGE ? 0.0f : clampf(-0.06f * v3_dot(acc, right), -0.8f, 0.8f);
    return (xform_t){mat3_from_fwd_up(fwd, v3(0.0f, 1.0f, 0.0f), bank), pos, HERO_SPAN};
}

// A ship warping in at `tw` whose own pose is base(t): the ship once it
// is there, the flash before.
static void submit_warp_in(xform_t (*base)(float), float tw, float t, float size, unsigned seed, bool hero,
                           marauder_livery_t livery) {
    xform_t const b = base(t);
    xform_t       pose;
    if (warp_pose(&b, t, tw, WARP_IN, &pose)) {
        if (hero) {
            player_ship_submit(&pose, 1.0f, t);
        } else {
            marauder_submit(&pose, livery, 1.0f, t, livery == MARAUDER_GREEN ? 1u : 2u);
        }
    }
    xform_t const at_flash = base(tw - WARP_STRETCH_SECS);
    warp_submit_flash(warp_point(&at_flash, WARP_IN), size, t, tw, WARP_IN, seed);
}

// Where a ray from `from` along `dir` first strikes a marauder or a rock,
// no further than the beam's range.
static float beam_reach(vec3_t from, vec3_t dir, float t) {
    float         d = LASER_STYLE_PLAYER.range;
    xform_t const g = green_base(t), y = yellow_base(t);
    marauder_raycast(&g, from, dir, d, &d);
    marauder_raycast(&y, from, dir, d, &d);
    for (int i = 0; i < ROCK_N; i++) {
        rock_t const* r = &ROCKS[i];
        xform_t const x = {mat3_axis_angle(v3_norm(r->axis), r->spin * t), r->pos, r->radius};
        asteroid_raycast(r->shape, &x, from, dir, d, &d);
    }
    return d;
}

// Hit i's spot on its target, in the target's own frame: where the beam
// fired at the ship's middle enters the hull.
static vec3_t hit_spot(int i) {
    float const   th   = hit_time(i);
    xform_t const hero = hero_base(th);
    xform_t const ship = HITS[i].yellow ? yellow_base(th) : green_base(th);
    vec3_t const  gun  = player_ship_gun(&hero, HITS[i].k & 1);
    vec3_t const  dir  = v3_norm(v3_sub(ship.pos, gun));
    float         d    = v3_len(v3_sub(ship.pos, gun));
    marauder_raycast(&ship, gun, dir, d, &d);
    vec3_t const rel = v3_scale(v3_sub(v3_add(gun, v3_scale(dir, d)), ship.pos), 1.0f / ship.scale);
    return v3(v3_dot(rel, ship.r.right), v3_dot(rel, ship.r.up), v3_dot(rel, ship.r.fwd));
}

static int hit_index(int k) {
    for (int i = 0; i < HIT_N; i++) {
        if (HITS[i].k == k) return i;
    }
    return -1;
}

// The shot lit at t, if any: along the nose from alternate pods; a hit
// ends on its spot on the hull, a miss runs on until it leaves the frame
// (or strikes whatever it meets).
static void submit_fire(float t) {
    if (t < FIRE0) return;
    int const   k  = (int)floorf((t - FIRE0) / FIRE_INTERVAL);
    float const tf = FIRE0 + (float)k * FIRE_INTERVAL;
    if (tf > FIRE1 || !laser_lit(t, tf, &LASER_STYLE_PLAYER)) return;
    xform_t const hero = hero_base(t);
    vec3_t const  gun  = player_ship_gun(&hero, k & 1);
    int const     h    = hit_index(k);
    if (h >= 0) {
        xform_t const ship = HITS[h].yellow ? yellow_base(t) : green_base(t);
        laser_submit_beam(gun, xform_apply(&ship, hit_spot(h)), t, tf, &LASER_STYLE_PLAYER);
    } else {
        vec3_t const dir = v3_norm(hero.r.fwd);
        laser_submit_beam(gun, v3_add(gun, v3_scale(dir, beam_reach(gun, dir, t))), t, tf, &LASER_STYLE_PLAYER);
    }
}

// The hits: sparks off the hull, riding with the ship. (No burst: with
// the rocks in view a fireball's textured shells would overrun the list.)
static void submit_hits(float t) {
    for (int i = 0; i < HIT_N; i++) {
        float const th = hit_time(i);
        if (t < th || t > th + IMPACT_SECS) continue;
        xform_t const ship = HITS[i].yellow ? yellow_base(t) : green_base(t);
        vec3_t const  at   = xform_apply(&ship, hit_spot(i));
        impact_submit(at, v3_sub(hero_pos(t), at), 1.2f, t, th, 300u + (unsigned)i);
    }
}

// --- Scene ------------------------------------------------------------------

static void ambush_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    marauder_init();
    asteroid_init();
    explosion_init();  // the hits' sparks
    system2_init();
}

static void ambush_shutdown(void) {
    player_ship_shutdown();
    marauder_shutdown();
    asteroid_shutdown();
    explosion_shutdown();
    system2_shutdown();
}

static void ambush_enter(void) {
    system2_light();
}

static void ambush_camera(double td) {
    float const t = (float)td;
    if (t < SHOT_PASS) {
        // Out on the hiding side of the rock, looking in along the way the
        // hero comes: it drops out of warp in the distance and brakes into
        // cover in front of the rock, the rock between it and the
        // marauders' line beyond.
        camera_look_at(v3(-18.0f, 4.0f, -3.0f), v3(-6.0f, 1.0f, 14.0f), 0.0f);
    } else if (t < SHOT_AMBUSH) {
        // Low beside the rock, a few units off the marauders' line: their
        // flashes far off, then turning to follow them in, past and away.
        camera_look_at(v3(8.2f, -1.5f, -6.0f), formation_centre(&FORMATION, t), 0.0f);
    } else if (t < SHOT_FIRE) {
        // Behind and above the hero, looking ahead along its nose.
        xform_t const hero = hero_base(t);
        vec3_t const  fwd  = v3_norm(hero.r.fwd);
        vec3_t const  eye  = v3_add(v3_add(hero.pos, v3_scale(fwd, -3.2f)), v3(0.0f, 1.0f, 0.0f));
        camera_look_at(eye, v3_add(hero.pos, v3_scale(fwd, 6.0f)), 0.0f);
    } else {
        // Ahead of the pair on the sunlit side (-x), riding with them and
        // looking back past them at the hero on their tail.
        vec3_t const c = formation_centre(&FORMATION, t);
        camera_look_at(v3_add(c, v3(-1.9f, 0.9f, -3.4f)), v3_add(c, v3(0.0f, 0.3f, 2.4f)), 0.0f);
    }
}

static void ambush_submit(double td) {
    float const t = (float)td;
    system2_submit_sky(t);
    for (int i = 0; i < ROCK_N; i++) {
        rock_t const* r = &ROCKS[i];
        xform_t const x = {mat3_axis_angle(v3_norm(r->axis), r->spin * t), r->pos, r->radius};
        asteroid_submit(r->shape, &x);
    }
    submit_warp_in(hero_base, T_HERO_IN, t, 1.6f, 91u, true, MARAUDER_GREEN);
    submit_warp_in(green_base, T_GREEN_IN, t, 1.3f, 92u, false, MARAUDER_GREEN);
    submit_warp_in(yellow_base, T_YELLOW_IN, t, 1.3f, 93u, false, MARAUDER_YELLOW);
    submit_hits(t);
    submit_fire(t);
}

static char const* ambush_shot(double t) {
    return t < SHOT_PASS ? "arrival" : t < SHOT_AMBUSH ? "pass" : t < SHOT_FIRE ? "ambush" : "guns";
}

scene_def_t const SCENE_ASTEROID_AMBUSH = {
    .name     = "asteroid_ambush",
    .duration = SCENE_SECS,
    .init     = ambush_init,
    .shutdown = ambush_shutdown,
    .enter    = ambush_enter,
    .camera   = ambush_camera,
    .submit   = ambush_submit,
    .shot     = ambush_shot,
};
