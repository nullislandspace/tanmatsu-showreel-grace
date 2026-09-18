// =====================================================================
//  Showreel scene  --  spacestation_flyby
// ---------------------------------------------------------------------
//  The player's ship is chased by two marauders. It threads the gap
//  between two spokes of a turning 2001-style station; the marauders take
//  the longer way round the ring, catch up and open fire (and miss).
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
#include "assets/laser.h"
#include "assets/marauder.h"
#include "assets/player_ship.h"
#include "assets/starfield.h"
#include "assets/station.h"
#include "camera.h"
#include "scenes/scenes.h"
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

// --- Lasers -------------------------------------------------------------
#define FIRE_START    12.0f  // first shot
#define FIRE_INTERVAL 0.35f  // per marauder, guns alternating
#define FIRE_MISS     1.6f   // miss distance, world units

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

static float player_roll(float t) {
    return 6.2831853f * smoothstep(ROLL_START, ROLL_END, t);
}

static xform_t player_pose(float t) {
    return ship_pose(&PLAYER_PATH, t, PLAYER_SPAN, player_roll(t));
}

// --- Camera per shot --------------------------------------------------------

// The three ships' centroid at t.
static vec3_t ships_centroid(float t) {
    vec3_t const sum = v3_add(v3_add(path_pos(&PLAYER_PATH, t), path_pos(&GREEN_PATH, t)), path_pos(&YELLOW_PATH, t));
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
            vec3_t const mid    = v3_lerp(path_pos(&GREEN_PATH, t), path_pos(&YELLOW_PATH, t), 0.5f);
            vec3_t const target = v3_lerp(player, mid, 0.3f);
            camera_look_at(eye, target, 0.0f);
            break;
        }
    }
}

// --- Lasers -----------------------------------------------------------------

// The shot lit at t, if any, from the marauder on `path`: a shot every
// FIRE_INTERVAL from FIRE_START, guns alternating, each a beam from the
// gun's current position at the player, offset by a pseudo-random miss.
static void submit_lasers(path_t const* path, float t, unsigned seed) {
    if (t < FIRE_START) return;
    int const   k  = (int)floorf((t - FIRE_START) / FIRE_INTERVAL);
    float const tf = FIRE_START + (float)k * FIRE_INTERVAL;
    if (!laser_lit(t, tf, &LASER_STYLE_MARAUDER)) return;
    xform_t const pose   = ship_pose(path, t, MARAUDER_SPAN, 0.0f);
    vec3_t const  gun    = marauder_gun(&pose, k & 1);
    vec3_t const  miss   = v3((hash01(k, seed) - 0.5f) * 2.0f, (hash01(k, seed + 1u) - 0.5f) * 2.0f, 0.0f);
    vec3_t const  target = v3_add(path_pos(&PLAYER_PATH, t), v3_scale(v3_norm(miss), FIRE_MISS));
    laser_submit_beam(gun, target, t, tf, &LASER_STYLE_MARAUDER);
}

// --- Scene ------------------------------------------------------------------

static void flyby_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    marauder_init();
    station_init();
    starfield_init();
}

static void flyby_shutdown(void) {
    player_ship_shutdown();
    marauder_shutdown();
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

    xform_t const green  = ship_pose(&GREEN_PATH, t, MARAUDER_SPAN, 0.0f);
    xform_t const yellow = ship_pose(&YELLOW_PATH, t, MARAUDER_SPAN, 0.0f);
    marauder_submit(&green, MARAUDER_GREEN, 1.0f, td, 1u);
    marauder_submit(&yellow, MARAUDER_YELLOW, 1.0f, td, 2u);

    submit_lasers(&GREEN_PATH, t, 101u);
    submit_lasers(&YELLOW_PATH, t, 202u);
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
