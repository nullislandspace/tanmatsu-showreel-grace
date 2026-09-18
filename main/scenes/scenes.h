#pragma once
// =====================================================================
//  Showreel  --  every scene the reel knows about
// ---------------------------------------------------------------------
//  One file per scene in scenes/. Adding a scene: define its
//  scene_def_t there, declare it here, list it in reel.c's ALL_SCENES
//  (and in PLAYLIST if it should play).
// =====================================================================

#include "scene.h"

// The player's ship on a turntable against black. The first reel item;
// kept for reuse, not in the playlist once real scenes exist.
extern scene_def_t const SCENE_TURNTABLE;

// Development: orbits each asset in turn (one named shot per asset).
extern scene_def_t const SCENE_ASSET_VIEWER;
