#pragma once
// =====================================================================
//  Showreel  --  what a scene is
// ---------------------------------------------------------------------
//  One reel item: a camera and some assets, choreographed as a PURE
//  FUNCTION OF TIME. submit(t) is handed the seconds since the scene
//  started and must draw exactly that instant -- no state carried from
//  frame to frame, no per-frame dt -- so any instant can be rendered on
//  its own (the automated shot tests jump straight to one) and a slow
//  frame cannot make the choreography drift (showtime.h).
//
//  Assets are shared between scenes (assets/): a scene's init calls the
//  init of every asset it uses, and those are idempotent.
// =====================================================================

#include <stdbool.h>

typedef struct {
    char const* name;
    // Seconds the scene runs before the reel moves on; <= 0 means until
    // skipped (N key).
    float       duration;
    // Load / build what the scene needs (once, at startup).
    void (*init)(char const* asset_dir);
    void (*shutdown)(void);
    // The scene becomes current: set the light and anything else global.
    void (*enter)(void);
    // Draw instant t (seconds since the scene started): set the camera
    // first, then submit the geometry.
    void (*submit)(double t);
    // Optional: the name of the shot running at t (logs and perf).
    char const* (*shot)(double t);
} scene_def_t;
