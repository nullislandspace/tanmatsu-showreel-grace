// =====================================================================
//  Showreel  --  app entry point
// ---------------------------------------------------------------------
//  The engine owns the frame loop (se_run): device and display
//  bootstrap, the two framebuffers, the input-queue pump, the
//  device-global keys (volume, audio jack, F1 = launcher), vsync and the
//  blit. This file is what a game plugs into that loop.
//
//  What is on screen is the reel's business (reel.h): a playlist of
//  scenes (scenes/), each a pure function of the show clock
//  (showtime.h), built from shared assets (assets/). This file only
//  runs the frame: clock, backdrop, submit, rasterize, stats, keys.
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

#include <stdbool.h>
#include "devtest.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "graceloader.h"
#include "profile.h"
#include "reel.h"
#include "screenshot.h"
#include "showtime.h"
#include "synthengine3d.h"  // the whole engine public API

static char const TAG[] = "showreel";

// Not handed to se_app_config_t.backdrop_argb: that field is only used
// when no on_backdrop callback is registered, and this app registers
// one. Alpha is ignored for an RGB565 target.
#define BACKDROP_ARGB 0xFF000000u

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
static bool s_shot_pending  = false;  // P pressed; capture at end of frame

// Once-a-second profiling, two lines, in the same shape as Stunt Racer's:
//
//   the phase split  -- per-frame ms for each prof_phase_t, the
//                       engine's blit and vsync wait among them, plus
//                       the unclaimed residual (profile.h), and
//   the summary      -- FPS, the renderer, the scene[/shot], the last frame's rasterize
//                       split (post-cull triangle / edge counts and their
//                       wallclock), and internal SRAM.
//
// SRAM is reported as total free AND the largest free block, because the
// second is the one that actually gates an allocation: total free can
// look healthy while fragmentation has already made the next
// contiguous buffer impossible. Both come from MALLOC_CAP_INTERNAL only
// -- PSRAM is a separate 32 MB pool and would drown the number that
// matters. Sampled once per period rather than tracked, so a transient
// dip between two samples will not show; this is a trend, not a
// low-water mark.
static int64_t s_stats_last_us = 0;  // start of the current period; 0 = restart
static int     s_stats_frames  = 0;

// Throw away the period in progress and start a fresh one at the next
// log_frame_stats(). For stalls the numbers should not average in: a
// screenshot blocks the loop for about a second, which would otherwise
// show up as one period of ~15 fps with a huge "rest".
static void stats_restart(void) {
    s_stats_last_us = 0;
    s_stats_frames  = 0;
}

static void log_frame_stats(void) {
    int64_t const now = esp_timer_get_time();
    if (s_stats_last_us == 0) {
        // First frame after the splash (or after stats_restart()): start
        // the clock at the END of it and drop its phase times, so the
        // first period covers exactly the frames it counts -- not
        // on_init, se_splash(), a screenshot's stall or half a frame of
        // phases the clock never saw start.
        s_stats_last_us = now;
        prof_reset();
        return;
    }
    s_stats_frames++;
    prof_frame();

    // The engine's present runs after on_render returns, so what it
    // reports now is the present that happened since the previous
    // on_render -- inside this period, one per frame counted.
    int64_t blit_us = 0, vsync_us = 0;
    se_present_stats(&blit_us, &vsync_us);
    prof_add(PROF_BLIT, blit_us);
    prof_add(PROF_VSYNC, vsync_us);
    int64_t const elapsed = now - s_stats_last_us;
    if (elapsed < 1000000) return;

    // Last frame's rasterize split. Instantaneous, not averaged over the
    // period like the phases -- on a scene this steady the two agree.
    int     tri_n = 0, line_n = 0, ttri_n = 0;
    int64_t tri_us = 0, line_us = 0, ttri_us = 0;
    scene_raster_stats(&tri_n, &line_n, &tri_us, &line_us);
    scene_textured_stats(&ttri_n, &ttri_us);

    float const frame_ms = (float)elapsed / (1000.0f * (float)s_stats_frames);
    float const fps      = (float)((double)s_stats_frames * 1000000.0 / (double)elapsed);
    devtest_period(fps, frame_ms);
    char phases[160];
    if (prof_flush(phases, sizeof(phases), frame_ms)) {
        ESP_LOGI(TAG, "  %.2f ms/frame:  %s", (double)frame_ms, phases);
    }

    char const* const shot = reel_shot_name();
    ESP_LOGI(TAG,
             "%.1f fps  %s  [%s%s%s]  tris %d (%lld us)  ttris %d (%lld us)  lines %d (%lld us)  sram free %u KiB "
             "largest %u KiB",
             (double)s_stats_frames * 1000000.0 / (double)elapsed, se_renderer_name(RENDER_MODE), reel_scene_name(),
             shot[0] ? "/" : "", shot, tri_n, (long long)tri_us, ttri_n, (long long)ttri_us, line_n, (long long)line_us,
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024));

    s_stats_frames  = 0;
    s_stats_last_us = now;
}

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

    // Every scene and its assets (textures from wherever graceloader
    // started us: the SD card install, /sd/apps/at.cavac.showreel). After
    // se_splash() so the splash's frames are not delayed by the file
    // reads, and so the splash keeps its own flat colours: each scene
    // sets its own light when it is entered.
    reel_init(graceloader_get_install_basepath());

    // Debug console for the automated tests (devtest.h): idle unless the
    // host sends a command, then it runs the test and returns to the
    // launcher by itself.
    devtest_start(stats_restart);

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

// Input events the engine did not consume itself (F1 and the volume
// keys never get here). Scancodes arrive for release too, with
// BSP_INPUT_SCANCODE_RELEASE_MODIFIER set, so the exact matches below
// fire on the press only.
//
//   P   screenshot, taken at the end of the frame (not here) so the file
//       holds a finished image
//   N   next scene
static void on_input(bsp_input_event_t const* ev, void* user) {
    (void)user;
    if (ev->type != INPUT_EVENT_TYPE_SCANCODE) return;
    switch (ev->args_scancode.scancode) {
        case BSP_INPUT_SCANCODE_P:
            s_shot_pending = true;
            break;
        case BSP_INPUT_SCANCODE_N:
            reel_next();
            break;
        default:
            break;
    }
}

// Per-frame logic: latch this frame's show time, then let the reel move
// on if the current scene has run its course. The engine's dt is not
// used: every scene is a function of the show clock, not of dt.
static void on_update(float dt, void* user) {
    (void)user;
    (void)dt;
    showtime_frame();
    devtest_update();  // may steer the show clock (shot tests)
    reel_frame();
}

// Start of frame. Enqueue the black FILL, then do every bit of CPU work
// that does NOT touch framebuffer pixels while the PPA runs it: the
// camera, the model transform, the triangle/edge submission and the
// engine's cull + order pass. scene_begin() and scene_prepare() are
// documented as pixel-free (se_scene.h), which is exactly what makes
// them safe to overlap with a hardware blit into the framebuffer.
static void on_backdrop(pax_buf_t* fb, void* user) {
    (void)user;

    prof_begin(PROF_FILL);
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
        // whose submit returned false. Timed as "fill" too, so a PPA
        // that has silently stopped working shows up as this phase
        // jumping from ~0 to several milliseconds.
        pax_background(fb, BACKDROP_ARGB);
    }
    prof_end(PROF_FILL);

    prof_begin(PROF_SUBMIT);
    scene_begin(fb);
    reel_submit();  // the scene sets its camera first, then submits
    prof_end(PROF_SUBMIT);

    prof_begin(PROF_PREPARE);
    scene_prepare(RENDER_MODE);
    prof_end(PROF_PREPARE);
}

// Rest of frame: paint. The FILL has to be complete first -- the ship
// writes into the middle of the screen the fill is blacking out, and a
// fill still in flight would land on top of the hull.
static void on_render(pax_buf_t* fb, void* user) {
    (void)user;

    prof_begin(PROF_WAIT);
    if (s_fill_inflight) {
        se_ppa_wait_job(JOB_CLEAR);
    }
    prof_end(PROF_WAIT);

    prof_begin(PROF_RASTER);
    int64_t const rast_t0 = esp_timer_get_time();
    scene_rasterize(RENDER_MODE);
    int64_t const rast_us = esp_timer_get_time() - rast_t0;
    prof_end(PROF_RASTER);

    // Last thing in the frame, so the capture is of the finished image.
    // It blocks for as long as the SD write takes (about a second). That
    // stall is taken out of the show clock, so the show resumes where it
    // was instead of jumping a second ahead, and the stats period is
    // restarted so it does not land in the performance numbers.
    if (s_shot_pending) {
        s_shot_pending   = false;
        int64_t const t0 = esp_timer_get_time();
        screenshot_capture(fb);
        showtime_exclude(esp_timer_get_time() - t0);
        stats_restart();
    }

    devtest_after_render(fb, rast_us);
    log_frame_stats();
}

// Only if the loop is ever asked to stop (se_request_exit). Under
// graceloader F1 reboots to the launcher and this never runs; kept so
// the textures have a matching unload.
static void on_shutdown(void* user) {
    (void)user;
    reel_shutdown();
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
        .on_input    = on_input,
        .on_update   = on_update,  // required
        .on_backdrop = on_backdrop,
        .on_render   = on_render,
        .on_shutdown = on_shutdown,
    };
    se_run(&cfg, &cb, NULL);
}
