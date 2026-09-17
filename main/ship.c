// =====================================================================
//  Showreel  --  the Race the Synth ship, on a turntable
// ---------------------------------------------------------------------
//  See ship.h. The mesh is objects/ship_model.h, vendored from
//  tanmatsu-synthracer-grace (which generates it from openscad/ship.3mf);
//  nothing here hand-edits it, so a re-export drops straight in.
//
//  Back-face culling stays here: the engine sees only anonymous
//  projected triangles, whereas this file knows the mesh's winding
//  (se_scene.h). Shading does NOT -- the engine's positional light
//  (se_light.h, configured in main.c) shades each face inside
//  scene_tri, so this file submits flat region colours.
// =====================================================================

#include "ship.h"
#include <math.h>
#include <stdint.h>
#include "objects/ship_model.h"
#include "synthengine3d.h"

// --- Framing ----------------------------------------------------------
//
// The model is game-sized: SHIP_MODEL_SCALE fits it to the racer's
// collision box, which is a 0.56-world-unit wingspan -- about 63 px
// across at a playable distance. A showreel wants it filling the
// screen, so this uses its own scale instead of the header's.

#define SHIP_DIST_Z      3.0f  // world units in front of the eye
#define SHIP_TARGET_SPAN 2.8f  // wingspan in world units
// Lateral extent of the raw model, in model units (x spans -5.75..5.75).
// From the mesh, not a free parameter: re-export the 3MF wider and this
// is what has to change for SHIP_TARGET_SPAN to still mean what it says.
#define SHIP_MODEL_SPAN  11.5f
#define SHIP_SCALE       (SHIP_TARGET_SPAN / SHIP_MODEL_SPAN)

// Why 2.8 and not bigger: only the span-to-distance ratio decides the
// projected size, and the limit is not the flat wingspan -- it is
// perspective on whichever corner of the hull the turntable has swung
// nearest. Broadside, the near wing blows the silhouette up to ~719 px
// across; nose-quartering with the nod at full tilt drives the roof up
// towards the top edge. Swept over every (yaw, nod) pose this pair
// leaves about 20 px of margin on the tightest one and never crosses
// RENDER_NEAR_CLIP_Z. Enlarging the ship means shrinking SHIP_NOD_AMP
// to pay for it.

// The model is centred in x and z but base-anchored in y (belly at
// y = 0, roof at 2.949), so the turntable axis has to run through the
// hull's own mid-height or the ship would orbit its belly.
#define SHIP_MODEL_MID_Y 1.474519f

// Where the hull's centre sits in world y. The projection puts world y
// equal to the camera's at row RENDER_HORIZON_Y (256), which is 16 rows
// below the middle of a 480-row screen -- so parking the ship at the
// camera height would leave it sitting low. Lifting it by exactly that
// row offset, converted back through the projection, lands the hull's
// centre on the screen's centre. Derived from the RENDER_* constants so
// it follows an FOV or horizon override rather than going stale.
#define SHIP_CENTER_Y ((RENDER_HORIZON_Y - (float)DISPLAY_LOG_H / 2.0f) * SHIP_DIST_Z / RENDER_FOCAL_LEN)

// --- Motion -----------------------------------------------------------
//
// Yaw is the turntable: one revolution every ~10 s, which is slow
// enough to read the faceting and fast enough not to look stalled. The
// pitch nod is what makes it a showreel rather than a spec sheet --
// without it the roof and the belly never come into view. Incommensurate
// with the yaw rate on purpose, so the pose never quite repeats.
#define SHIP_YAW_RATE 0.6f   // rad/s
#define SHIP_NOD_AMP  0.25f  // rad, peak pitch either side of level
#define SHIP_NOD_RATE 0.23f  // rad/s of the nod's sine

static float s_yaw   = 0.0f;
static float s_nod_t = 0.0f;

void ship_center(float* x, float* y, float* z) {
    if (x != NULL) *x = 0.0f;
    if (y != NULL) *y = SHIP_CENTER_Y;
    if (z != NULL) *z = SHIP_DIST_Z;
}

void ship_update(float dt) {
    s_yaw += SHIP_YAW_RATE * dt;
    // Wrap rather than letting it grow without bound: cosf/sinf lose
    // precision on a large argument, and this runs until F1.
    if (s_yaw > 2.0f * (float)M_PI) s_yaw -= 2.0f * (float)M_PI;
    s_nod_t += SHIP_NOD_RATE * dt;
    if (s_nod_t > 2.0f * (float)M_PI) s_nod_t -= 2.0f * (float)M_PI;
}

void ship_submit(void) {
    float const pitch = SHIP_NOD_AMP * sinf(s_nod_t);
    float const cy = cosf(s_yaw), sy = sinf(s_yaw);
    float const cp = cosf(pitch), sp = sinf(pitch);

    // Model -> world, once per vertex: centre the mesh on its own axes,
    // scale to the framing above, yaw about its vertical axis, nod about
    // its lateral axis, then push it out in front of the camera.
    static float wx[SHIP_MODEL_VERT_COUNT];
    static float wy[SHIP_MODEL_VERT_COUNT];
    static float wz[SHIP_MODEL_VERT_COUNT];
    for (size_t i = 0; i < SHIP_MODEL_VERT_COUNT; i++) {
        ship_model_vert_t const* v  = &SHIP_MODEL_VERTS[i];
        float const              mx = v->x * SHIP_SCALE;
        float const              my = (v->y - SHIP_MODEL_MID_Y) * SHIP_SCALE;
        float const              mz = v->z * SHIP_SCALE;

        float const ax = mx * cy + mz * sy;  // yaw about +y
        float const az = -mx * sy + mz * cy;

        wx[i] = ax;
        wy[i] = my * cp - az * sp + SHIP_CENTER_Y;  // nod about +x
        wz[i] = my * sp + az * cp + SHIP_DIST_Z;
    }

    // Every region is drawn: the racer hides the battery panel and the
    // magnet poles until those attachments are fitted, but there is no
    // attachment state here and the fully equipped ship is the more
    // interesting model.
    render_camera_t const cam = render_camera();

    for (size_t i = 0; i < SHIP_MODEL_TRI_COUNT; i++) {
        ship_model_tri_t const* t = &SHIP_MODEL_TRIS[i];
        int const               a = t->a, b = t->b, c = t->c;

        // CCW-outward face normal from two edges of the face.
        float const ux = wx[b] - wx[a], uy = wy[b] - wy[a], uz = wz[b] - wz[a];
        float const vx = wx[c] - wx[a], vy = wy[c] - wy[a], vz = wz[c] - wz[a];
        float const nx = uy * vz - uz * vy;
        float const ny = uz * vx - ux * vz;
        float const nz = ux * vy - uy * vx;

        // Back-face cull -- the engine will not do this (it sees only
        // anonymous projected triangles), and without it the far side of
        // the hull z-fights the near side as the turntable comes round.
        float const fcx = (wx[a] + wx[b] + wx[c]) * (1.0f / 3.0f);
        float const fcy = (wy[a] + wy[b] + wy[c]) * (1.0f / 3.0f);
        float const fcz = (wz[a] + wz[b] + wz[c]) * (1.0f / 3.0f);
        if (nx * (cam.x - fcx) + ny * (cam.y - fcy) + nz * (cam.z - fcz) <= 0.0f) {
            continue;
        }

        // Flat region colour. The shading is the engine's now: with a
        // light set (see main.c) scene_tri derives this face's normal
        // from the very vertices passed below and scales the colour
        // itself, once, at submit time.
        scene_tri(wx[a], wy[a], wz[a], wx[b], wy[b], wz[b], wx[c], wy[c], wz[c], SHIP_REGION_COLOR[t->region]);
    }

    // The cyan ridge outline, drawn over the faces. scene_render biases
    // edges towards the camera so an edge wins against the face it
    // outlines but still loses to genuinely nearer geometry -- so the
    // far-side ridges stay hidden behind the hull without this needing
    // to cull them.
    for (size_t i = 0; i < SHIP_MODEL_EDGE_COUNT; i++) {
        uint8_t const a = SHIP_MODEL_EDGES[i][0];
        uint8_t const b = SHIP_MODEL_EDGES[i][1];
        scene_line(wx[a], wy[a], wz[a], wx[b], wy[b], wz[b], SHIP_MODEL_OUTLINE_COLOR);
    }
}
