// =====================================================================
//  Showreel scene  --  asset viewer (development)
// ---------------------------------------------------------------------
//  Not in the playlist: a place to look at each asset generator's output
//  on its own. The camera orbits one asset at a time against the stars,
//  each for ASSET_SECS; every asset is a named shot, so a perf run
//  (`make testrun TEST="perf scene=assets"`) reports what each one costs
//  and a shot test can photograph any of them at a known angle.
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

#define ASSET_SECS   8.0f
#define ORBIT_RATE   0.5f   // rad/s round the asset
#define STATION_SPIN 0.15f  // rad/s about its axis

typedef enum {
    SHOT_PLAYER = 0,
    SHOT_MARAUDER_GREEN,
    SHOT_MARAUDER_YELLOW,
    SHOT_STATION,
    SHOT_COUNT,
} shot_t;

static char const* const SHOT_NAMES[SHOT_COUNT] = {
    [SHOT_PLAYER]          = "player",
    [SHOT_MARAUDER_GREEN]  = "marauder_green",
    [SHOT_MARAUDER_YELLOW] = "marauder_yellow",
    [SHOT_STATION]         = "station",
};

static shot_t shot_at(double t) {
    int const i = (int)(t / ASSET_SECS);
    return (shot_t)(i < 0 ? 0 : (i >= SHOT_COUNT ? SHOT_COUNT - 1 : i));
}

static void viewer_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    marauder_init();
    station_init();
    starfield_init();
}

static void viewer_shutdown(void) {
    player_ship_shutdown();
    marauder_shutdown();
    station_shutdown();
}

static void viewer_enter(void) {
    // A far "sun" up and to the left behind the camera's start position:
    // effectively directional.
    se_light_set(&(se_light_t){.x = -600.0f, .y = 400.0f, .z = -500.0f, .brightness = 0.75f, .two_sided = true});
}

// Orbit the origin at distance d and height h, starting behind-left.
static void orbit(double t, float d, float h) {
    float const  a   = (float)fmod(ORBIT_RATE * t, 2.0 * M_PI) + 2.4f;
    vec3_t const eye = v3(d * sinf(a), h, d * cosf(a));
    camera_look_at(eye, v3(0.0f, 0.0f, 0.0f), 0.0f);
}

static void viewer_camera(double t) {
    shot_t const shot = shot_at(t);
    double const lt   = t - (double)shot * ASSET_SECS;  // time within the shot
    if (shot == SHOT_STATION) {
        orbit(lt * 0.3, 62.0f, 14.0f);
    } else {
        orbit(lt, 1.6f, 0.35f);
    }
}

static void viewer_submit(double t) {
    shot_t const shot = shot_at(t);
    double const lt   = t - (double)shot * ASSET_SECS;  // time within the shot
    xform_t      x    = {mat3_rot_y(0.0f), v3(0.0f, 0.0f, 0.0f), 1.0f};

    switch (shot) {
        case SHOT_PLAYER:
            starfield_submit();
            player_ship_submit(&x, 1.0f, t);
            // Its blue guns, as the marauders' red ones below.
            for (int side = 0; side < 2; side++) {
                float const  tf  = floorf((float)lt) + 0.5f * (float)side;
                vec3_t const gun = player_ship_gun(&x, side);
                laser_submit_beam(gun, v3_add(gun, v3(0.0f, 0.0f, 20.0f)), (float)lt, tf, &LASER_STYLE_PLAYER);
            }
            break;
        case SHOT_MARAUDER_GREEN:
        case SHOT_MARAUDER_YELLOW: {
            starfield_submit();
            marauder_livery_t const l = shot == SHOT_MARAUDER_GREEN ? MARAUDER_GREEN : MARAUDER_YELLOW;
            marauder_submit(&x, l, 1.0f, t, (unsigned)l + 1u);
            // A shot from each gun every second, alternating.
            for (int side = 0; side < 2; side++) {
                float const  tf  = floorf((float)lt) + 0.5f * (float)side;
                vec3_t const gun = marauder_gun(&x, side);
                laser_submit_beam(gun, v3_add(gun, v3(0.0f, 0.0f, 20.0f)), (float)lt, tf, &LASER_STYLE_MARAUDER);
            }
            break;
        }
        case SHOT_STATION:
        default:
            starfield_submit();
            x.r = mat3_rot_z((float)fmod(STATION_SPIN * t, 2.0 * M_PI));
            station_submit(&x);
            break;
    }
}

static char const* viewer_shot(double t) {
    return SHOT_NAMES[shot_at(t)];
}

scene_def_t const SCENE_ASSET_VIEWER = {
    .name     = "assets",
    .duration = ASSET_SECS * SHOT_COUNT,
    .init     = viewer_init,
    .shutdown = viewer_shutdown,
    .enter    = viewer_enter,
    .camera   = viewer_camera,
    .submit   = viewer_submit,
    .shot     = viewer_shot,
};
