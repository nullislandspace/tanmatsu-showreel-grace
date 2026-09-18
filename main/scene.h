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
#include "backdrop.h"

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
    // Set the camera for instant t. Called first every frame, before the
    // backdrop is queued: a sky/ground backdrop needs the horizon, so the
    // camera, before the geometry is submitted -- and the PPA paints
    // while submit() runs.
    void (*camera)(double t);
    // Draw instant t (seconds since the scene started): submit the
    // geometry. The camera is already set.
    void (*submit)(double t);
    // Optional: the name of the shot running at t (logs and perf).
    char const* (*shot)(double t);
    // What is behind everything (backdrop.h). Zero-initialised: black
    // space.
    backdrop_t backdrop;
    // Optional: a backdrop that changes during the scene (e.g. per shot);
    // when set it is used instead of `backdrop`.
    backdrop_t const* (*backdrop_at)(double t);
} scene_def_t;
