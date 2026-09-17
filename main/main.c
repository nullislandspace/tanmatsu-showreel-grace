// =====================================================================
//  Showreel  --  app entry point
// ---------------------------------------------------------------------
//  Scaffold only. The engine owns the frame loop (se_run): device and
//  display bootstrap, the two framebuffers, the input-queue pump, the
//  device-global keys (volume, audio jack, F1 = launcher), vsync and the
//  blit. This file is what a game plugs into that loop -- for now, one
//  spinning triangle, so there is something on screen that proves the
//  engine is wired in and the projection is sane.
//
//  Note: the graceloader template called gpio_install_isr_service(0),
//  which the engine does NOT do. Nothing here installs a GPIO ISR, so it
//  stays out; add it back before se_run() if that ever changes.
// =====================================================================

#include <math.h>

#include "esp_log.h"
#include "synthengine3d.h"  // the whole engine public API

static char const TAG[] = "showreel";

#define TRI_Z      4.0f  // distance in front of the camera, world units
#define TRI_APEX_Y 2.0f  // apex height above the ground plane
#define TRI_SPIN   1.0f  // radians per second

static float s_angle = 0.0f;

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

    // Output-neutral scene passes (both default OFF; see se_scene.h).
    // Frustum cull is a near-pure win. depth_order is an overdraw-
    // dependent trade-off, so it stays off until there is real content
    // to measure it against.
    scene_set_options(&(se_scene_options_t){
        .frustum_cull = true,
        .depth_order  = false,
    });
}

// Per-frame logic. dt is seconds since the previous frame, already
// clamped by the engine to SE_FRAME_DT_MAX.
static void on_update(float dt, void* user) {
    (void)user;
    s_angle += TRI_SPIN * dt;
}

// Per frame, after the engine clears the backdrop.
static void on_render(pax_buf_t* fb, void* user) {
    (void)user;
    render_set_camera(0.0f, 1.0f);  // eye at x=0, height 1, looking +z
    scene_begin(fb);

    // The base corners orbit the vertical axis through (0, ., TRI_Z) and
    // the apex sits on that axis, so the triangle sweeps rather than
    // translates.
    float const c = cosf(s_angle);
    float const s = sinf(s_angle);
    scene_tri(-c, 0.0f, TRI_Z - s,      // base-left
              c, 0.0f, TRI_Z + s,       // base-right
              0.0f, TRI_APEX_Y, TRI_Z,  // apex
              0xFFFF31F1u);             // magenta

    scene_render(SE_RENDER_ZBUFFER);
}

// Hand the loop to the engine. Does not return under graceloader: F1
// reboots to the launcher.
void app_main(void) {
    static se_app_config_t const cfg = {
        .f1_exits      = true,         // engine handles F1 = back to launcher
        .backdrop_argb = 0xFF101018u,  // cleared to this each frame
    };
    static se_app_callbacks_t const cb = {
        .on_init   = on_init,
        .on_update = on_update,  // required
        .on_render = on_render,
    };
    se_run(&cfg, &cb, NULL);
}
