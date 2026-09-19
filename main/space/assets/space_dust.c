// =====================================================================
//  Showreel asset  --  space dust (see space_dust.h)
// =====================================================================

#include "space/assets/space_dust.h"
#include <math.h>
#include <stdbool.h>
#include "camera.h"
#include "synthengine3d.h"

#define DUST_N   160
#define DUST_BOX 24.0f  // units: the cloud round the camera, each way

static vec3_t   s_pos[DUST_N];  // in [0, DUST_BOX)^3, fixed in space
static uint32_t s_argb[DUST_N];
static bool     s_ready;

void space_dust_init(void) {
    if (s_ready) return;
    for (int i = 0; i < DUST_N; i++) {
        s_pos[i] = v3(DUST_BOX * hash01(i, 0xD057u), DUST_BOX * hash01(i, 0xD058u), DUST_BOX * hash01(i, 0xD059u));
        // Faint grey, a little warm or cool: dimmer than the stars.
        float const    b = 70.0f + 70.0f * hash01(i, 0xD05Au);
        float const    w = 0.9f + 0.2f * hash01(i, 0xD05Bu);
        uint32_t const r = (uint32_t)fminf(255.0f, b * w), g = (uint32_t)b, bl = (uint32_t)fminf(255.0f, b / w);
        s_argb[i] = 0xFF000000u | (r << 16) | (g << 8) | bl;
    }
    s_ready = true;
}

// The copy of coordinate p (in [0, box)) nearest to c.
static float wrap_near(float p, float c) {
    float const d = p - c;
    return c + d - DUST_BOX * floorf(d / DUST_BOX + 0.5f);
}

void space_dust_submit(vec3_t cam_vel, float streak) {
    if (!s_ready) return;
    vec3_t const eye   = camera_eye();
    // A moment `streak` ago the camera was v * streak further back, so the
    // mote -- fixed in space -- appeared v * streak further on: the streak
    // runs from where it is now towards the point the camera heads for.
    vec3_t const trail = v3_scale(cam_vel, streak);
    for (int i = 0; i < DUST_N; i++) {
        vec3_t const p = v3(wrap_near(s_pos[i].x, eye.x), wrap_near(s_pos[i].y, eye.y), wrap_near(s_pos[i].z, eye.z));
        if (streak > 0.0f) {
            vec3_t const q = v3_add(p, trail);
            scene_line(p.x, p.y, p.z, q.x, q.y, q.z, s_argb[i]);
        } else {
            scene_point(p.x, p.y, p.z, s_argb[i]);
        }
    }
}
