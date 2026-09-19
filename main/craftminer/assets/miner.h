#pragma once
// =====================================================================
//  CraftMiner  --  the miner (third person) and his arm (first person)
// ---------------------------------------------------------------------
//  Posed as a pure function of the scene's parameters (miner_pose_t), so
//  a scene computes the pose from t and submits it; nothing is kept
//  between frames. The meshes are miner_mesh.h's.
// =====================================================================

#include <stdbool.h>
#include "xform.h"

typedef enum {
    MINER_HOLD_NONE,
    MINER_HOLD_PICK,
    MINER_HOLD_BLOCK
} miner_hold_t;

typedef struct {
    float        walk;        // walk cycle, radians: legs and arms swing with sin(walk)
    float        stride;      // 0 standing still .. 1 walking
    float        swing;       // the tool arm: 0 at rest .. 1 raised (a stroke runs 1 -> 0)
    float        head_yaw;    // radians, + to his left
    float        head_pitch;  // radians, + looking down
    miner_hold_t hold;
    int          hold_block;  // MINER_HOLD_BLOCK: which block (voxel_world.h's vox_block_t)
} miner_pose_t;

// Load the textures and build the meshes (once; every scene that uses
// the miner calls it, and miner_shutdown() when done).
void miner_init(void);
void miner_shutdown(void);

// The miner standing at `root`: its origin between his feet on the
// ground, +z the way he faces. The figure is MINER_HEIGHT tall.
#define MINER_SCALE  0.9f
#define MINER_HEIGHT (2.04f * MINER_SCALE)
#define MINER_EYE    (1.62f)  // eye height, for a first-person camera (blocks)
void miner_submit(xform_t const* root, miner_pose_t const* pose);

// The swing of a mining stroke at `phase` (0..1 through one stroke):
// up quickly, down hard, a short rest. For miner_pose_t.swing.
float miner_stroke(float phase);

// First person: his right arm and what it holds, at the lower right of
// the current camera's view, swinging with `swing` (as in the pose).
// `bob` lifts it (walking). Call after the camera is set.
void miner_submit_fp_arm(float swing, miner_hold_t hold, int hold_block, float bob);
