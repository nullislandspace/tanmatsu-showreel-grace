// =====================================================================
//  Showreel scene  --  title (scene 1)
// ---------------------------------------------------------------------
//  "Borderworlds:" / "Superior" in 3D block letters on the planet
//  scene's sky (D-28, D-29, D-34, D-35). Both lines lie in one plane canted into the screen: the
//  left end near the left edge, the right end further away, ending at
//  ~70% of the screen width. "Superior" is set larger so both lines are
//  equally long, and they start and end at the same screen x. The first
//  line drifts down from above the screen, the second up from below;
//  they ease into place, hold, and a slow dolly keeps the hold alive.
//  Once they are in place the hero ship flies past behind them, left
//  to right, parallel to the lettering: in at the left edge of the
//  screen, out at the right.
//
//  The cant is solved at init from where the ends should land on screen
//  (title_layout below), not tuned by hand.
// =====================================================================

#include <math.h>
#include "camera.h"
#include "space/assets/planet_base.h"
#include "space/assets/player_ship.h"
#include "space/assets/title_text.h"
#include "space/assets/title_text_mesh.h"
#include "space/space.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define LINE1_IN   0.3f  // "Borderworlds:" starts drifting down ...
#define LINE2_IN   0.8f  // ... "Superior" up, half a second later
#define DRIFT_SECS 4.5f  // each takes this long to settle
#define HOLD_SECS  7.2f  // both in place, before the next scene
#define SCENE_SECS (LINE2_IN + DRIFT_SECS + HOLD_SECS)
#define FLY_IN     (LINE2_IN + DRIFT_SECS + 0.3f)  // the flypast starts once both lines rest
#define FLY_SECS   4.0f                            // edge to edge

#define SKY_ARGB PLANET_SKY_ARGB  // the planet's sky: the reel's next scene

// --- Layout -----------------------------------------------------------------
#define LEFT_FRAC  0.05f  // left end of both lines, fraction of the screen width
#define RIGHT_FRAC 0.70f  // right end
#define CANT_DEG   28.0f  // how far the plane turns away: the right end recedes
#define CAP1       1.0f   // cap height of "Borderworlds:"
#define DEPTH      0.3f   // extrusion
// Resting baselines (world y; the eye is at y = 0, which the projection
// puts at row 256 -- a little below the middle). "Superior" (cap 1.65,
// descender 0.47) sits 0.35 below the first line's baseline, and the
// block is centred on the screen.
#define BASE1_Y    0.92f
#define BASE2_Y    (-1.08f)
#define DRIFT_Y    6.5f  // how far above / below its rest a line starts: off screen

// --- The flypast ----------------------------------------------------------------
// Behind the lettering's plane, level between the two lines, so the
// letters cut across the ship as it passes.
#define SHIP_SPAN   2.4f
#define SHIP_BEHIND 2.4f   // behind the letters' front faces, along the plane's normal
#define SHIP_Y      0.75f  // between "Superior"'s cap line (0.57) and the first baseline

// --- Camera -------------------------------------------------------------------
#define DOLLY 0.6f  // units forward over the whole scene, centred on t = SCENE_SECS / 2

static title_line_t s_line[2];
static mat3_t       s_cant;          // the lines' plane: yawed so the right end recedes
static vec3_t       s_left;          // left end of the baselines (x, z; y per line)
static float        s_fly0, s_fly1;  // the flypast: start and end along the plane's +x

// Place the plane: turned by the cant a, the left end at screen x
// LEFT_FRAC and the right end (the line's width W further along the
// plane) at RIGHT_FRAC, from an eye at the origin looking down +z. With
// k_l, k_r the two ends' x/z on screen and z0 the left end's depth:
//   left:  x0 = k_l z0
//   right: x0 + W cos(a) = k_r (z0 + W sin(a))
// so z0 = W (cos(a) - k_r sin(a)) / (k_r - k_l). The cant alone decides
// how large the letters look (the projection does not care about scale).
static void title_layout(float width) {
    float const w  = (float)DISPLAY_LOG_W;
    float const kl = (LEFT_FRAC * w - RENDER_HALF_W) / RENDER_FOCAL_LEN;
    float const kr = (RIGHT_FRAC * w - RENDER_HALF_W) / RENDER_FOCAL_LEN;
    float const a  = CANT_DEG * 3.1415927f / 180.0f;
    float const z0 = width * (cosf(a) - kr * sinf(a)) / (kr - kl);
    // The line's +x runs (cos a, 0, sin a): a yaw of -a (mat3_rot_y turns
    // +x towards -z for a positive angle).
    s_cant         = mat3_rot_y(-a);
    s_left         = v3(kl * z0, 0.0f, z0);
}

static float camera_z(float t) {
    return DOLLY * (t / SCENE_SECS - 0.5f);
}

// The ship's centre at distance s along the plane's +x, on its line.
static vec3_t ship_at(float s) {
    vec3_t const along = mat3_apply(&s_cant, v3(1.0f, 0.0f, 0.0f));
    vec3_t const back  = mat3_apply(&s_cant, v3(0.0f, 0.0f, 1.0f));
    return v3_add(v3_add(v3(s_left.x, SHIP_Y, s_left.z), v3_scale(back, SHIP_BEHIND)), v3_scale(along, s));
}

// Where along its line the ship's centre sits on the ray through screen
// column `col` (from the camera at the flypast's midpoint): with the
// line's point p + s u, solve (p.x + s u.x) = k (p.z + s u.z - cam_z).
static float ship_s_at_column(float col) {
    float const  k  = (col - RENDER_HALF_W) / RENDER_FOCAL_LEN;
    float const  cz = camera_z(FLY_IN + 0.5f * FLY_SECS);
    vec3_t const p  = ship_at(0.0f);
    vec3_t const u  = mat3_apply(&s_cant, v3(1.0f, 0.0f, 0.0f));
    return (k * (p.z - cz) - p.x) / (u.x - k * u.z);
}

static void title_init(char const* asset_dir) {
    player_ship_init(asset_dir);
    title_line_make(&s_line[0], "Borderworlds:", CAP1, DEPTH, TITLE_HERO);
    float const w = s_line[0].width;
    title_line_make(&s_line[1], "Superior", CAP1 * w / title_text_width("Superior", CAP1), DEPTH, TITLE_MARAUDER);
    title_layout(w);
    // Start and end just off screen: the whole ship, flames included,
    // outside the edge.
    vec3_t lo, hi;
    player_ship_bounds(&lo, &hi);
    float const reach = SHIP_SPAN * (fmaxf(hi.z, -lo.z) + player_ship_flame_reach());
    s_fly0            = ship_s_at_column(0.0f) - reach;
    s_fly1            = ship_s_at_column((float)DISPLAY_LOG_W) + reach;
}

static void title_shutdown(void) {
    title_line_free(&s_line[0]);
    title_line_free(&s_line[1]);
    player_ship_shutdown();
}

static void title_enter(void) {
    // Up and to the left, on the camera's side: the faces lit, the
    // extruded sides and bevels in shade, so the letters read as solid.
    se_light_set(&(se_light_t){.x = -300.0f, .y = 400.0f, .z = -350.0f, .brightness = 0.75f, .two_sided = true});
}

static void title_camera(double td) {
    float const t = (float)td;
    render_set_camera_6dof(0.0f, 0.0f, camera_z(t), 0.0f, 0.0f, 0.0f);
}

// A line's baseline height at t: from `start` off screen, eased to `rest`.
static float drift(float t, float t_in, float rest, float start) {
    return start + (rest - start) * smoothstep(t_in, t_in + DRIFT_SECS, t);
}

static void title_submit(double td) {
    float const t    = (float)td;
    float const y[2] = {drift(t, LINE1_IN, BASE1_Y, BASE1_Y + DRIFT_Y), drift(t, LINE2_IN, BASE2_Y, BASE2_Y - DRIFT_Y)};
    for (int i = 0; i < 2; i++) {
        xform_t const x = {s_cant, v3(s_left.x, y[i], s_left.z), 1.0f};
        title_line_submit(&s_line[i], &x);
    }
    // The flypast: constant speed along the line, nose along the plane's
    // +x, full throttle.
    if (t >= FLY_IN && t <= FLY_IN + FLY_SECS) {
        float const   s    = s_fly0 + (s_fly1 - s_fly0) * (t - FLY_IN) / FLY_SECS;
        vec3_t const  fwd  = mat3_apply(&s_cant, v3(1.0f, 0.0f, 0.0f));
        xform_t const ship = {mat3_from_fwd_up(fwd, v3(0.0f, 1.0f, 0.0f), 0.0f), ship_at(s), SHIP_SPAN};
        player_ship_submit(&ship, 1.0f, td);
    }
}

scene_def_t const SCENE_TITLE = {
    .name     = "title",
    .duration = SCENE_SECS,
    .init     = title_init,
    .shutdown = title_shutdown,
    .enter    = title_enter,
    .camera   = title_camera,
    .submit   = title_submit,
    .backdrop = {.sky_argb = SKY_ARGB},
};
