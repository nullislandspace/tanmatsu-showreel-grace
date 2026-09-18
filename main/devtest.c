// =====================================================================
//  Showreel  --  automated device tests (see devtest.h)
// =====================================================================

#include "devtest.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "bsp/device.h"
#include "debugcon.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "profile.h"
#include "reel.h"
#include "report.h"
#include "screenshot.h"
#include "showtime.h"
#include "synthengine3d.h"

#define SHOT_DIR          "/sd/showreel/test"
#define SHOTS_MAX         32
#define SHOTPERF_MAX      16
#define PERF_SECS_ENDLESS 30.0f
// The shots test steps the clock itself; fixed-step mode keeps the wall
// clock out of it entirely.
#define SHOTS_FIXED_FPS   30.0f

typedef enum {
    T_IDLE = 0,
    T_PERF,
    T_SHOTS,
} test_t;

typedef struct {
    char    name[24];
    int     frames;
    int64_t rast_us, rast_max, interval_us, interval_max;
    int64_t tris, ttris, lines;
} shot_acc_t;

static void (*s_stats_restart)(void);
static test_t s_test;
static char   s_scene[32];

// perf
static float      s_secs;
static int64_t    s_last_frame_us;
static shot_acc_t s_acc[SHOTPERF_MAX];
static int        s_acc_n;

// shots
static float s_t[SHOTS_MAX];
static int   s_t_n, s_t_i;
static bool  s_shots_ok;

void devtest_start(void (*stats_restart)(void)) {
    s_stats_restart = stats_restart;
    debugcon_start();
}

// Leave exactly as the engine's F1 does (se_run.c): audio down, then
// back to the launcher. Does not return.
static void exit_to_launcher(char const* why) {
    report_emitf("BYE", "{\"t\":\"bye\",\"why\":\"%s\"}", why);
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(300));  // let the USB FIFO drain
    audio_mixer_shutdown();
    bsp_device_restart_to_launcher();
}

static void end_test(char const* status) {
    report_emitf("END", "{\"t\":\"end\",\"status\":\"%s\",\"scene\":\"%s\"}", status, s_scene);
    s_test = T_IDLE;
    exit_to_launcher("test done");
}

// Value of `key=` in the argument string, copied into out (empty if absent).
static bool arg(char const* args, char const* key, char* out, size_t n) {
    size_t const kl = strlen(key);
    for (char const* p = args; p && *p;) {
        while (*p == ' ') p++;
        if (strncmp(p, key, kl) == 0 && p[kl] == '=') {
            p        += kl + 1;
            size_t i  = 0;
            while (*p && *p != ' ' && i + 1 < n) out[i++] = *p++;
            out[i] = '\0';
            return true;
        }
        p = strchr(p, ' ');
    }
    if (n) out[0] = '\0';
    return false;
}

static void start_perf(char const* args) {
    char secs[16];
    arg(args, "secs", secs, sizeof(secs));
    showtime_set_realtime();
    if (!reel_hold(s_scene)) {
        report_emitf("END", "{\"t\":\"end\",\"status\":\"error\",\"msg\":\"no scene '%s'\"}", s_scene);
        exit_to_launcher("bad scene");
        return;
    }
    // Default: the scene's own length, or a fixed stretch for an
    // endless one.
    s_secs = secs[0] ? (float)strtol(secs, NULL, 10) : reel_scene_duration();
    if (s_secs <= 0.0f) s_secs = PERF_SECS_ENDLESS;
    s_acc_n         = 0;
    s_last_frame_us = 0;
    s_test          = T_PERF;
    if (s_stats_restart) s_stats_restart();
    report_emitf("BEGIN", "{\"t\":\"begin\",\"test\":\"perf\",\"scene\":\"%s\",\"secs\":%.2f}", s_scene,
                 (double)s_secs);
}

static void start_shots(char const* args) {
    // Instants in whole milliseconds (graceloader exports no float
    // parser, and integer ms also make exact, reproducible file names).
    char list[192];
    arg(args, "ms", list, sizeof(list));
    s_t_n = 0;
    for (char* p = list; *p && s_t_n < SHOTS_MAX;) {
        s_t[s_t_n++] = (float)strtol(p, &p, 10) / 1000.0f;
        if (*p == ',')
            p++;
        else
            break;
    }
    if (s_t_n == 0 || !reel_hold(s_scene)) {
        report_emitf("END", "{\"t\":\"end\",\"status\":\"error\",\"msg\":\"need scene= and ms=\"}");
        exit_to_launcher("bad args");
        return;
    }
    showtime_set_fixed_step(SHOTS_FIXED_FPS);
    mkdir("/sd/showreel", 0777);
    mkdir(SHOT_DIR, 0777);
    s_t_i      = 0;
    s_shots_ok = true;
    s_test     = T_SHOTS;
    report_emitf("BEGIN", "{\"t\":\"begin\",\"test\":\"shots\",\"scene\":\"%s\",\"n\":%d}", s_scene, s_t_n);
}

void devtest_update(void) {
    char line[DEBUGCON_LINE_MAX];
    if (debugcon_poll(line)) {
        if (strcmp(line, "EXIT") == 0 || strcmp(line, "BADGELINK") == 0) {
            exit_to_launcher(line);
            return;
        }
        // "RUN <test> k=v ..."
        char        test[16] = {0};
        char const* rest     = line + 4;
        size_t      i        = 0;
        while (*rest && *rest != ' ' && i + 1 < sizeof(test)) test[i++] = *rest++;
        arg(rest, "scene", s_scene, sizeof(s_scene));
        debugcon_hello("HELLO");  // identity, straight into the test's log
        if (strcmp(test, "perf") == 0) {
            start_perf(rest);
        } else if (strcmp(test, "shots") == 0) {
            start_shots(rest);
        } else {
            report_emitf("END", "{\"t\":\"end\",\"status\":\"error\",\"msg\":\"unknown test '%s'\"}", test);
            exit_to_launcher("unknown test");
            return;
        }
    }
    // Shots: this frame draws exactly the next requested instant.
    if (s_test == T_SHOTS && s_t_i < s_t_n) showtime_set(reel_scene_start() + (double)s_t[s_t_i]);
}

static uint32_t fnv1a(void const* data, size_t n) {
    uint8_t const* p = data;
    uint32_t       h = 0x811C9DC5u;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x01000193u;
    return h;
}

static shot_acc_t* shot_slot(char const* name) {
    if (name[0] == '\0') name = "-";  // a scene without shot names: one slot
    for (int i = 0; i < s_acc_n; i++) {
        if (strcmp(s_acc[i].name, name) == 0) return &s_acc[i];
    }
    if (s_acc_n == SHOTPERF_MAX) return NULL;
    shot_acc_t* a = &s_acc[s_acc_n++];
    memset(a, 0, sizeof(*a));
    strlcpy(a->name, name, sizeof(a->name));
    return a;
}

static void perf_frame(int64_t rast_us) {
    int64_t const now   = esp_timer_get_time();
    int           tri_n = 0, line_n = 0, ttri_n = 0;
    int64_t       tri_us = 0, line_us = 0, ttri_us = 0;
    scene_raster_stats(&tri_n, &line_n, &tri_us, &line_us);
    scene_textured_stats(&ttri_n, &ttri_us);

    shot_acc_t* a = shot_slot(reel_shot_name());
    if (a != NULL && s_last_frame_us != 0) {
        int64_t const iv = now - s_last_frame_us;
        a->frames++;
        a->rast_us     += rast_us;
        a->interval_us += iv;
        if (rast_us > a->rast_max) a->rast_max = rast_us;
        if (iv > a->interval_max) a->interval_max = iv;
        a->tris  += tri_n;
        a->ttris += ttri_n;
        a->lines += line_n;
    }
    s_last_frame_us = now;

    if (reel_scene_time() < (double)s_secs) return;
    for (int i = 0; i < s_acc_n; i++) {
        shot_acc_t const* s = &s_acc[i];
        if (s->frames == 0) continue;
        double const f = (double)s->frames;
        report_emitf("SHOTPERF",
                     "{\"shot\":\"%s\",\"frames\":%d,\"fps\":%.2f,\"frame_ms_max\":%.2f,\"rast_ms\":%.2f,"
                     "\"rast_ms_max\":%.2f,\"tris\":%.1f,\"ttris\":%.1f,\"lines\":%.1f}",
                     s->name, s->frames, 1e6 * f / (double)s->interval_us, (double)s->interval_max / 1000.0,
                     (double)s->rast_us / f / 1000.0, (double)s->rast_max / 1000.0, (double)s->tris / f,
                     (double)s->ttris / f, (double)s->lines / f);
    }
    end_test("ok");
}

static void shots_frame(pax_buf_t* fb) {
    float const t = s_t[s_t_i];
    char        path[96];
    snprintf(path, sizeof(path), SHOT_DIR "/%s_%06d.png", s_scene, (int)(t * 1000.0f + 0.5f));
    uint32_t const hash = fnv1a(pax_buf_get_pixels(fb), pax_buf_get_size(fb));
    bool const     ok   = screenshot_capture_to(fb, path);
    if (!ok) s_shots_ok = false;

    int     tri_n = 0, line_n = 0, ttri_n = 0;
    int64_t tri_us = 0, line_us = 0, ttri_us = 0;
    scene_raster_stats(&tri_n, &line_n, &tri_us, &line_us);
    scene_textured_stats(&ttri_n, &ttri_us);
    report_emitf("SHOT",
                 "{\"i\":%d,\"t\":%.3f,\"shot\":\"%s\",\"path\":\"%s\",\"ok\":%s,\"fnv\":\"%08" PRIx32
                 "\",\"tris\":%d,\"ttris\":%d,\"lines\":%d}",
                 s_t_i, (double)t, reel_shot_name(), path, ok ? "true" : "false", hash, tri_n, ttri_n, line_n);

    if (++s_t_i == s_t_n) end_test(s_shots_ok ? "ok" : "bad");
}

void devtest_after_render(pax_buf_t* fb, int64_t rast_us) {
    switch (s_test) {
        case T_PERF:
            perf_frame(rast_us);
            break;
        case T_SHOTS:
            shots_frame(fb);
            break;
        default:
            break;
    }
}

void devtest_period(float fps, float frame_ms) {
    if (s_test != T_PERF) return;
    float ms[PROF_COUNT] = {0};
    prof_snapshot(ms);
    int     tri_n = 0, line_n = 0, ttri_n = 0;
    int64_t tri_us = 0, line_us = 0, ttri_us = 0;
    scene_raster_stats(&tri_n, &line_n, &tri_us, &line_us);
    scene_textured_stats(&ttri_n, &ttri_us);

    char ph[200];
    int  w = 0;
    for (int i = 0; i < PROF_COUNT && w < (int)sizeof(ph); i++) {
        w += snprintf(ph + w, sizeof(ph) - (size_t)w, "%s\"%s\":%.3f", i ? "," : "", prof_name((prof_phase_t)i),
                      (double)ms[i]);
    }
    report_emitf("PERF",
                 "{\"t\":%.2f,\"shot\":\"%s\",\"fps\":%.2f,\"frame_ms\":%.3f,\"ph\":{%s},\"tris\":%d,\"tri_us\":%lld,"
                 "\"ttris\":%d,\"ttri_us\":%lld,\"lines\":%d,\"line_us\":%lld,\"sram\":%u,\"sram_big\":%u}",
                 reel_scene_time(), reel_shot_name(), (double)fps, (double)frame_ms, ph, tri_n, (long long)tri_us,
                 ttri_n, (long long)ttri_us, line_n, (long long)line_us,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
}
