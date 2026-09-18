// =====================================================================
//  Showreel scene  --  marauder_pursuit
// ---------------------------------------------------------------------
//  The opening: the two marauders in formation, seen from close behind
//  and just right of the right one, weaving a little and firing ahead at
//  a target out of frame -- the player's ship. It is there to establish
//  that they are not going to give up before spacestation_flyby shows
//  the chase itself.
//
//  A pure function of scene time t, like every scene (scene.h). The
//  formation flies straight along -z; each ship adds its own gentle
//  weave (sideways and up/down) and banks into it, and the camera rides
//  along with the formation.
// =====================================================================

#include <math.h>
#include "assets/laser.h"
#include "assets/marauder.h"
#include "assets/starfield.h"
#include "camera.h"
#include "scenes/scenes.h"
#include "synthengine3d.h"

#define SCENE_SECS 6.0f

// --- Formation ------------------------------------------------------------
#define SPEED         14.0f  // units/s along -z, as in the flyby
#define MARAUDER_SPAN 0.9f
// Formation slots, relative to the formation's centre. The camera looks
// along -z, so screen-right is world -x: the "right" marauder (yellow)
// flies at negative x, half a length behind the green one.
#define SLOT_X        0.75f
#define SLOT_Y        0.1f
#define SLOT_BACK     0.6f

// Weave: sideways and vertical sways plus a rocking roll, each ship with
// its own frequencies and phases so they never move in step.
typedef struct {
    float lat_amp, lat_hz, lat_ph;  // sideways sway
    float vert_amp, vert_hz, vert_ph;
    float rock_amp, rock_hz, rock_ph;  // extra roll on top of the bank
} weave_t;

static weave_t const WEAVE_GREEN  = {0.22f, 0.31f, 0.0f, 0.12f, 0.43f, 1.1f, 0.10f, 0.9f, 0.4f};
static weave_t const WEAVE_YELLOW = {0.18f, 0.37f, 2.0f, 0.10f, 0.51f, 2.6f, 0.12f, 0.8f, 1.9f};

// Bank per unit of sideways acceleration (as in the flyby), capped.
#define BANK_PER_ACC 0.06f
#define BANK_MAX     0.6f

// --- Guns ------------------------------------------------------------------
#define FIRE_INTERVAL 0.2f   // per ship, guns alternating
#define TARGET_AHEAD  60.0f  // the (unseen) player, this far ahead
#define TARGET_MISS   1.5f   // spread of the aim around it

// --- Camera -----------------------------------------------------------------
// Close behind and right of the right marauder, a little above it, and
// looking diagonally across at the pair: ~50 degrees off the flight
// direction, more than half the horizontal field of view (~42 degrees),
// so the beams, aimed far ahead, run off the edge of the screen instead
// of converging in it. From ~1.5 units the right ship is up to ~370 px
// wide. Keep the camera this far out: the engine's near plane is
// RENDER_NEAR_CLIP_Z = 0.5 in camera-space depth, and 20% closer the
// right ship's wingtip crossed it (clipped away at t ~ 1.5 and 3.3-4.4 s).
// Now its nearest point stays >= 0.63 deep.
#define CAM_RIGHT 0.90f  // beyond the right ship, towards screen-right (-x)
#define CAM_UP    0.42f
#define CAM_BACK  1.08f
#define CAM_BOB   0.04f  // a slight drift, so the ride does not look locked

static float const TWO_PI = 6.2831853f;

static vec3_t formation_centre(float t) {
    return v3(0.0f, 0.0f, -SPEED * t);
}

// Position, velocity and acceleration of a weaving ship in slot `slot`.
static void ship_motion(vec3_t slot, weave_t const* w, float t, vec3_t* p, vec3_t* v, vec3_t* a) {
    float const lw = TWO_PI * w->lat_hz, vw = TWO_PI * w->vert_hz;
    float const lx = w->lat_amp * sinf(lw * t + w->lat_ph);
    float const vy = w->vert_amp * sinf(vw * t + w->vert_ph);
    *p             = v3_add(v3_add(formation_centre(t), slot), v3(lx, vy, 0.0f));
    *v = v3(w->lat_amp * lw * cosf(lw * t + w->lat_ph), w->vert_amp * vw * cosf(vw * t + w->vert_ph), -SPEED);
    *a = v3(-w->lat_amp * lw * lw * sinf(lw * t + w->lat_ph), -w->vert_amp * vw * vw * sinf(vw * t + w->vert_ph), 0.0f);
}

static xform_t ship_pose(vec3_t slot, weave_t const* w, float t) {
    vec3_t p, v, a;
    ship_motion(slot, w, t, &p, &v, &a);
    vec3_t const f     = v3_norm(v);
    vec3_t const right = v3_norm(v3_cross(v3(0.0f, 1.0f, 0.0f), f));
    // Turning right dips the right wing: negative roll (xform.h).
    float const  bank  = clampf(-BANK_PER_ACC * v3_dot(a, right), -BANK_MAX, BANK_MAX);
    float const  rock  = w->rock_amp * sinf(TWO_PI * w->rock_hz * t + w->rock_ph);
    return (xform_t){mat3_from_fwd_up(f, v3(0.0f, 1.0f, 0.0f), bank + rock), p, MARAUDER_SPAN};
}

static vec3_t const SLOT_GREEN  = {SLOT_X, SLOT_Y, 0.0f};
static vec3_t const SLOT_YELLOW = {-SLOT_X, -SLOT_Y, SLOT_BACK};

// The shot lit at t from one ship, if any: a shot every FIRE_INTERVAL
// (guns alternating), each a beam from the gun's current position at the
// unseen target ahead, with a small pseudo-random miss. `offset` staggers
// the two ships' salvos.
static void submit_lasers(vec3_t slot, weave_t const* w, float t, float offset, unsigned seed) {
    if (t < offset) return;
    int const   k  = (int)floorf((t - offset) / FIRE_INTERVAL);
    float const tf = offset + (float)k * FIRE_INTERVAL;
    if (!laser_lit(t, tf, &LASER_STYLE_MARAUDER)) return;
    xform_t const pose = ship_pose(slot, w, t);
    vec3_t const  gun  = marauder_gun(&pose, k & 1);
    vec3_t const  miss =
        v3((hash01(k, seed) - 0.5f) * 2.0f * TARGET_MISS, (hash01(k, seed + 1u) - 0.5f) * 2.0f * TARGET_MISS, 0.0f);
    vec3_t const target = v3_add(v3_add(formation_centre(t), v3(0.0f, 0.3f, -TARGET_AHEAD)), miss);
    laser_submit_beam(gun, target, t, tf, &LASER_STYLE_MARAUDER);
}

static void pursuit_init(char const* asset_dir) {
    (void)asset_dir;
    marauder_init();
    starfield_init();
}

static void pursuit_shutdown(void) {
    marauder_shutdown();
}

static void pursuit_enter(void) {
    // The flyby's sun: up and to the left, side-lighting the formation.
    se_light_set(&(se_light_t){.x = -500.0f, .y = 350.0f, .z = -60.0f, .brightness = 0.8f, .two_sided = true});
}

// Riding with the formation (the right ship's slot, not its weave, so
// the ships move in the frame), aimed between the two ships, with a slow
// drift.
static void pursuit_camera(double td) {
    float const  t      = (float)td;
    vec3_t const centre = formation_centre(t);
    vec3_t const bob    = v3(CAM_BOB * sinf(0.7f * t), CAM_BOB * sinf(0.9f * t + 1.0f), 0.0f);
    vec3_t const eye    = v3_add(v3_add(v3_add(centre, SLOT_YELLOW), v3(-CAM_RIGHT, CAM_UP, CAM_BACK)), bob);
    vec3_t const mid    = v3_scale(v3_add(v3_add(centre, SLOT_GREEN), v3_add(centre, SLOT_YELLOW)), 0.5f);
    camera_look_at(eye, mid, 0.0f);
}

static void pursuit_submit(double td) {
    float const t = (float)td;
    starfield_submit();

    xform_t const green  = ship_pose(SLOT_GREEN, &WEAVE_GREEN, t);
    xform_t const yellow = ship_pose(SLOT_YELLOW, &WEAVE_YELLOW, t);
    marauder_submit(&green, MARAUDER_GREEN, 1.0f, td, 1u);
    marauder_submit(&yellow, MARAUDER_YELLOW, 1.0f, td, 2u);

    submit_lasers(SLOT_GREEN, &WEAVE_GREEN, t, 0.4f, 11u);
    submit_lasers(SLOT_YELLOW, &WEAVE_YELLOW, t, 0.5f, 22u);
}

scene_def_t const SCENE_MARAUDER_PURSUIT = {
    .name     = "marauder_pursuit",
    .duration = SCENE_SECS,
    .init     = pursuit_init,
    .shutdown = pursuit_shutdown,
    .enter    = pursuit_enter,
    .camera   = pursuit_camera,
    .submit   = pursuit_submit,
};
