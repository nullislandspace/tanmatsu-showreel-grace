// =====================================================================
//  Showreel scene  --  the player's ship on a turntable
// ---------------------------------------------------------------------
//  The ship spins in place in front of a fixed camera, nodding gently,
//  lit from behind and left of the camera. Nothing else -- no stars,
//  no floor -- so the mesh and the shading are all there is to look at.
// =====================================================================

#include <math.h>
#include "assets/player_ship.h"
#include "scenes/scenes.h"
#include "synthengine3d.h"
#include "xform.h"

// --- Framing ----------------------------------------------------------
//
// The camera is the engine's legacy pose: eye at the origin, looking
// straight down +z. The ship sits SHIP_DIST_Z in front of it.
#define SHIP_DIST_Z 3.0f   // world units in front of the eye
#define SHIP_SPAN   2.25f  // wingspan in world units

// Why 2.25: only the span-to-distance ratio decides the projected size,
// and the limit is perspective on whatever the turntable swings nearest
// -- not the flat wingspan. The flames set it: they carry the silhouette
// ~3.3 raw model units past the tail, so tail-on their tips reach
// towards the camera and balloon, and broadside the ship is hull plus
// flame long. Swept over every (yaw, nod) pose with the flames at full
// flicker length, 2.25 leaves ~26 px on the tightest pose (broadside),
// keeps the nearest point at z >= 1.08 (clear of RENDER_NEAR_CLIP_Z),
// and puts the widest silhouette at ~652 px. Longer flames or a bigger
// ship means re-running that sweep.

// Where the hull's centre sits in world y. The projection puts world y
// equal to the camera's at row RENDER_HORIZON_Y (256), which is 16 rows
// below the middle of a 480-row screen -- so parking the ship at the
// camera height would leave it sitting low. Lifting it by exactly that
// row offset, converted back through the projection, lands the hull's
// centre on the screen's centre.
#define SHIP_CENTER_Y ((RENDER_HORIZON_Y - (float)DISPLAY_LOG_H / 2.0f) * SHIP_DIST_Z / RENDER_FOCAL_LEN)

// --- Motion -----------------------------------------------------------
//
// Yaw is the turntable: one revolution every ~10 s. The pitch nod is
// what makes it a showreel rather than a spec sheet -- without it the
// roof and the belly never come into view. Incommensurate with the yaw
// rate on purpose, so the pose never quite repeats.
#define YAW_RATE 0.6f   // rad/s
#define NOD_AMP  0.25f  // rad, peak pitch either side of level
#define NOD_RATE 0.23f  // rad/s of the nod's sine

// --- Light ------------------------------------------------------------
//
// Aimed relative to the hull's centre: start from the direction the ship
// sees the camera in (straight back down -z), swing it 45 deg to the
// left and lift it 20 deg above the horizontal plane through the hull.
// At this distance the swing carries it past the eye, so it ends up
// behind the camera as well as left of it -- a three-quarter key light.
// 75% directional, 25% global: a face turned away falls to a quarter.
#define LIGHT_AZIMUTH_DEG   45.0f
#define LIGHT_ELEVATION_DEG 20.0f
#define LIGHT_DISTANCE      6.0f
#define LIGHT_BRIGHTNESS    0.75f

static void turntable_init(char const* asset_dir) {
    player_ship_init(asset_dir);
}

static void turntable_shutdown(void) {
    player_ship_shutdown();
}

static void turntable_enter(void) {
    float const az = LIGHT_AZIMUTH_DEG * (float)M_PI / 180.0f;
    float const el = LIGHT_ELEVATION_DEG * (float)M_PI / 180.0f;
    float const h  = cosf(el);
    se_light_set(&(se_light_t){
        .x          = -LIGHT_DISTANCE * h * sinf(az),
        .y          = SHIP_CENTER_Y + LIGHT_DISTANCE * sinf(el),
        .z          = SHIP_DIST_Z - LIGHT_DISTANCE * h * cosf(az),
        .brightness = LIGHT_BRIGHTNESS,
        // The mesh is outward-wound and back faces are culled, so the flip
        // is a no-op; kept because a mis-wound face then still lights.
        .two_sided  = true,
    });
}

static void turntable_camera(double t) {
    (void)t;
    render_set_camera(0.0f, 0.0f);
}

static void turntable_submit(double t) {
    // Wrap the angles so the float trig keeps its precision on long runs.
    float const   yaw   = (float)fmod(YAW_RATE * t, 2.0 * M_PI);
    float const   pitch = NOD_AMP * sinf((float)fmod(NOD_RATE * t, 2.0 * M_PI));
    // Yaw about the ship's own vertical axis first, then nod about the
    // world's lateral axis: Rx(nod) * Ry(yaw).
    mat3_t const  ry    = mat3_rot_y(yaw);
    mat3_t const  rx    = mat3_rot_x(pitch);
    xform_t const x     = {
            .r     = mat3_mul(&rx, &ry),
            .pos   = v3(0.0f, SHIP_CENTER_Y, SHIP_DIST_Z),
            .scale = SHIP_SPAN,
    };
    player_ship_submit(&x, 1.0f, t);
}

scene_def_t const SCENE_TURNTABLE = {
    .name     = "turntable",
    .duration = 0.0f,  // until skipped
    .init     = turntable_init,
    .shutdown = turntable_shutdown,
    .enter    = turntable_enter,
    .camera   = turntable_camera,
    .submit   = turntable_submit,
};
