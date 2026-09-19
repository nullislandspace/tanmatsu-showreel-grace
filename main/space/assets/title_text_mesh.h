#pragma once
// =====================================================================
//  Showreel asset  --  3D block lettering for the title (engine-free)
// ---------------------------------------------------------------------
//  A small stroke font: each glyph is one to three paths on a grid (cap
//  height 7, x-height 5, descender 2 units), drawn one unit wide with
//  mitred joints and extruded (mesh_stroke) -- chunky, chamfered, retro.
//  Only the glyphs the title needs exist: B S d e i l o p r s u w and
//  ':'; any other character becomes a space.
//
//  Model space of a line: the baseline along +x from x = 0, cap height
//  along +y, the letters' FRONT face at z = 0 facing -z (so the line
//  reads left to right from in front of it, as the default camera looks
//  down +z), the back face at z = depth.
//
//  Materials: TITLE_MAT_FACE (front and back), TITLE_MAT_SIDE.
// =====================================================================

#include "mesh.h"

enum {
    TITLE_MAT_FACE = 0,
    TITLE_MAT_SIDE,
    TITLE_MAT_COUNT,
};

// Append `text` to `m` with cap height `height` and extrusion `depth`
// (both in the mesh's units). Returns the line's width, from the left
// edge of the first glyph to the right edge of the last.
float title_text_build(mesh_t* m, char const* text, float height, float depth);

// The width title_text_build() would return, without building anything.
float title_text_width(char const* text, float height);
