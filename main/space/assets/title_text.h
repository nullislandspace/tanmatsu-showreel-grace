#pragma once
// =====================================================================
//  Showreel asset  --  the title's 3D lettering
// ---------------------------------------------------------------------
//  One line of block letters (title_text_mesh.h for the font and its
//  model space) in one of two finishes (D-29):
//
//    TITLE_HERO      the hero ship's plates: brushed steel faces,
//                    riveted sides
//    TITLE_MARAUDER  the green marauder's livery on the faces, gunmetal
//                    sides
// =====================================================================

#include "mesh.h"
#include "xform.h"

typedef enum {
    TITLE_HERO,
    TITLE_MARAUDER,
} title_finish_t;

typedef struct {
    mesh_t         mesh;
    float          width;  // model units
    title_finish_t finish;
} title_line_t;

// Build a line (cap height `height`, extrusion `depth`) and load its
// textures (texcache). Free it with title_line_free().
void title_line_make(title_line_t* line, char const* text, float height, float depth, title_finish_t finish);
void title_line_free(title_line_t* line);

// Draw it posed by `x`. Call after the scene's camera is set.
void title_line_submit(title_line_t const* line, xform_t const* x);
