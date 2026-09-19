// =====================================================================
//  Showreel scene  --  asset viewer (development)
// ---------------------------------------------------------------------
//  Not in the playlist: a place to look at each asset generator's output
//  on its own. One asset (or effect) at a time, each for ASSET_SECS;
//  every one is a named shot, so a perf run (`make testrun TEST="perf
//  scene=assets"`) reports what each costs and a shot test can
//  photograph any of them at a known moment.
// =====================================================================

#include <math.h>
#include "camera.h"
#include "common/starfield.h"
#include "space/assets/asteroid.h"
#include "space/assets/explosion.h"
#include "space/assets/laser.h"
#include "space/assets/marauder.h"
#include "space/assets/planet.h"
#include "space/assets/planet_base.h"
#include "space/assets/player_ship.h"
#include "space/assets/space_dust.h"
#include "space/assets/station.h"
#include "space/assets/title_text.h"
#include "space/assets/title_text_mesh.h"
#include "space/assets/warp.h"
#include "space/space.h"
#include "synthengine3d.h"

#define ASSET_SECS   8.0f
#define ORBIT_RATE   0.5f   // rad/s round the asset
#define STATION_SPIN 0.15f  // rad/s about its axis

typedef enum {
    SHOT_PLAYER = 0,
    SHOT_MARAUDER_GREEN,
    SHOT_MARAUDER_YELLOW,
    SHOT_STATION,
    SHOT_TITLE,
    SHOT_PLANET_TERRAN,
    SHOT_PLANET_GAS,
    SHOT_ASTEROIDS,
    SHOT_WARP,
    SHOT_EXPLOSION,
    SHOT_DUST,
    SHOT_BASE,
    SHOT_COUNT,
} shot_t;

static char const* const SHOT_NAMES[SHOT_COUNT] = {
    [SHOT_PLAYER]          = "player",
    [SHOT_MARAUDER_GREEN]  = "marauder_green",
    [SHOT_MARAUDER_YELLOW] = "marauder_yellow",
    [SHOT_STATION]         = "station",
    [SHOT_TITLE]           = "title",
    [SHOT_PLANET_TERRAN]   = "planet_terran",
    [SHOT_PLANET_GAS]      = "planet_gas",
    [SHOT_ASTEROIDS]       = "asteroids",
    [SHOT_WARP]            = "warp",
    [SHOT_EXPLOSION]       = "explosion",
    [SHOT_DUST]            = "dust",
    [SHOT_BASE]            = "base",
};

// The title, as scene 1 will set it: "Superior" scaled so both lines are
// equally long (D-29).
#define TITLE_CAP   1.0f
#define TITLE_DEPTH 0.3f
static title_line_t s_title[2];

// Warp shot: out at 1 s (facing away), in at 5 s (facing the camera).
#define WARP_OUT_T 1.0f
#define WARP_IN_T  5.0f
// Explosion shot: three laser hits, then the ship blows up at 2 s.
#define BLAST_T    2.0f
// Dust shot: riding along at this speed.
#define DUST_SPEED 20.0f

static shot_t shot_at(double t) {
    int const i = (int)(t / ASSET_SECS);
    return (shot_t)(i < 0 ? 0 : (i >= SHOT_COUNT ? SHOT_COUNT - 1 : i));
}

static void viewer_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    marauder_init();
    station_init();
    starfield_init();
    planet_init();
    planet_base_init();
    asteroid_init();
    explosion_init();
    space_dust_init();
    title_line_make(&s_title[0], "Borderworlds:", TITLE_CAP, TITLE_DEPTH, TITLE_HERO);
    float const w = s_title[0].width;
    title_line_make(&s_title[1], "Superior", TITLE_CAP * w / title_text_width("Superior", TITLE_CAP), TITLE_DEPTH,
                    TITLE_MARAUDER);
}

static void viewer_shutdown(void) {
    player_ship_shutdown();
    marauder_shutdown();
    station_shutdown();
    planet_shutdown();
    planet_base_shutdown();
    asteroid_shutdown();
    explosion_shutdown();
    title_line_free(&s_title[0]);
    title_line_free(&s_title[1]);
}

static void viewer_enter(void) {
    // A far "sun" up and to the left behind the camera's start position:
    // effectively directional.
    se_light_set(&(se_light_t){.x = -600.0f, .y = 400.0f, .z = -500.0f, .brightness = 0.75f, .two_sided = true});
}

// Orbit `target` at distance d and height h (above it), starting behind-left.
static void orbit_at(double t, float d, float h, vec3_t target) {
    float const  a   = (float)fmod(ORBIT_RATE * t, 2.0 * M_PI) + 2.4f;
    vec3_t const eye = v3_add(target, v3(d * sinf(a), h, d * cosf(a)));
    camera_look_at(eye, target, 0.0f);
}

static void orbit(double t, float d, float h) {
    orbit_at(t, d, h, v3(0.0f, 0.0f, 0.0f));
}

static xform_t ident(float scale) {
    return (xform_t){mat3_rot_y(0.0f), v3(0.0f, 0.0f, 0.0f), scale};
}

static void viewer_camera(double t) {
    shot_t const shot = shot_at(t);
    double const lt   = t - (double)shot * ASSET_SECS;  // time within the shot
    switch (shot) {
        case SHOT_STATION:
            orbit(lt * 0.3, 62.0f, 14.0f);
            break;
        case SHOT_TITLE: {
            // In front of the lines (they read from -z), swaying.
            float const  a   = 0.5f * sinf(0.5f * (float)lt);
            vec3_t const mid = v3(0.0f, -0.9f, 0.0f);
            camera_look_at(v3_add(mid, v3(12.0f * sinf(a), 1.5f, -12.0f * cosf(a))), mid, 0.0f);
            break;
        }
        case SHOT_PLANET_TERRAN:
        case SHOT_PLANET_GAS:
            orbit(lt * 0.4, 3.2f, 0.6f);
            break;
        case SHOT_ASTEROIDS:
            orbit(lt, 5.0f, 1.0f);
            break;
        case SHOT_WARP:
            camera_look_at(v3(3.0f, 1.2f, -7.0f), v3(0.0f, 0.0f, 6.0f), 0.0f);
            break;
        case SHOT_EXPLOSION:
            orbit(lt * 0.5, 4.5f, 1.0f);
            break;
        case SHOT_DUST: {
            vec3_t const ship = v3(0.0f, 0.0f, DUST_SPEED * (float)lt);
            camera_look_at(v3_add(ship, v3(1.4f, 0.5f, -2.6f)), v3_add(ship, v3(0.0f, 0.0f, 1.5f)), 0.0f);
            break;
        }
        case SHOT_BASE:
            orbit_at(lt * 0.4, 48.0f, 11.0f, v3(0.0f, 4.0f, 10.0f));
            break;
        default:
            orbit(lt, 1.6f, 0.35f);
            break;
    }
}

// Two blue shots a second from the hero's guns, as the marauders' below.
static void guns_player(xform_t const* x, double lt) {
    for (int side = 0; side < 2; side++) {
        float const  tf  = floorf((float)lt) + 0.5f * (float)side;
        vec3_t const gun = player_ship_gun(x, side);
        laser_submit_beam(gun, v3_add(gun, v3(0.0f, 0.0f, 20.0f)), (float)lt, tf, &LASER_STYLE_PLAYER);
    }
}

static void viewer_submit(double t) {
    shot_t const shot = shot_at(t);
    double const lt   = t - (double)shot * ASSET_SECS;  // time within the shot
    float const  ft   = (float)lt;
    xform_t      x    = ident(1.0f);

    if (shot != SHOT_BASE) starfield_submit();
    switch (shot) {
        case SHOT_PLAYER:
            player_ship_submit(&x, 1.0f, t);
            guns_player(&x, lt);
            break;
        case SHOT_MARAUDER_GREEN:
        case SHOT_MARAUDER_YELLOW: {
            marauder_livery_t const l = shot == SHOT_MARAUDER_GREEN ? MARAUDER_GREEN : MARAUDER_YELLOW;
            marauder_submit(&x, l, 1.0f, t, (unsigned)l + 1u);
            // A shot from each gun every second, alternating.
            for (int side = 0; side < 2; side++) {
                float const  tf  = floorf(ft) + 0.5f * (float)side;
                vec3_t const gun = marauder_gun(&x, side);
                laser_submit_beam(gun, v3_add(gun, v3(0.0f, 0.0f, 20.0f)), ft, tf, &LASER_STYLE_MARAUDER);
            }
            break;
        }
        case SHOT_STATION:
            x.r = mat3_rot_z((float)fmod(STATION_SPIN * t, 2.0 * M_PI));
            station_submit(&x);
            break;
        case SHOT_TITLE:
            for (int i = 0; i < 2; i++) {
                // Centred; "Superior" under "Borderworlds:", clear of its
                // descender.
                xform_t const lx = {mat3_rot_y(0.0f), v3(-0.5f * s_title[i].width, i ? -2.6f : 0.0f, 0.0f), 1.0f};
                title_line_submit(&s_title[i], &lx);
            }
            break;
        case SHOT_PLANET_TERRAN:
        case SHOT_PLANET_GAS:
            x.r = mat3_rot_y(0.1f * ft);
            planet_submit(&x, shot == SHOT_PLANET_TERRAN ? PLANET_TERRAN : PLANET_GAS);
            break;
        case SHOT_ASTEROIDS:
            x.r = mat3_axis_angle(v3_norm(v3(0.3f, 1.0f, 0.2f)), 0.2f * ft);
            asteroid_submit(0, &x);
            for (int i = 1; i < ASTEROID_SHAPES; i++) {
                float const   a  = 2.1f * (float)i;
                xform_t const xs = {mat3_axis_angle(v3_norm(v3(1.0f, 0.5f, (float)i)), 0.7f * ft),
                                    v3(2.6f * sinf(a), 0.6f * (float)(i - 2), 2.6f * cosf(a)), 0.3f};
                asteroid_submit(i, &xs);
            }
            break;
        case SHOT_WARP: {
            // Out: facing away (+z), from WARP_OUT_T. In: facing the camera
            // (-z), arriving at WARP_IN_T.
            bool const       out  = ft < 3.0f;
            warp_dir_t const dir  = out ? WARP_OUT : WARP_IN;
            float const      tw   = out ? WARP_OUT_T : WARP_IN_T;
            xform_t const    base = {out ? mat3_rot_y(0.0f) : mat3_rot_y(3.1415927f), v3(0.0f, 0.0f, 0.0f), 1.0f};
            xform_t          pose;
            if (warp_pose(&base, ft, tw, dir, &pose)) marauder_submit(&pose, MARAUDER_GREEN, 1.0f, t, 1u);
            warp_submit_flash(warp_point(&base, dir), 1.0f, ft, tw, dir, 77u);
            break;
        }
        case SHOT_EXPLOSION:
            x.r = mat3_rot_y(1.5707963f);  // broadside to the orbit's start
            if (ft < BLAST_T) {
                marauder_submit(&x, MARAUDER_YELLOW, 1.0f, t, 2u);
                // Hits on the hull: nose, left wing, tail.
                static float const HIT_T[3]    = {0.5f, 0.9f, 1.3f};
                static float const HIT_P[3][3] = {{0.0f, 0.05f, 0.55f}, {-0.35f, 0.0f, -0.1f}, {0.1f, 0.06f, -0.45f}};
                for (int i = 0; i < 3; i++) {
                    vec3_t const p = xform_apply(&x, v3(HIT_P[i][0], HIT_P[i][1], HIT_P[i][2]));
                    impact_submit(p, v3_sub(p, x.pos), 0.3f, ft, HIT_T[i], 90u + (unsigned)i);
                }
            } else {
                marauder_submit_debris(&x, v3(0.0f, 0.0f, 0.0f), MARAUDER_YELLOW, ft, BLAST_T, 5u);
            }
            explosion_submit(x.pos, v3(0.0f, 0.0f, 0.0f), 1.0f, ft, BLAST_T, 9u);
            break;
        case SHOT_DUST: {
            vec3_t const vel = v3(0.0f, 0.0f, DUST_SPEED);
            x.pos            = v3_scale(vel, ft);
            space_dust_submit(vel, 0.03f);
            player_ship_submit(&x, 1.0f, t);
            guns_player(&x, lt);
            break;
        }
        case SHOT_BASE:
        default:
            planet_base_submit(t);
            break;
    }
}

static char const* viewer_shot(double t) {
    return SHOT_NAMES[shot_at(t)];
}

static backdrop_t const SPACE = {0};

static backdrop_t const* viewer_backdrop(double t) {
    return shot_at(t) == SHOT_BASE ? planet_base_backdrop() : &SPACE;
}

scene_def_t const SCENE_ASSET_VIEWER = {
    .name        = "assets",
    .duration    = ASSET_SECS * SHOT_COUNT,
    .init        = viewer_init,
    .shutdown    = viewer_shutdown,
    .enter       = viewer_enter,
    .camera      = viewer_camera,
    .submit      = viewer_submit,
    .shot        = viewer_shot,
    .backdrop_at = viewer_backdrop,
};
