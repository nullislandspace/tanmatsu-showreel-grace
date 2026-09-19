// =====================================================================
//  Showreel scene  --  marauder_downfall (scene 10)
// ---------------------------------------------------------------------
//  The second system, right after the ambush (D-28). Close beside the
//  marauders' formation, like the pursuit: blue beams come in from the
//  hero ship behind them, out of frame. Most miss, close; three hit the
//  yellow ship, each throwing sparks off its hull and leaving a burn,
//  and then it blows apart -- its parts tumbling away through a fireball
//  (explosion.h, marauder_submit_debris). The green one flies on alone
//  under more fire for a couple of seconds; the camera swings round
//  behind it and watches it warp away.
//
//  One shot, riding with the formation off the green ship's right; after
//  the blast it follows the green ship. The guns stop before the swing:
//  from behind, a beam that misses would shrink to a point mid-screen.
// =====================================================================

#include <math.h>
#include "camera.h"
#include "space/assets/explosion.h"
#include "space/assets/laser.h"
#include "space/assets/marauder.h"
#include "space/assets/warp.h"
#include "space/scenes/formation.h"
#include "space/scenes/system2.h"
#include "space/space.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define SCENE_SECS    8.8f
#define FIRE0         0.6f  // the hero's guns: first salvo, at both ...
#define FIRE1         3.4f
#define FIRE2         4.4f  // ... then at the green one alone
#define FIRE3         5.2f
#define FIRE_INTERVAL 0.2f
#define T_BOOM        3.5f  // the yellow ship blows up
#define T_GREEN_WARP  6.4f
#define T_SWING0      5.3f  // the camera swings round behind the green ship
#define T_SWING1      6.2f

// --- Ships ------------------------------------------------------------------
#define MARAUDER_SPAN 0.9f
#define BURN_SIZE     0.12f  // a hit's burn, in ship spans
#define BOOM_SIZE     0.8f   // the fireball, in ship spans (close to the lens)

static formation_t const FORMATION = {
    .origin = {0.0f, 0.0f, 0.0f}, .vel = {0.0f, 0.0f, -14.0f}, .bank_per_acc = 0.06f, .bank_max = 0.6f};
// The pursuit's pair, mirrored (sways too): close, the yellow one on the left and a
// little back, so the camera riding off the green one's right is on the
// sunlit side (system2.h: the sun is at -x, the formation's right).
static slot_t const SLOT_GREEN = {
    .offset = {0.75f, 0.1f, 0.0f},
    .weave  = {0.22f, 0.31f, 0.0f, 0.12f, 0.43f, 1.1f, 0.10f, 0.9f, 0.4f},
};
static slot_t const SLOT_YELLOW = {
    .offset = {-0.75f, -0.1f, -0.6f},
    .weave  = {0.18f, 0.37f, 2.0f, 0.10f, 0.51f, 2.6f, 0.12f, 0.8f, 1.9f},
};

// The hero, out of frame behind them, in the formation's frame.
#define HERO_SLOT \
    { 0.3f, 0.9f, -16.0f }

// The shots that hit the yellow ship, and where they are aimed (its model
// space); each strikes the hull where the beam towards that point first
// meets it.
typedef struct {
    int    k;  // shot number: fired at FIRE0 + k * FIRE_INTERVAL
    vec3_t at;
} hit_t;
static hit_t const HITS[] = {
    {5, {0.3f, 0.02f, -0.1f}},    // 1.6 s: the right wing (towards the camera)
    {9, {-0.1f, 0.06f, -0.45f}},  // 2.4 s: the tail
    {12, {0.0f, 0.08f, 0.1f}},    // 3.0 s: the spine
};
#define HIT_N ((int)(sizeof(HITS) / sizeof(HITS[0])))

// --- Camera -------------------------------------------------------------------
// Off the green ship's right, a little behind and above, looking across
// both: the yellow one blows up at a safe distance from the lens.
#define CAM_OFFSET \
    { 1.6f, 0.45f, -1.3f }
// ... and after the swing: behind it, a little right and above, looking
// along its flight so the warp's streak and flash are in view.
#define CAM_CHASE \
    { 0.7f, 0.6f, -3.2f }
#define CHASE_LEAD 6.0f  // units ahead of the green ship the camera looks, after the swing

// The green ship's climb away from the blast, in the formation's frame.
static vec3_t green_climb(float t) {
    float const k = smoothstep(T_BOOM - 0.1f, T_BOOM + 1.2f, t);
    return v3(0.3f * k, 0.8f * k, 0.5f * k);
}

// The green ship, climbing away from the blast once its wingman goes.
static xform_t green_pose(float t) {
    xform_t g = formation_pose(&FORMATION, &SLOT_GREEN, t, MARAUDER_SPAN);
    g.pos     = v3_add(g.pos, formation_to_world(&FORMATION, green_climb(t)));
    return g;
}

static xform_t yellow_pose(float t) {
    return formation_pose(&FORMATION, &SLOT_YELLOW, t, MARAUDER_SPAN);
}

static vec3_t hero_pos(float t) {
    vec3_t const sway = v3(0.3f * sinf(0.8f * t), 0.2f * sinf(1.1f * t + 0.4f), 0.0f);
    return v3_add(formation_centre(&FORMATION, t), formation_to_world(&FORMATION, v3_add((vec3_t)HERO_SLOT, sway)));
}

static int hit_index(int k) {
    for (int i = 0; i < HIT_N; i++) {
        if (HITS[i].k == k) return i;
    }
    return -1;
}

// Where miss k is aimed at t: beside its target (the ships in turn, the
// green one alone after the blast), 0.9 .. 1.3 off it in a seeded
// direction across the line of fire.
static vec3_t miss_aim(int k, float t, vec3_t from) {
    bool const   at_yellow = t < T_BOOM && (k & 1);
    vec3_t const target    = (at_yellow ? yellow_pose(t) : green_pose(t)).pos;
    vec3_t const los       = v3_norm(v3_sub(target, from));
    vec3_t const side      = v3_norm(v3_cross(los, v3(0.0f, 1.0f, 0.0f)));
    vec3_t const up        = v3_cross(side, los);
    float const  a         = 6.2831853f * hash01(k, 0xD0F1u);
    float const  r         = 0.9f + 0.4f * hash01(k, 0xD0F2u);
    return v3_add(target, v3_add(v3_scale(side, r * cosf(a)), v3_scale(up, r * sinf(a))));
}

static float hit_time(int i) {
    return FIRE0 + (float)HITS[i].k * FIRE_INTERVAL;
}

// The hero's gun `side` at t.
static vec3_t hero_gun(float t, int side) {
    return v3_add(hero_pos(t), v3(side ? 0.3f : -0.3f, -0.05f, -0.9f));
}

// Hit i's spot on the yellow ship, in its own frame: where the beam from
// the gun towards the aimed point first meets the hull.
static vec3_t hit_spot(int i) {
    float const   th  = hit_time(i);
    xform_t const y   = yellow_pose(th);
    vec3_t const  gun = hero_gun(th, HITS[i].k & 1);
    vec3_t const  aim = xform_apply(&y, HITS[i].at);
    vec3_t const  dir = v3_norm(v3_sub(aim, gun));
    float         d   = v3_len(v3_sub(aim, gun));
    marauder_raycast(&y, gun, dir, d, &d);
    vec3_t const rel = v3_scale(v3_sub(v3_add(gun, v3_scale(dir, d)), y.pos), 1.0f / y.scale);
    return v3(v3_dot(rel, y.r.right), v3_dot(rel, y.r.up), v3_dot(rel, y.r.fwd));
}

static void submit_fire(float t) {
    for (int salvo = 0; salvo < 2; salvo++) {
        float const f0 = salvo ? FIRE2 : FIRE0, f1 = salvo ? FIRE3 : FIRE1;
        if (t < f0) continue;
        int const   k  = (int)floorf((t - f0) / FIRE_INTERVAL) + (salvo ? 100 : 0);
        float const tf = f0 + (float)(k % 100) * FIRE_INTERVAL;
        if (tf > f1 || !laser_lit(t, tf, &LASER_STYLE_PLAYER)) continue;
        // The hero's pods, a little ahead of its centre (it flies -z); its
        // nose is on the aim, so the beam runs from the pod towards it.
        vec3_t const gun = hero_gun(t, k & 1);
        int const    h   = salvo ? -1 : hit_index(k);
        if (h >= 0) {
            xform_t const y = yellow_pose(t);
            laser_submit_beam(gun, xform_apply(&y, hit_spot(h)), t, tf, &LASER_STYLE_PLAYER);
        } else {
            laser_submit_ray(gun, v3_norm(v3_sub(miss_aim(k, t, gun), gun)), t, tf, &LASER_STYLE_PLAYER);
        }
    }
}

// The hits on the yellow ship: sparks, then a burn riding on its hull,
// until the whole ship goes.
static void submit_hits(float t) {
    for (int i = 0; i < HIT_N; i++) {
        float const th = hit_time(i);
        if (t < th || t >= T_BOOM) continue;
        xform_t const y  = yellow_pose(t);
        vec3_t const  at = xform_apply(&y, hit_spot(i));
        impact_submit(at, v3_sub(at, hero_pos(t)), 0.9f, t, th, 40u + (unsigned)i);
        explosion_submit(at, v3(0.0f, 0.0f, 0.0f), BURN_SIZE * MARAUDER_SPAN, t, th, 50u + (unsigned)i);
    }
}

// --- Scene ------------------------------------------------------------------

static void downfall_init(char const* asset_dir) {
    (void)asset_dir;
    marauder_init();
    explosion_init();
    system2_init();
}

static void downfall_shutdown(void) {
    marauder_shutdown();
    explosion_shutdown();
    system2_shutdown();
}

static void downfall_enter(void) {
    system2_light();
}

static void downfall_camera(double td) {
    float const  t     = (float)td;
    // Riding with the green ship's slot, climbing with it after the blast.
    vec3_t const climb = formation_to_world(&FORMATION, green_climb(t));
    vec3_t const green = v3_add(formation_slot_pos(&FORMATION, &SLOT_GREEN, t), climb);
    vec3_t const yel   = formation_slot_pos(&FORMATION, &SLOT_YELLOW, t);
    vec3_t const bob   = v3(0.04f * sinf(0.7f * t), 0.04f * sinf(0.9f * t + 1.0f), 0.0f);
    // Off its right; then round behind it.
    float const  s     = smoothstep(T_SWING0, T_SWING1, t);
    vec3_t const off   = v3_lerp((vec3_t)CAM_OFFSET, (vec3_t)CAM_CHASE, s);
    vec3_t const eye   = v3_add(v3_add(green, formation_to_world(&FORMATION, off)), bob);
    // Across at both; after the blast, onto the green one; after the
    // swing, ahead along its flight.
    float const  k     = smoothstep(T_BOOM, T_BOOM + 1.0f, t);
    vec3_t const look  = v3_lerp(v3_lerp(green, yel, 0.6f), green, k);
    camera_look_at(eye, v3_add(look, formation_to_world(&FORMATION, v3(0.0f, 0.0f, CHASE_LEAD * s))), 0.0f);
}

static void downfall_submit(double td) {
    float const t = (float)td;
    system2_submit_sky(t);

    xform_t const g = green_pose(t);
    xform_t       pose;
    if (warp_pose(&g, t, T_GREEN_WARP, WARP_OUT, &pose)) marauder_submit(&pose, MARAUDER_GREEN, 1.0f, td, 1u);
    xform_t const g_flash = green_pose(T_GREEN_WARP + WARP_STRETCH_SECS);
    warp_submit_flash(warp_point(&g_flash, WARP_OUT), 1.3f, t, T_GREEN_WARP, WARP_OUT, 61u);

    if (t < T_BOOM) {
        xform_t const y = yellow_pose(t);
        marauder_submit(&y, MARAUDER_YELLOW, 1.0f, td, 2u);
    } else {
        // The wreck carries on at the formation's speed, flying apart.
        xform_t const y = yellow_pose(T_BOOM);
        marauder_submit_debris(&y, FORMATION.vel, MARAUDER_YELLOW, t, T_BOOM, 7u);
        explosion_submit(y.pos, FORMATION.vel, BOOM_SIZE * MARAUDER_SPAN, t, T_BOOM, 8u);
    }
    submit_hits(t);
    submit_fire(t);
}

static char const* downfall_shot(double t) {
    return t < T_BOOM ? "hits" : t < T_SWING0 ? "blast" : "warp";
}

scene_def_t const SCENE_MARAUDER_DOWNFALL = {
    .name     = "marauder_downfall",
    .duration = SCENE_SECS,
    .init     = downfall_init,
    .shutdown = downfall_shutdown,
    .enter    = downfall_enter,
    .camera   = downfall_camera,
    .submit   = downfall_submit,
    .shot     = downfall_shot,
};
