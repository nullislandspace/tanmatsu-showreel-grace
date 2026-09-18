// =====================================================================
//  Showreel scene  --  title (scene 1)
// ---------------------------------------------------------------------
//  "Borderworlds:" / "Superior" in 3D block letters on a sky-blue
//  backdrop (D-28, D-29, D-34). Both lines lie in one plane canted into the screen: the
//  left end near the left edge, the right end further away, ending at
//  ~70% of the screen width. "Superior" is set larger so both lines are
//  equally long, and they start and end at the same screen x. The first
//  line drifts down from above the screen, the second up from below;
//  they ease into place, hold, and a slow dolly keeps the hold alive.
//
//  The cant is solved at init from where the ends should land on screen
//  (title_layout below), not tuned by hand.
// =====================================================================

#include <math.h>
#include "assets/title_text.h"
#include "assets/title_text_mesh.h"
#include "camera.h"
#include "scenes/scenes.h"
#include "synthengine3d.h"

// --- Timing (scene time, seconds) -------------------------------------------
#define LINE1_IN   0.3f  // "Borderworlds:" starts drifting down ...
#define LINE2_IN   0.8f  // ... "Superior" up, half a second later
#define DRIFT_SECS 4.5f  // each takes this long to settle
#define HOLD_SECS  7.2f  // both in place, before the next scene
#define SCENE_SECS (LINE2_IN + DRIFT_SECS + HOLD_SECS)

#define SKY_ARGB 0xFF87CEEBu  // sky blue

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

// --- Camera -------------------------------------------------------------------
#define DOLLY 0.6f  // units forward over the whole scene, centred on t = SCENE_SECS / 2

static title_line_t s_line[2];
static mat3_t       s_cant;  // the lines' plane: yawed so the right end recedes
static vec3_t       s_left;  // left end of the baselines (x, z; y per line)

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

static void title_init(char const* asset_dir) {
    (void)asset_dir;
    title_line_make(&s_line[0], "Borderworlds:", CAP1, DEPTH, TITLE_HERO);
    float const w = s_line[0].width;
    title_line_make(&s_line[1], "Superior", CAP1 * w / title_text_width("Superior", CAP1), DEPTH, TITLE_MARAUDER);
    title_layout(w);
}

static void title_shutdown(void) {
    title_line_free(&s_line[0]);
    title_line_free(&s_line[1]);
}

static void title_enter(void) {
    // Up and to the left, on the camera's side: the faces lit, the
    // extruded sides and bevels in shade, so the letters read as solid.
    se_light_set(&(se_light_t){.x = -300.0f, .y = 400.0f, .z = -350.0f, .brightness = 0.75f, .two_sided = true});
}

static void title_camera(double td) {
    float const t = (float)td;
    float const z = DOLLY * (t / SCENE_SECS - 0.5f);
    render_set_camera_6dof(0.0f, 0.0f, z, 0.0f, 0.0f, 0.0f);
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
