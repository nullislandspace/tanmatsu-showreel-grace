// =====================================================================
//  Showreel  --  app entry point
// ---------------------------------------------------------------------
//  The engine owns the frame loop (se_run): device and display
//  bootstrap, the two framebuffers, the input-queue pump, the
//  device-global keys (volume, audio jack, F1 = launcher), vsync and the
//  blit. This file is what a game plugs into that loop.
//
//  First reel item: the Race the Synth ship on a turntable against a
//  black screen (see ship.c). Nothing else -- no floor, no horizon, no
//  HUD -- so the mesh and the shading are all there is to look at.
//
//  The black is a PPA FILL, not a CPU clear. The framebuffers live in
//  PSRAM, so se_run's default pax_background() clear would push 768 KB
//  of RGB565 through the CPU every frame with nothing overlapping it.
//  se_ppa_fill() hands that to the PPA and returns immediately, and the
//  frame is split so the geometry work runs while the hardware fills
//  (see on_backdrop / on_render below).
//
//  Note: the graceloader template called gpio_install_isr_service(0),
//  which the engine does NOT do. Nothing here installs a GPIO ISR, so it
//  stays out; add it back before se_run() if that ever changes.
// =====================================================================

#include <math.h>
#include <stdbool.h>
#include "esp_log.h"
#include "ship.h"
#include "synthengine3d.h"  // the whole engine public API

static char const TAG[] = "showreel";

// Eye on the z = 0 plane at height 0, looking straight down +z. The
// ship frames itself against this (SHIP_CENTER_Y in ship.c), so moving
// the camera means re-deriving that.
#define CAM_X 0.0f
#define CAM_Y 0.0f

// Not handed to se_app_config_t.backdrop_argb: that field is only used
// when no on_backdrop callback is registered, and this app registers
// one. Alpha is ignored for an RGB565 target.
#define BACKDROP_ARGB 0xFF000000u

// --- Scene light ------------------------------------------------------
//
// One positional light (se_light.h), aimed relative to the hull's centre
// rather than in absolute world coordinates, so it keeps its framing if
// the ship's distance changes.
//
// Placed behind and to the left of the camera: start from the direction
// the ship sees the camera in (straight back down -z), swing it 45 deg
// to the left and lift it 20 deg above the horizontal plane through the
// hull. At this distance the 45 deg swing carries it past the eye, so it
// ends up behind the camera (negative z) as well as left of it -- a
// three-quarter key light, which is what makes a faceted hull read as
// solid instead of as a silhouette.
#define LIGHT_AZIMUTH_DEG   45.0f  // left of the ship -> camera axis
#define LIGHT_ELEVATION_DEG 20.0f  // above the plane through the hull
#define LIGHT_DISTANCE      6.0f   // world units from the hull's centre
// Directional share of the total illumination: 75% from the light, 25%
// global. A face turned away from the light falls to a quarter of its
// colour, so the faceting reads hard rather than softly modelled.
#define LIGHT_BRIGHTNESS    0.75f

// Same renderer for both halves of the split -- prepare builds what
// rasterize consumes, so they must agree. One hull filling the screen
// is light overdraw, the case SE_RENDER_RAYCAST's per-pixel edge tests
// tend to lose on, so this starts at the z-buffer; scene_raster_stats()
// is there to settle it once the reel has denser items to measure.
#define RENDER_MODE SE_RENDER_ZBUFFER

// The frame's only PPA job. Ids are ours to scheme and the frame drains
// its own (the wait below), so restarting at 0 every frame is fine.
#define JOB_CLEAR 0u

static bool s_ppa_up        = false;  // compositor came up in on_init
static bool s_fill_inflight = false;  // this frame's FILL was accepted

// Once, after the engine has booted display + audio + scene, before the
// first frame.
static void on_init(void* user) {
    (void)user;
    ESP_LOGI(TAG, "SynthEngine3D %s", se_version_string());

    // Engine title sequence. Blocking: it draws and presents its own
    // frames for about a second, then returns. First thing in on_init so
    // it plays against a clean framebuffer, and it needs se_run to have
    // bootstrapped (it borrows the engine's framebuffers and vsync) --
    // which on_init guarantees.
    se_splash();

    // Bring up the PPA compositor (registers the FILL client and starts
    // the engine's pump task). A failure is logged and degrades to the
    // CPU clear in on_backdrop -- slower, never a blank screen.
    s_ppa_up = se_ppa_init();
    if (!s_ppa_up) {
        ESP_LOGW(TAG, "PPA unavailable -- falling back to CPU backdrop clear");
    }

    // Aim the scene light. After se_splash() so the splash keeps its own
    // flat colours, and once here rather than per frame -- neither the
    // light nor the hull's centre moves (the ship spins in place).
    float cx, cy, cz;
    ship_center(&cx, &cy, &cz);
    float const az = LIGHT_AZIMUTH_DEG * (float)M_PI / 180.0f;
    float const el = LIGHT_ELEVATION_DEG * (float)M_PI / 180.0f;
    float const h  = cosf(el);  // horizontal part of the unit direction
    se_light_set(&(se_light_t){
        .x          = cx - LIGHT_DISTANCE * h * sinf(az),
        .y          = cy + LIGHT_DISTANCE * sinf(el),
        .z          = cz - LIGHT_DISTANCE * h * cosf(az),
        .brightness = LIGHT_BRIGHTNESS,
        // The mesh is CCW-outward and ship.c culls its own back faces, so
        // the cross-product normal already points at the camera and the
        // flip is a no-op -- kept on because it costs one dot product and
        // makes a mis-wound triangle light correctly rather than go dark.
        .two_sided  = true,
    });

    // Output-neutral scene passes (both default OFF; see se_scene.h).
    // Frustum cull is a near-pure win. depth_order is an overdraw-
    // dependent trade-off: one hull filling the screen is light
    // overdraw, which is the case it can lose on, so it stays off until
    // the reel has a scene dense enough to measure it against.
    scene_set_options(&(se_scene_options_t){
        .frustum_cull = true,
        .depth_order  = false,
    });
}

// Per-frame logic. dt is seconds since the previous frame, already
// clamped by the engine to SE_FRAME_DT_MAX.
static void on_update(float dt, void* user) {
    (void)user;
    ship_update(dt);
}

// Start of frame. Enqueue the black FILL, then do every bit of CPU work
// that does NOT touch framebuffer pixels while the PPA runs it: the
// camera, the model transform, the triangle/edge submission and the
// engine's cull + order pass. scene_begin() and scene_prepare() are
// documented as pixel-free (se_scene.h), which is exactly what makes
// them safe to overlap with a hardware blit into the framebuffer.
static void on_backdrop(pax_buf_t* fb, void* user) {
    (void)user;

    s_fill_inflight = false;
    if (s_ppa_up) {
        // Whole screen, in logical rows -- pax_buf_get_height() is
        // orientation-aware, so this is the 480 the ship projects into
        // rather than the panel's raw 800.
        s_fill_inflight = se_ppa_fill(fb, JOB_CLEAR, 0, pax_buf_get_height(fb), BACKDROP_ARGB);
    }
    if (!s_fill_inflight) {
        // Refused (queue full / unsupported orientation) or PPA never
        // came up. Clear on the CPU instead; must NOT wait on a job id
        // whose submit returned false.
        pax_background(fb, BACKDROP_ARGB);
    }

    render_set_camera(CAM_X, CAM_Y);
    scene_begin(fb);
    ship_submit();
    scene_prepare(RENDER_MODE);
}

// Rest of frame: paint. The FILL has to be complete first -- the ship
// writes into the middle of the screen the fill is blacking out, and a
// fill still in flight would land on top of the hull.
static void on_render(pax_buf_t* fb, void* user) {
    (void)user;
    (void)fb;
    if (s_fill_inflight) {
        se_ppa_wait_job(JOB_CLEAR);
    }
    scene_rasterize(RENDER_MODE);
}

// Hand the loop to the engine. Does not return under graceloader: F1
// reboots to the launcher.
void app_main(void) {
    static se_app_config_t const cfg = {
        .f1_exits = true,  // engine handles F1 = back to launcher
        // No backdrop_argb: on_backdrop below owns the clear.
    };
    static se_app_callbacks_t const cb = {
        .on_init     = on_init,
        .on_update   = on_update,  // required
        .on_backdrop = on_backdrop,
        .on_render   = on_render,
    };
    se_run(&cfg, &cb, NULL);
}
