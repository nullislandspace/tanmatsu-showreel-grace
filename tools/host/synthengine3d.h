#pragma once
// Host stand-in for the engine's umbrella header (make scenecheck). The
// scene and asset code only uses the scene, light and texture APIs, so
// this includes the REAL engine headers for those -- the types stay the
// engine's own -- and tools/host/engine_stub.c implements the functions.
#include "se_config.h"
#include "se_light.h"
#include "se_scene.h"
#include "se_texture.h"
