#pragma once
// =====================================================================
//  Showreel  --  development scenes of the core (not of any segment)
// ---------------------------------------------------------------------
//  They exercise the shared machinery (backdrop, horizon) and are never
//  in the playlist; the tests select them by name.
// =====================================================================

#include "scene.h"

// The sky/ground backdrop under every camera attitude (level, a full
// roll, pitching).
extern scene_def_t const SCENE_HORIZON_TEST;
