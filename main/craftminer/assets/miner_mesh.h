#pragma once
// =====================================================================
//  CraftMiner  --  the miner's meshes (engine-free, under the mesh check)
// ---------------------------------------------------------------------
//  Our own blocky figure: a yellow hard hat with a lamp, a big brown
//  moustache, a red shirt, blue overalls, brown boots -- and a pickaxe.
//  One mesh per moving piece, each built round its joint so a pose is a
//  rotation about the origin (miner.c):
//
//    head   neck at the origin; the hat, its brim and the lamp on it
//    body   hips at the origin, shoulders at y = MINER_BODY_H
//    arm    shoulder at the origin, hanging down -y
//    leg    hip at the origin, hanging down -y
//    pick   held in the fist: the handle along +z, the head at its end
//
//  Model units are blocks, +y up, the figure facing +z (its right hand
//  on -x). About 2 blocks tall; miner.c scales it to 0.9.
// =====================================================================

#include "mesh.h"

typedef enum {
    MM_SKIN,
    MM_SHIRT,
    MM_OVERALLS,
    MM_BOOTS,
    MM_HAT,
    MM_LAMP,
    MM_HAIR,
    MM_FACE,
    MM_WOOD,
    MM_IRON,
    MM_COUNT
} miner_mat_t;

#define MINER_LEG_H      0.72f  // hip height
#define MINER_BODY_H     0.70f  // hips to shoulders
#define MINER_ARM_H      0.62f
#define MINER_HIP_X      0.12f     // each leg's hip off the middle
#define MINER_SHOULDER_X 0.36f     // each arm's shoulder off the middle
#define MINER_FIST_Y     (-0.56f)  // the fist, down the arm from the shoulder

void miner_build_head(mesh_t* m);
void miner_build_body(mesh_t* m);
void miner_build_arm(mesh_t* m);
void miner_build_leg(mesh_t* m);
void miner_build_pick(mesh_t* m);
