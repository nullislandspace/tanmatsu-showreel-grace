// =====================================================================
//  Showreel scene  --  planet_landing (scene 2)
// ---------------------------------------------------------------------
//  The hero ship comes down on the industrial planet (D-28): it flies in
//  nose first over the base, brakes to a hover above the pad, sinks onto
//  it and shuts its engines down. The set is planet_base (the pad, the
//  works, the burning flare stacks, the PPA sky and ground).
//
//  Two shots: a wide establishing view of the base with the ship coming
//  in, and a low one beside the pad for the touchdown.
// =====================================================================

#include <math.h>
#include "assets/planet_base.h"
#include "assets/player_ship.h"
#include "camera.h"
#include "scenes/scenes.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define SCENE_SECS       12.0f
#define T_HOVER          7.0f   // the approach ends in a hover over the pad
#define T_SINK0          7.4f   // the vertical descent ...
#define T_TOUCH          10.4f  // ... ends on the pad
#define T_ENGINE_OFF     11.2f  // flames gone
#define T_TOUCHDOWN_SHOT 6.0f

// --- The ship -------------------------------------------------------------------
// Twice the span it has in space: at 1 unit the ship is a speck against
// works 9-14 units tall (scenecheck: 37 px at touchdown). The pad, 6
// units across, still frames it.
#define SPAN        2.0f
#define HALF_HEIGHT 0.128f  // model mid-height to belly, at span 1 (player_ship.h)
#define HOVER_Y     5.5f

// The approach: nose first along +z, descending and braking to the hover
// point over the pad (a Catmull-Rom path; its last point is the hover).
static vec3_t const APPROACH_PTS[] = {
    {-10.0f, 40.0f, -110.0f}, {-6.0f, 26.0f, -60.0f}, {-2.0f, 13.0f, -22.0f},
    {0.0f, 7.0f, -4.0f},      {0.0f, HOVER_Y, 0.0f},  {0.0f, HOVER_Y, 0.0f},
};
static path_t const APPROACH = {APPROACH_PTS, 6, 0.0f, 1.75f};  // points at 0, 1.75 .. 8.75 s

typedef enum {
    SHOT_ESTABLISH = 0,
    SHOT_TOUCHDOWN,
} shot_t;

static shot_t shot_at(float t) {
    return t < T_TOUCHDOWN_SHOT ? SHOT_ESTABLISH : SHOT_TOUCHDOWN;
}

// Where the ship is at t: on the approach path until the hover, then
// sinking straight down onto the pad.
static vec3_t ship_pos(float t) {
    if (t < T_HOVER) return path_pos(&APPROACH, t);
    float const touch_y = planet_base_pad_centre().y + HALF_HEIGHT * SPAN;
    float const y       = HOVER_Y + (touch_y - HOVER_Y) * smoothstep(T_SINK0, T_TOUCH, t);
    return v3(0.0f, y, 0.0f);
}

// Its pose: heading +z throughout (it lands facing the works), the nose
// dipping while it descends fast and flaring up as it brakes, levelling
// out for the hover; a slight sway before it settles.
static xform_t ship_pose(float t) {
    float pitch = 0.0f, roll = 0.0f;
    if (t < T_HOVER) {
        vec3_t const v      = path_vel(&APPROACH, t);
        float const  h      = 0.05f;
        vec3_t const acc    = v3_scale(v3_sub(path_vel(&APPROACH, t + h), path_vel(&APPROACH, t - h)), 0.5f / h);
        float const  speed  = sqrtf(v.x * v.x + v.z * v.z) + 1.0f;
        // Positive pitch is nose down (xform.h): down with the descent,
        // up (negative) with the braking.
        pitch               = clampf(0.5f * atanf(-v.y / speed) + 0.03f * acc.z, -0.35f, 0.3f);
        roll                = clampf(-0.04f * acc.x, -0.3f, 0.3f);
        // Fade the attitude out into the hover.
        float const settle  = 1.0f - smoothstep(T_HOVER - 1.2f, T_HOVER, t);
        pitch              *= settle;
        roll               *= settle;
    } else if (t < T_TOUCH) {
        float const k = 1.0f - smoothstep(T_SINK0, T_TOUCH, t);
        roll          = 0.04f * k * sinf(3.1f * t);
        pitch         = 0.025f * k * sinf(2.3f * t + 1.0f);
    }
    return (xform_t){mat3_from_ypr(0.0f, pitch, roll), ship_pos(t), SPAN};
}

static float throttle(float t) {
    if (t < T_HOVER - 1.5f) return 0.7f;
    if (t < T_TOUCH) return 1.0f;  // braking, hovering, sinking
    return 1.0f - smoothstep(T_TOUCH, T_ENGINE_OFF, t);
}

static void landing_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    planet_base_init();
}

static void landing_shutdown(void) {
    player_ship_shutdown();
    planet_base_shutdown();
}

static void landing_enter(void) {
    // A low sun off to the left, behind the camera side: the halls and the
    // tanks lit on one face, long in shade on the other.
    se_light_set(&(se_light_t){.x = -500.0f, .y = 260.0f, .z = -300.0f, .brightness = 0.8f, .two_sided = true});
}

static void landing_camera(double td) {
    float const  t    = (float)td;
    vec3_t const ship = ship_pos(t);
    if (shot_at(t) == SHOT_ESTABLISH) {
        // High and wide, south-west of the base: the whole works, the ship
        // coming in over the camera's shoulder. Aimed between the ship and
        // the works, so both stay in the frame.
        vec3_t const eye = v3(-26.0f, 11.0f, -28.0f);
        camera_look_at(eye, v3_lerp(ship, v3(0.0f, 3.0f, 12.0f), 0.3f), 0.0f);
    } else {
        // Low beside the pad, looking up at the ship as it sinks onto it:
        // more sky (painted by the PPA, free) and less ground than looking
        // down at the pad.
        vec3_t const eye = v3(3.4f, 0.7f, -4.2f);
        camera_look_at(eye, v3_add(ship, v3(0.0f, 0.3f, 0.0f)), 0.0f);
    }
}

static void landing_submit(double td) {
    float const t = (float)td;
    planet_base_submit(td);
    xform_t const pose = ship_pose(t);
    player_ship_submit(&pose, throttle(t), td);
}

static char const* landing_shot(double t) {
    return shot_at((float)t) == SHOT_ESTABLISH ? "establish" : "touchdown";
}

static backdrop_t const* landing_backdrop(double t) {
    (void)t;
    return planet_base_backdrop();
}

scene_def_t const SCENE_PLANET_LANDING = {
    .name        = "planet_landing",
    .duration    = SCENE_SECS,
    .init        = landing_init,
    .shutdown    = landing_shutdown,
    .enter       = landing_enter,
    .camera      = landing_camera,
    .submit      = landing_submit,
    .shot        = landing_shot,
    .backdrop_at = landing_backdrop,
};
