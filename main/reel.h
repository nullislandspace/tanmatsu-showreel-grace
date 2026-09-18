#pragma once
// =====================================================================
//  Showreel  --  the reel: which scene plays, and when
// ---------------------------------------------------------------------
//  Plays the scenes in PLAYLIST (reel.c) one after another, each for its
//  duration, and loops. Every scene is timed from the show clock
//  (showtime.h), so the reel is a pure function of show time too.
//
//  Per frame: showtime_frame(), then reel_frame(), then reel_camera(),
//  the backdrop (reel_backdrop()), and (between scene_begin and
//  scene_prepare) reel_submit().
// =====================================================================

#include <stdbool.h>
#include "backdrop.h"

// Initialise every scene (loads all assets). Once, from on_init.
void reel_init(char const* asset_dir);
void reel_shutdown(void);

// Advance the playlist if the current scene has run its course.
void              reel_frame(void);
// Set the current scene's camera for the current show time. First thing
// in a frame: the backdrop (reel_backdrop) needs it.
void              reel_camera(void);
// The current scene's backdrop.
backdrop_t const* reel_backdrop(void);
// Draw the current scene at the current show time (camera already set).
void              reel_submit(void);

// Skip to the next scene in the playlist.
void reel_next(void);
// Start the playlist over from its first scene, at the current show
// time, and reset the cycle count.
void reel_restart(void);
// How many times the playlist has wrapped round to its first scene since
// startup or reel_restart().
int  reel_cycles(void);
// Play the scene called `name` (any scene, played or not) from t = 0
// and stay on it (no advancing) until reel_release(). For tests.
// Returns false if there is no such scene.
bool reel_hold(char const* name);
void reel_release(void);

char const* reel_scene_name(void);
// The current shot's name, or "" if the scene does not name its shots.
char const* reel_shot_name(void);
// The current scene's duration in seconds (<= 0: endless).
float       reel_scene_duration(void);
// Seconds since the current scene started.
double      reel_scene_time(void);
// Show time at which the current scene started (t = 0).
double      reel_scene_start(void);
