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

// Scene 1: the 3D title "Borderworlds:" / "Superior", drifting in.
extern scene_def_t const SCENE_TITLE;

// Scene 2: the hero ship lands at the planet base.
extern scene_def_t const SCENE_PLANET_LANDING;

// Scene 3: the marauders fly towards the planet (medium close-up).
extern scene_def_t const SCENE_MARAUDER_APPROACH;

// Opening: the two marauders in formation, from close behind, firing
// at the (unseen) player.
extern scene_def_t const SCENE_MARAUDER_PURSUIT;

// The player's ship threads a turning wheel station, chased by two
// marauders.
extern scene_def_t const SCENE_SPACESTATION_FLYBY;

// Development: orbits each asset in turn (one named shot per asset).
extern scene_def_t const SCENE_ASSET_VIEWER;

// Development: the sky/ground backdrop under every camera attitude
// (level, a full roll, pitching).
extern scene_def_t const SCENE_HORIZON_TEST;
