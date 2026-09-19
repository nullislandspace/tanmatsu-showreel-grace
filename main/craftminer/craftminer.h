#pragma once
// =====================================================================
//  Showreel  --  CraftMiner: every scene it has
// ---------------------------------------------------------------------
//  One file per scene in craftminer/scenes/, the block world in
//  craftminer/voxel/, the other assets in craftminer/assets/, the
//  textures in textures/craftminer/. The plan and its log:
//  claudeplans/craftminer.md. Adding a scene: define its scene_def_t
//  there, declare it here, list it in reel.c's ALL_SCENES (and in
//  PLAYLIST if it should play).
// =====================================================================

#include "scene.h"

// Scene 1: "CraftMiner" in blocks, popping into the sky over the meadow.
extern scene_def_t const SCENE_CM_TITLE;
// Scene 2: a flight over the block world.
extern scene_def_t const SCENE_CM_OVERWORLD;
// Scene 3: the miner walks across the meadow to the cliff.
extern scene_def_t const SCENE_CM_WALK;
// Scene 4: mining into the cliff, third person then first person.
extern scene_def_t const SCENE_CM_MINING;
// Scene 5: the log cabin goes up.
extern scene_def_t const SCENE_CM_BUILDING;
// Scene 6: nightfall at the finished cabin.
extern scene_def_t const SCENE_CM_NIGHTFALL;

// Development: the block world from the air, on foot and among the
// trees, to tune the level of detail (not in the playlist).
extern scene_def_t const SCENE_CM_WORLD_TEST;
