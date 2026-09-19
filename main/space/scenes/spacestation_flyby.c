// =====================================================================
//  Showreel scene  --  spacestation_flyby
// ---------------------------------------------------------------------
//  The player's ship is chased by two marauders. It threads the gap
//  between two spokes of a turning 2001-style station; the marauders take
//  the longer way round the ring, catch up and open fire (and miss).
//
//  Past the wheel the marauders leave their paths and close onto the
//  player's tail -- one high on its left, one low on its right -- and
//  turn their noses on it. They only fire straight ahead, so every beam
//  runs along its ship's body; each is aimed just past the player on the
//  shooter's own side and runs on out of the frame -- except three that
//  hit: the shooter's nose swings onto the player for them, the beam ends
//  on its hull, a small burst goes off there and the ship shudders.
//
//  Everything is a pure function of scene time t (scene.h). The ships
//  follow Catmull-Rom paths (xform.h) sampled on one shared time grid,
//  face along their velocity and bank into their turns. The station's
//  starting angle is solved so that a gap is centred exactly where the
//  player crosses the wheel's plane, at exactly the moment it does -- the
//  "daring" part needs no collision logic.
//
//  World: station at the origin, axis along z, wheel in the z = 0 plane.
//  1 unit is about the player's wingspan. The ships fly towards -z.
// =====================================================================

#include <math.h>
#include "camera.h"
#include "space/assets/explosion.h"
#include "space/assets/laser.h"
#include "space/assets/marauder.h"
#include "space/assets/player_ship.h"
#include "space/assets/starfield.h"
#include "space/assets/station.h"
#include "space/space.h"
#include "synthengine3d.h"

#define SCENE_SECS 20.0f

// --- Station ------------------------------------------------------------
#define STATION_SPIN 0.15f  // rad/s about its axis

// --- Paths --------------------------------------------------------------
//
// All three paths share one time grid: point k is reached at k * PATH_DT.
// Point PATH_CROSS_K of the player's path is its crossing of the wheel's
// plane, through the gap at the top of the wheel.
#define PATH_DT      1.7f
#define PATH_N       13
#define PATH_CROSS_K 5
#define T_CROSS      (PATH_CROSS_K * PATH_DT)  // 8.5 s
#define CROSS_R      12.5f                     // radius of the crossing point
#define CROSS_ANGLE  1.5707963f                // straight up (+y)

static vec3_t const PLAYER_PTS[PATH_N] = {
    {-30.0f, 36.0f, 118.0f}, {-22.0f, 30.0f, 96.0f}, {-14.0f, 24.0f, 73.0f}, {-7.0f, 18.5f, 50.0f},
    {-2.0f, 14.5f, 25.0f},   {0.0f, CROSS_R, 0.0f},  {1.0f, 11.5f, -24.0f},  {4.0f, 9.0f, -47.0f},
    {10.0f, 6.0f, -68.0f},   {12.0f, 5.0f, -90.0f},  {8.0f, 7.0f, -112.0f},  {2.0f, 9.0f, -134.0f},
    {-4.0f, 8.0f, -156.0f},
};
// Marauder #1 (green): over the top of the ring, ~7 units clear of its rim.
static vec3_t const GREEN_PTS[PATH_N] = {
    {-26.0f, 40.0f, 136.0f}, {-20.0f, 34.0f, 114.0f}, {-14.0f, 30.0f, 91.0f}, {-10.0f, 30.0f, 68.0f},
    {-8.0f, 32.0f, 44.0f},   {-6.0f, 32.0f, 20.0f},   {-4.0f, 31.0f, -2.0f},  {-2.0f, 24.0f, -26.0f},
    {4.0f, 14.0f, -52.0f},   {10.0f, 7.0f, -80.0f},   {7.0f, 8.5f, -105.0f},  {3.0f, 10.5f, -128.0f},
    {-3.0f, 10.0f, -150.0f},
};
// Marauder #2 (yellow): round the left side of the ring, ~8 units clear.
static vec3_t const YELLOW_PTS[PATH_N] = {
    {-36.0f, 32.0f, 132.0f}, {-32.0f, 28.0f, 110.0f}, {-30.0f, 22.0f, 87.0f}, {-30.0f, 17.0f, 64.0f},
    {-31.0f, 13.0f, 40.0f},  {-31.0f, 11.0f, 16.0f},  {-30.0f, 10.0f, -8.0f}, {-24.0f, 9.0f, -32.0f},
    {-12.0f, 6.0f, -56.0f},  {5.0f, 3.0f, -82.0f},    {9.0f, 5.0f, -106.0f},  {0.0f, 7.5f, -128.0f},
    {-6.0f, 6.5f, -150.0f},
};

static path_t const PLAYER_PATH = {PLAYER_PTS, PATH_N, 0.0f, PATH_DT};
static path_t const GREEN_PATH  = {GREEN_PTS, PATH_N, 0.0f, PATH_DT};
static path_t const YELLOW_PATH = {YELLOW_PTS, PATH_N, 0.0f, PATH_DT};

// --- Ships --------------------------------------------------------------
#define PLAYER_SPAN   1.0f
#define MARAUDER_SPAN 0.9f
// Bank: roll per unit of sideways acceleration (rad per u/s^2), capped.
#define BANK_PER_ACC  0.06f
#define BANK_MAX      0.9f
// The player's barrel roll, once the marauders are on its tail.
#define ROLL_START    15.0f
#define ROLL_END      16.4f

// --- The attack ---------------------------------------------------------
// Each marauder blends from its path onto a track that follows the
// player's own path LAG seconds behind, offset in the player's heading
// frame (no roll). Once there, it turns its nose on its aim point: the
// player plus a miss on the marauder's own side, swinging slowly round.
#define LAG           0.5f   // ~6.6 units behind at the player's speed
#define FIRE_INTERVAL 0.35f  // per marauder, guns alternating
#define MISS_MIN      1.15f  // miss distance range, world units: clear of
#define MISS_MAX      1.5f   // the hull (half-span 0.5) and the flames
#define HIT_SWING     0.35f  // seconds the nose takes to swing onto the player for a hit
#define HIT_BURST     0.12f  // the hit's fireball, in ship spans: a burn on the hull, not the ship going up
#define HIT_SHUDDER   0.5f   // seconds the player shakes after a hit

typedef struct {
    path_t const* path;
    float         right, up;     // the track's offset in the player's heading frame
    float         join0, join1;  // path -> track blend
    float         aim0, aim1;    // nose: velocity -> aim point; fires from aim1
    float         phase;         // the miss's swing
} raider_t;

static raider_t const GREEN  = {&GREEN_PATH, -0.9f, 1.2f, 11.0f, 15.0f, 11.6f, 12.4f, 0.0f};
static raider_t const YELLOW = {&YELLOW_PATH, 1.6f, -0.8f, 11.0f, 15.8f, 13.0f, 13.8f, 2.1f};

// The shots that hit: shooter and shot number (fired at aim1 + k *
// FIRE_INTERVAL). All in the reverse shot, one before the barrel roll
// and two after it.
typedef struct {
    raider_t const* raider;
    int             k;
} hit_t;

static hit_t const HITS[] = {{&GREEN, 6}, {&YELLOW, 9}, {&GREEN, 15}};  // 14.5, 16.95, 17.65 s
#define HIT_N ((int)(sizeof(HITS) / sizeof(HITS[0])))

static float hit_time(hit_t const* h) {
    return h->raider->aim1 + (float)h->k * FIRE_INTERVAL;
}

// 1 while marauder `r` is lined up for one of its hits, easing to 0
// HIT_SWING either side: how much of its miss it drops.
static float hit_weight(raider_t const* r, float t) {
    float w = 0.0f;
    for (int i = 0; i < HIT_N; i++) {
        if (HITS[i].raider != r) continue;
        w = fmaxf(w, 1.0f - smoothstep(0.0f, HIT_SWING, fabsf(t - hit_time(&HITS[i]))));
    }
    return w;
}

// --- Shots --------------------------------------------------------------
typedef enum {
    SHOT_ESTABLISH = 0,  // wide, static: the ships approach the turning station
    SHOT_CHASE,          // behind the player, through the gap
    SHOT_EXIT,           // far side, looking back at the wheel
    SHOT_REVERSE,        // ahead of the player, looking back at the pursuit
    SHOT_COUNT,
} shot_t;

static float const       SHOT_START[SHOT_COUNT] = {0.0f, 5.0f, 9.2f, 12.6f};
static char const* const SHOT_NAMES[SHOT_COUNT] = {"establish", "chase", "exit", "reverse"};

static shot_t shot_at(float t) {
    shot_t s = SHOT_ESTABLISH;
    for (int i = 1; i < SHOT_COUNT; i++) {
        if (t >= SHOT_START[i]) s = (shot_t)i;
    }
    return s;
}

// --- Choreography helpers ---------------------------------------------------

// The station's spin angle at t: phased so that the middle of a gap
// (half a spoke pitch past a spoke) sits at CROSS_ANGLE at T_CROSS.
static float station_angle(float t) {
    float const pitch  = 6.2831853f / (float)STATION_SPOKES;
    float const phase0 = CROSS_ANGLE - 0.5f * pitch - STATION_SPIN * T_CROSS;
    return phase0 + STATION_SPIN * t;
}

// Pose of a ship on `path` at t: facing along its velocity, banking into
// the turn, plus `extra_roll`.
static xform_t ship_pose(path_t const* path, float t, float span, float extra_roll) {
    vec3_t const v     = path_vel(path, t);
    vec3_t const f     = v3_norm(v);
    float const  h     = 0.05f;
    vec3_t const acc   = v3_scale(v3_sub(path_vel(path, t + h), path_vel(path, t - h)), 0.5f / h);
    vec3_t const right = v3_norm(v3_cross(v3(0.0f, 1.0f, 0.0f), f));
    // Turning right (acceleration towards +right) dips the right wing:
    // negative roll in this axis system (xform.h).
    float const  bank  = clampf(-BANK_PER_ACC * v3_dot(acc, right), -BANK_MAX, BANK_MAX);
    return (xform_t){
        .r     = mat3_from_fwd_up(f, v3(0.0f, 1.0f, 0.0f), bank + extra_roll),
        .pos   = path_pos(path, t),
        .scale = span,
    };
}

// A hit rocks the ship: a fast, dying wobble in roll.
static float hit_shudder(float t) {
    float roll = 0.0f;
    for (int i = 0; i < HIT_N; i++) {
        float const dt = t - hit_time(&HITS[i]);
        if (dt >= 0.0f && dt < HIT_SHUDDER) roll += 0.3f * sinf(45.0f * dt) * (1.0f - dt / HIT_SHUDDER);
    }
    return roll;
}

static float player_roll(float t) {
    return 6.2831853f * smoothstep(ROLL_START, ROLL_END, t) + hit_shudder(t);
}

static xform_t player_pose(float t) {
    return ship_pose(&PLAYER_PATH, t, PLAYER_SPAN, player_roll(t));
}

// --- The marauders ------------------------------------------------------------

// The player's heading frame at t: right and up, level (no bank, no roll).
static void heading_frame(float t, vec3_t* right, vec3_t* up) {
    vec3_t const f = v3_norm(path_vel(&PLAYER_PATH, t));
    *right         = v3_norm(v3_cross(v3(0.0f, 1.0f, 0.0f), f));
    *up            = v3_cross(f, *right);
}

static vec3_t raider_pos(raider_t const* r, float t) {
    vec3_t const on_path = path_pos(r->path, t);
    float const  w       = smoothstep(r->join0, r->join1, t);
    if (w <= 0.0f) return on_path;
    vec3_t right, up;
    heading_frame(t - LAG, &right, &up);
    vec3_t const track =
        v3_add(path_pos(&PLAYER_PATH, t - LAG), v3_add(v3_scale(right, r->right), v3_scale(up, r->up)));
    return v3_lerp(on_path, track, w);
}

static vec3_t raider_vel(raider_t const* r, float t) {
    float const h = 0.02f;
    return v3_scale(v3_sub(raider_pos(r, t + h), raider_pos(r, t - h)), 0.5f / h);
}

// Where the marauder points its guns at t: past the player, on the side
// the marauder flies on (so it hardly has to crab), the miss swinging
// +-70 degrees about that side and in and out between MISS_MIN and MISS_MAX.
static vec3_t raider_aim(raider_t const* r, float t) {
    vec3_t const player = path_pos(&PLAYER_PATH, t);
    vec3_t const los    = v3_norm(v3_sub(player, raider_pos(r, t)));
    vec3_t       right, up;
    heading_frame(t, &right, &up);
    vec3_t const off  = v3_add(v3_scale(right, r->right), v3_scale(up, r->up));
    vec3_t const side = v3_norm(v3_sub(off, v3_scale(los, v3_dot(off, los))));
    mat3_t const turn = mat3_axis_angle(los, 1.2f * sinf(1.3f * t + r->phase));
    float const  miss = (MISS_MIN + (MISS_MAX - MISS_MIN) * (0.5f + 0.5f * sinf(2.3f * t + 2.0f * r->phase))) *
                       (1.0f - hit_weight(r, t));
    return v3_add(player, v3_scale(mat3_apply(&turn, side), miss));
}

// Pose: facing along the velocity, turning onto the aim point from aim0
// to aim1; banking into the turn as ship_pose does.
static xform_t raider_pose(raider_t const* r, float t) {
    vec3_t const pos = raider_pos(r, t);
    vec3_t const v   = raider_vel(r, t);
    float const  a   = smoothstep(r->aim0, r->aim1, t);
    vec3_t const f   = a > 0.0f ? v3_norm(v3_lerp(v3_norm(v), v3_norm(v3_sub(raider_aim(r, t), pos)), a)) : v3_norm(v);
    float const  h   = 0.05f;
    vec3_t const acc = v3_scale(v3_sub(raider_vel(r, t + h), raider_vel(r, t - h)), 0.5f / h);
    vec3_t const right = v3_norm(v3_cross(v3(0.0f, 1.0f, 0.0f), f));
    float const  bank  = clampf(-BANK_PER_ACC * v3_dot(acc, right), -BANK_MAX, BANK_MAX);
    return (xform_t){.r = mat3_from_fwd_up(f, v3(0.0f, 1.0f, 0.0f), bank), .pos = pos, .scale = MARAUDER_SPAN};
}

// --- Camera per shot --------------------------------------------------------

// The three ships' centroid at t.
static vec3_t ships_centroid(float t) {
    vec3_t const sum = v3_add(v3_add(path_pos(&PLAYER_PATH, t), raider_pos(&GREEN, t)), raider_pos(&YELLOW, t));
    return v3_scale(sum, 1.0f / 3.0f);
}

// The static cameras' eye points were checked against a host-side replica
// of these paths: every ship on screen for the whole shot (and, while
// establishing, the whole wheel), as close as that allows.
static void set_camera(shot_t shot, float t) {
    vec3_t const player = path_pos(&PLAYER_PATH, t);
    switch (shot) {
        case SHOT_ESTABLISH: {
            // Fixed, behind and above the ships as they fly away from it
            // towards the turning wheel; aimed half way between the ships
            // and the station.
            vec3_t const eye    = v3(-25.0f, 40.0f, 150.0f);
            vec3_t const target = v3_lerp(ships_centroid(t), v3(0.0f, 4.0f, 0.0f), 0.5f);
            camera_look_at(eye, target, 0.0f);
            break;
        }
        case SHOT_CHASE: {
            // On the player's own path, a moment behind and a little above
            // it: the camera threads the same gap, so the spokes sweep
            // right past the lens.
            vec3_t const eye   = v3_add(path_pos(&PLAYER_PATH, t - 0.25f), v3(0.0f, 0.9f, 0.0f));
            vec3_t const ahead = v3_add(player, v3_scale(v3_norm(path_vel(&PLAYER_PATH, t)), 6.0f));
            camera_look_at(eye, ahead, 0.0f);
            break;
        }
        case SHOT_EXIT: {
            // Fixed beyond the wheel, looking back: the player bursts through
            // the spokes, the marauders swing round the rim.
            // The whole wheel in the middle of the frame, the player coming
            // straight out of the gap towards the lens, the marauders over
            // the top and round the left of the ring. Short: the ships fly
            // towards this camera and pass it at ~13 s.
            vec3_t const eye = v3(14.0f, 20.0f, -80.0f);
            camera_look_at(eye, v3_lerp(v3(0.0f, 8.0f, 0.0f), ships_centroid(t), 0.5f), 0.0f);
            break;
        }
        case SHOT_REVERSE:
        default: {
            // Ahead of the player, off to its right, looking back at it and
            // at the pursuit behind it.
            vec3_t const eye    = v3_add(path_pos(&PLAYER_PATH, t + 0.28f), v3(1.3f, 0.7f, 0.0f));
            vec3_t const mid    = v3_lerp(raider_pos(&GREEN, t), raider_pos(&YELLOW, t), 0.5f);
            vec3_t const target = v3_lerp(player, mid, 0.3f);
            camera_look_at(eye, target, 0.0f);
            break;
        }
    }
}

// --- Lasers -----------------------------------------------------------------

static int hit_index(raider_t const* r, int k) {
    for (int i = 0; i < HIT_N; i++) {
        if (HITS[i].raider == r && HITS[i].k == k) return i;
    }
    return -1;
}

// Where hit i strikes the player, in the player's own frame: where the
// shot, straight along the gun's line at the moment it fires, meets the
// hull (or, should that line just graze past, the beam aimed at the
// ship's middle). Kept in the ship's frame, the spot rolls and shudders
// with it.
static vec3_t hit_spot(int i) {
    float const   th     = hit_time(&HITS[i]);
    xform_t const pose   = raider_pose(HITS[i].raider, th);
    xform_t const player = player_pose(th);
    vec3_t const  gun    = marauder_gun(&pose, HITS[i].k & 1);
    vec3_t        dir    = v3_norm(mat3_apply(&pose.r, v3(0.0f, 0.0f, 1.0f)));
    float         d      = LASER_STYLE_MARAUDER.range;
    if (!player_ship_raycast(&player, gun, dir, d, &d)) {
        dir = v3_norm(v3_sub(player.pos, gun));
        d   = v3_len(v3_sub(player.pos, gun));
        player_ship_raycast(&player, gun, dir, d, &d);
    }
    vec3_t const rel = v3_sub(v3_add(gun, v3_scale(dir, d)), player.pos);
    return v3(v3_dot(rel, player.r.right), v3_dot(rel, player.r.up), v3_dot(rel, player.r.fwd));
}

// Hit i's spot on the player at t, in the world.
static vec3_t hit_at(int i, float t) {
    xform_t const now = player_pose(t);
    return v3_add(now.pos, mat3_apply(&now.r, hit_spot(i)));
}

// The shot lit at t, if any, from marauder `r`: a shot every
// FIRE_INTERVAL once its nose is on the aim point (aim1), guns
// alternating; each fires straight ahead along the ship's body.
static void submit_lasers(raider_t const* r, float t) {
    if (t < r->aim1) return;
    int const   k  = (int)floorf((t - r->aim1) / FIRE_INTERVAL);
    float const tf = r->aim1 + (float)k * FIRE_INTERVAL;
    if (!laser_lit(t, tf, &LASER_STYLE_MARAUDER)) return;
    xform_t const pose = raider_pose(r, t);
    vec3_t const  fwd  = mat3_apply(&pose.r, v3(0.0f, 0.0f, 1.0f));
    vec3_t const  gun  = marauder_gun(&pose, k & 1);
    int const     h    = hit_index(r, k);
    if (h >= 0) {
        // Straight ahead onto the player: the beam stops at its hull.
        laser_submit_beam(gun, hit_at(h, t), t, tf, &LASER_STYLE_MARAUDER);
    } else {
        // A miss: on out of the frame (or onto the hull, should it meet it).
        xform_t const player = player_pose(t);
        float         d      = LASER_STYLE_MARAUDER.range;
        player_ship_raycast(&player, gun, fwd, d, &d);
        laser_submit_beam(gun, v3_add(gun, v3_scale(fwd, d)), t, tf, &LASER_STYLE_MARAUDER);
    }
}

// The hits' bursts, riding on the player where the beam struck.
static void submit_hits(float t) {
    for (int i = 0; i < HIT_N; i++) {
        float const th = hit_time(&HITS[i]);
        if (t < th || t > th + EXPLOSION_SECS) continue;
        vec3_t const  at      = hit_at(i, t);
        xform_t const shooter = raider_pose(HITS[i].raider, th);
        vec3_t const  normal  = v3_scale(mat3_apply(&shooter.r, v3(0.0f, 0.0f, 1.0f)), -1.0f);
        impact_submit(at, normal, 1.2f, t, th, 500u + (unsigned)i);
        explosion_submit(at, v3(0.0f, 0.0f, 0.0f), HIT_BURST * PLAYER_SPAN, t, th, 600u + (unsigned)i);
    }
}

// --- Scene ------------------------------------------------------------------

static void flyby_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    marauder_init();
    explosion_init();
    station_init();
    starfield_init();
}

static void flyby_shutdown(void) {
    player_ship_shutdown();
    marauder_shutdown();
    explosion_shutdown();
    station_shutdown();
}

static void flyby_enter(void) {
    // A far "sun" up and to the left, a little towards -z: side light for
    // every shot, whether it looks along the flight (-z) or back at it.
    // (From the +z side, the shots looking back saw only unlit faces.)
    se_light_set(&(se_light_t){.x = -500.0f, .y = 350.0f, .z = -60.0f, .brightness = 0.8f, .two_sided = true});
}

static void flyby_camera(double td) {
    float const t = (float)td;
    set_camera(shot_at(t), t);
}

static void flyby_submit(double td) {
    float const t = (float)td;
    starfield_submit();

    xform_t const station = {mat3_rot_z(station_angle(t)), v3(0.0f, 0.0f, 0.0f), 1.0f};
    station_submit(&station);

    xform_t const player = player_pose(t);
    player_ship_submit(&player, 1.0f, td);

    xform_t const green  = raider_pose(&GREEN, t);
    xform_t const yellow = raider_pose(&YELLOW, t);
    marauder_submit(&green, MARAUDER_GREEN, 1.0f, td, 1u);
    marauder_submit(&yellow, MARAUDER_YELLOW, 1.0f, td, 2u);

    submit_lasers(&GREEN, t);
    submit_lasers(&YELLOW, t);
    submit_hits(t);
}

static char const* flyby_shot(double t) {
    return SHOT_NAMES[shot_at((float)t)];
}

scene_def_t const SCENE_SPACESTATION_FLYBY = {
    .name     = "spacestation_flyby",
    .duration = SCENE_SECS,
    .init     = flyby_init,
    .shutdown = flyby_shutdown,
    .enter    = flyby_enter,
    .camera   = flyby_camera,
    .submit   = flyby_submit,
    .shot     = flyby_shot,
};
