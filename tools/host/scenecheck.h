#pragma once
// =====================================================================
//  Showreel  --  hooks between the host engine stand-in and the checker
// ---------------------------------------------------------------------
//  tools/host/engine_stub.c calls these for every primitive a scene
//  submits; tools/scenecheck.c implements them (make scenecheck).
// =====================================================================

#include <stdbool.h>
#include <stdint.h>
#include "xform.h"

// World-space primitives as submitted (before any clipping).
void sc_tri(vec3_t const v[3], bool textured);
void sc_line(vec3_t a, vec3_t b, uint32_t argb);
void sc_point(vec3_t p);

// The current camera, as the engine sees it.
vec3_t sc_to_camera(vec3_t world);
void   sc_project(vec3_t cam, float* sx, float* sy);
