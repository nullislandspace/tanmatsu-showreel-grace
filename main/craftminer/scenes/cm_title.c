// =====================================================================
//  CraftMiner  --  cm_title (scene 1)
// ---------------------------------------------------------------------
//  "CraftMiner" in blocks, high over the meadow: a 7-row pixel font, two
//  blocks deep, "Craft" in grass blocks (the green strip on every
//  block's side), "Miner" in cobblestone. The letters pop into the sky
//  column by column, left to right, while the camera drifts past below
//  them in the afternoon sun, clouds going over.
// =====================================================================

#include <math.h>
#include <stddef.h>
#include <string.h>
#include "camera.h"
#include "craftminer/craftminer.h"
#include "craftminer/scenes/cm_common.h"

#define SCENE_SECS 10.0f
#define BUILD_T0   0.4f    // the first block pops in ...
#define BUILD_DT   0.011f  // ... and each next one this much later

// The letters: in the plane z = TITLE_Z (two deep), bottom row at
// TITLE_Y, starting at TITLE_X; the camera south of them looks north
// (+z), so +x runs left to right on the screen.
#define TITLE_Y 24
#define TITLE_Z 56
#define GLYPH_H 7

typedef struct {
    char        c;
    char const* rows[GLYPH_H];  // '#' a block; all rows the glyph's width
} glyph_t;

static glyph_t const FONT[] = {
    {'C', {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
    {'r', {"....", "....", "#.##", "##..", "#...", "#...", "#..."}},
    {'a', {"....", "....", ".##.", "...#", ".###", "#..#", ".###"}},
    {'f', {"..##", ".#..", "####", ".#..", ".#..", ".#..", ".#.."}},
    {'t', {".#..", ".#..", "###.", ".#..", ".#..", ".#.#", "..#."}},
    {'M', {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
    {'i', {"#", ".", "#", "#", "#", "#", "#"}},
    {'n', {"....", "....", "###.", "#..#", "#..#", "#..#", "#..#"}},
    {'e', {"....", "....", ".##.", "#..#", "####", "#...", ".###"}},
};

static char const TEXT[] = "CraftMiner";
#define SPLIT 5  // "Craft" | "Miner"

#define MAX_BLOCKS 400
static cm_place_t s_places[MAX_BLOCKS];
static vox_edit_t s_edits[MAX_BLOCKS];
static int        s_n;
static float      s_mid_x;     // the middle of the title, for the camera
static float      s_x0, s_x1;  // its ends
static float      s_eye_y;     // the camera's lowest height: clear of the ground along its path

static glyph_t const* glyph(char c) {
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++) {
        if (FONT[i].c == c) return &FONT[i];
    }
    return NULL;
}

// The letters' blocks, in the order they pop in: column by column from
// the left, bottom to top, front layer then back.
static void build_title(void) {
    int width = -1;
    for (int k = 0; TEXT[k]; k++) width += (int)strlen(glyph(TEXT[k])->rows[0]) + 1;
    int const x0 = VOX_MEADOW_X - width / 2;
    s_mid_x      = (float)x0 + 0.5f * (float)width;
    s_x0         = (float)x0;
    s_x1         = (float)(x0 + width);
    int col      = x0;
    s_n          = 0;
    for (int k = 0; TEXT[k]; k++) {
        glyph_t const* g = glyph(TEXT[k]);
        int const      w = (int)strlen(g->rows[0]);
        uint8_t const  b = k < SPLIT ? VB_GRASS : VB_COBBLE;
        for (int cx = 0; cx < w; cx++, col++) {
            for (int row = GLYPH_H - 1; row >= 0; row--) {
                if (g->rows[row][cx] != '#') continue;
                for (int dz = 0; dz < 2 && s_n < MAX_BLOCKS; dz++) {
                    s_places[s_n] = (cm_place_t){BUILD_T0 + BUILD_DT * (float)s_n, (uint8_t)col,
                                                 (uint8_t)(TITLE_Y + GLYPH_H - 1 - row), (uint8_t)(TITLE_Z + dz), b};
                    s_n++;
                }
            }
        }
        col++;  // the gap between letters
    }
    cm_place_edits(s_places, s_n, s_edits);
}

// The camera's path at s (0..1 through the scene), before it is lifted
// clear of the ground.
static vec3_t path(float s) {
    return v3(s_mid_x - 9.0f + 16.0f * s, 19.0f + 3.0f * s, (float)TITLE_Z - 36.0f + 4.0f * s);
}

static void title_init(char const* asset_dir) {
    (void)asset_dir;
    cm_init();
    build_title();
    // The path crosses hills and trees: lift it all to 2.5 blocks over
    // the highest ground (tree tops included) within a block of it.
    s_eye_y = 0.0f;
    for (int k = 0; k <= 20; k++) {
        vec3_t const p = path((float)k / 20.0f);
        for (int dz = -1; dz <= 1; dz++) {
            for (int dx = -1; dx <= 1; dx++) {
                float const g = cm_ground(p.x + (float)dx, p.z + (float)dz) + 2.5f;
                if (g - p.y > s_eye_y) s_eye_y = g - p.y;
            }
        }
    }
}

static void title_shutdown(void) {
    cm_shutdown();
}

static void title_camera(double td) {
    float const         t = (float)td;
    cm_daylight_t const d = cm_daylight(1.0f);
    cm_light(&d);
    // A slow drift left to right and up, below and in front of the
    // letters, looking up at them.
    float const  s   = smoothstep(0.0f, SCENE_SECS, t);
    vec3_t const eye = v3_add(path(s), v3(0.0f, s_eye_y, 0.0f));
    camera_look_at(eye, v3(s_mid_x + 2.0f * s, (float)TITLE_Y + 2.5f, (float)TITLE_Z), 0.0f);
}

static void title_submit(double td) {
    float const         t    = (float)td;
    cm_daylight_t const d    = cm_daylight(1.0f);
    vox_view_t          view = cm_view(&d);
    // The letters, ~36 blocks off, keep their textures.
    view.tex_box[0] = s_x0, view.tex_box[1] = (float)TITLE_Z;
    view.tex_box[2] = s_x1, view.tex_box[3] = (float)TITLE_Z + 2.0f;
    cm_world_submit(t, &d, s_edits, s_n, &view);
    cm_place_submit(s_places, s_n, t);
}

scene_def_t const SCENE_CM_TITLE = {
    .name     = "cm_title",
    .duration = SCENE_SECS,
    .init     = title_init,
    .shutdown = title_shutdown,
    .camera   = title_camera,
    .submit   = title_submit,
    .backdrop = {.sky_argb = VOX_SKY_ARGB, .ground_argb = VOX_SKY_ARGB, .ground = true},
    .quarter  = true,
};
