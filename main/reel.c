// =====================================================================
//  Showreel  --  the reel (see reel.h)
// =====================================================================

#include "reel.h"
#include <stddef.h>
#include <string.h>
#include "esp_log.h"
#include "scenes/scenes.h"
#include "showtime.h"

static char const TAG[] = "reel";

// Every scene there is (initialised at startup, selectable by name).
static scene_def_t const* const ALL_SCENES[] = {
    &SCENE_TURNTABLE,
};
#define ALL_N (sizeof(ALL_SCENES) / sizeof(ALL_SCENES[0]))

// What plays, in order, looping.
static scene_def_t const* const PLAYLIST[] = {
    &SCENE_TURNTABLE,
};
#define PLAY_N (sizeof(PLAYLIST) / sizeof(PLAYLIST[0]))

static scene_def_t const* s_cur;
static size_t             s_play_idx;
static double             s_start;  // show time at the scene's t = 0
static bool               s_hold;

static void enter(scene_def_t const* sc) {
    s_cur   = sc;
    s_start = showtime_now();
    if (sc->enter) sc->enter();
    ESP_LOGI(TAG, "scene: %s", sc->name);
}

void reel_init(char const* asset_dir) {
    for (size_t i = 0; i < ALL_N; i++) {
        if (ALL_SCENES[i]->init) ALL_SCENES[i]->init(asset_dir);
    }
    s_play_idx = 0;
    enter(PLAYLIST[0]);
}

void reel_shutdown(void) {
    for (size_t i = 0; i < ALL_N; i++) {
        if (ALL_SCENES[i]->shutdown) ALL_SCENES[i]->shutdown();
    }
}

void reel_next(void) {
    s_hold     = false;
    s_play_idx = (s_play_idx + 1) % PLAY_N;
    enter(PLAYLIST[s_play_idx]);
}

void reel_frame(void) {
    if (s_hold || s_cur->duration <= 0.0f) return;
    if (reel_scene_time() >= (double)s_cur->duration) reel_next();
}

void reel_submit(void) {
    s_cur->submit(reel_scene_time());
}

bool reel_hold(char const* name) {
    for (size_t i = 0; i < ALL_N; i++) {
        if (strcmp(ALL_SCENES[i]->name, name) == 0) {
            enter(ALL_SCENES[i]);
            s_hold = true;
            return true;
        }
    }
    return false;
}

void reel_release(void) {
    s_hold = false;
}

char const* reel_scene_name(void) {
    return s_cur->name;
}

char const* reel_shot_name(void) {
    return s_cur->shot ? s_cur->shot(reel_scene_time()) : "";
}

float reel_scene_duration(void) {
    return s_cur->duration;
}

double reel_scene_time(void) {
    return showtime_now() - s_start;
}

double reel_scene_start(void) {
    return s_start;
}
