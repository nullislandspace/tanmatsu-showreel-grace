// =====================================================================
//  Showreel asset  --  the title's 3D lettering (see title_text.h)
// =====================================================================

#include "assets/title_text.h"
#include "assets/texcache.h"
#include "assets/title_text_mesh.h"
#include "mesh_render.h"

// Per finish: face and side texture, and the flat colour each falls back
// to if its texture is missing.
static struct {
    char const* face;
    char const* side;
    uint32_t    face_argb, side_argb;
} const FINISH[] = {
    [TITLE_HERO]     = {"plate_brushed.png", "plate_riveted.png", 0xFFB0B3B8u, 0xFF94979Cu},
    [TITLE_MARAUDER] = {"marauder_green.png", "plate_gunmetal.png", 0xFF465E38u, 0xFF5C626Eu},
};

void title_line_make(title_line_t* line, char const* text, float height, float depth, title_finish_t finish) {
    mesh_init(&line->mesh);
    line->mesh.name = "title";
    line->width     = title_text_build(&line->mesh, text, height, depth);
    line->finish    = finish;
    texcache_get(FINISH[finish].face);
    texcache_get(FINISH[finish].side);
}

void title_line_free(title_line_t* line) {
    mesh_free(&line->mesh);
}

void title_line_submit(title_line_t const* line, xform_t const* x) {
    mesh_mat_t const mats[TITLE_MAT_COUNT] = {
        [TITLE_MAT_FACE] = {texcache_get(FINISH[line->finish].face), FINISH[line->finish].face_argb, 0},
        [TITLE_MAT_SIDE] = {texcache_get(FINISH[line->finish].side), FINISH[line->finish].side_argb, 0},
    };
    mesh_submit(&line->mesh, x, mats, TITLE_MAT_COUNT);
}
