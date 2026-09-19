// =====================================================================
//  Showreel asset  --  3D block lettering for the title (see title_text_mesh.h)
// =====================================================================

#include "space/assets/title_text_mesh.h"
#include <stddef.h>

#define CAP     7.0f  // grid units
#define GAP     1.0f  // between glyphs
#define STROKES 3

// A path on the glyph grid. Open ends are where the stroke visibly stops
// (its flat cap lies exactly there) or buried inside another stroke;
// mitred joints sit on the stroke's centre line, half a unit in.
typedef struct {
    int   n;
    bool  closed;
    float p[MESH_STROKE_MAX_PTS][2];
} glyph_path_t;

typedef struct {
    char         c;
    float        width;
    int          n;
    glyph_path_t s[STROKES];
} glyph_t;

static glyph_t const GLYPHS[] = {
    {'B',
     5.0f,
     2,
     {{7, false, {{0.5f, 0}, {0.5f, 6.5f}, {3.5f, 6.5f}, {4.5f, 5.5f}, {4.5f, 4.5f}, {3.5f, 3.5f}, {0.5f, 3.5f}}},
      {5, false, {{3.5f, 3.5f}, {4.5f, 2.5f}, {4.5f, 1.5f}, {3.5f, 0.5f}, {0.5f, 0.5f}}}}},
    {'S',
     5.0f,
     1,
     {{10,
       false,
       {{5, 6.5f},
        {1.5f, 6.5f},
        {0.5f, 5.5f},
        {0.5f, 4.5f},
        {1.5f, 3.5f},
        {3.5f, 3.5f},
        {4.5f, 2.5f},
        {4.5f, 1.5f},
        {3.5f, 0.5f},
        {0, 0.5f}}}}},
    {'d',
     4.0f,
     2,
     {{2, false, {{3.5f, 0}, {3.5f, 7}}},
      {6, false, {{3.5f, 4.5f}, {1.5f, 4.5f}, {0.5f, 3.5f}, {0.5f, 1.5f}, {1.5f, 0.5f}, {3.5f, 0.5f}}}}},
    {'e',
     4.0f,
     1,
     {{9,
       false,
       {{4, 0.5f},
        {1.5f, 0.5f},
        {0.5f, 1.5f},
        {0.5f, 3.5f},
        {1.5f, 4.5f},
        {2.5f, 4.5f},
        {3.5f, 3.5f},
        {3.5f, 2.5f},
        {0.5f, 2.5f}}}}},
    {'i', 1.0f, 2, {{2, false, {{0.5f, 0}, {0.5f, 5}}}, {2, false, {{0.5f, 6}, {0.5f, 7}}}}},
    {'l', 2.0f, 1, {{4, false, {{0.5f, 7}, {0.5f, 1.5f}, {1.5f, 0.5f}, {2, 0.5f}}}}},
    {'o',
     4.0f,
     1,
     {{8,
       true,
       {{1.5f, 0.5f},
        {2.5f, 0.5f},
        {3.5f, 1.5f},
        {3.5f, 3.5f},
        {2.5f, 4.5f},
        {1.5f, 4.5f},
        {0.5f, 3.5f},
        {0.5f, 1.5f}}}}},
    {'p',
     4.0f,
     2,
     {{2, false, {{0.5f, -2}, {0.5f, 5}}},
      {6, false, {{0.5f, 4.5f}, {2.5f, 4.5f}, {3.5f, 3.5f}, {3.5f, 1.5f}, {2.5f, 0.5f}, {0.5f, 0.5f}}}}},
    {'r', 3.5f, 2, {{2, false, {{0.5f, 0}, {0.5f, 5}}}, {3, false, {{0.5f, 3.5f}, {1.5f, 4.5f}, {3.5f, 4.5f}}}}},
    {'s',
     4.0f,
     1,
     {{8,
       false,
       {{4, 4.5f}, {1.5f, 4.5f}, {0.5f, 3.5f}, {1.5f, 2.5f}, {2.5f, 2.5f}, {3.5f, 1.5f}, {2.5f, 0.5f}, {0, 0.5f}}}}},
    {'u',
     4.0f,
     2,
     {{4, false, {{0.5f, 5}, {0.5f, 1.5f}, {1.5f, 0.5f}, {3.5f, 0.5f}}}, {2, false, {{3.5f, 0}, {3.5f, 5}}}}},
    {'w',
     5.0f,
     2,
     {{7, false, {{0.5f, 5}, {0.5f, 1.5f}, {1.5f, 0.5f}, {2.5f, 1.5f}, {3.5f, 0.5f}, {4.5f, 1.5f}, {4.5f, 5}}},
      {2, false, {{2.5f, 1.5f}, {2.5f, 3.5f}}}}},
    {':', 1.0f, 2, {{2, false, {{0.5f, 0}, {0.5f, 1}}}, {2, false, {{0.5f, 3}, {0.5f, 4}}}}},
};
#define GLYPH_N ((int)(sizeof(GLYPHS) / sizeof(GLYPHS[0])))
#define SPACE   3.0f

static glyph_t const* glyph(char c) {
    for (int i = 0; i < GLYPH_N; i++) {
        if (GLYPHS[i].c == c) return &GLYPHS[i];
    }
    return NULL;
}

static float layout(mesh_t* m, char const* text, float height, float depth) {
    float const u = height / CAP;  // model units per grid unit
    float       x = 0.0f;          // pen, in grid units
    float       w = 0.0f;
    for (char const* c = text; *c; c++) {
        glyph_t const* g = glyph(*c);
        if (g == NULL) {
            x += SPACE + GAP;
            continue;
        }
        if (m != NULL) {
            for (int k = 0; k < g->n; k++) {
                glyph_path_t const* s = &g->s[k];
                float               p[MESH_STROKE_MAX_PTS][2];
                for (int i = 0; i < s->n; i++) {
                    p[i][0] = (x + s->p[i][0]) * u;
                    p[i][1] = s->p[i][1] * u;
                }
                // One texture repeat per cap height.
                mesh_stroke(m, s->n, (float const(*)[2])p, s->closed, u, 0.0f, depth, TITLE_MAT_FACE, TITLE_MAT_SIDE,
                            height);
            }
        }
        w  = x + g->width;
        x += g->width + GAP;
    }
    return w * u;
}

float title_text_build(mesh_t* m, char const* text, float height, float depth) {
    return layout(m, text, height, depth);
}

float title_text_width(char const* text, float height) {
    return layout(NULL, text, height, 0.0f);
}
