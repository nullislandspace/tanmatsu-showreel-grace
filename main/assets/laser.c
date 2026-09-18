// =====================================================================
//  Showreel asset  --  lasers (see laser.h)
// =====================================================================

#include "assets/laser.h"
#include "synthengine3d.h"

laser_style_t const LASER_STYLE_MARAUDER = {
    .duration = 0.12f,
    .range    = 80.0f,
    .argb     = LASER_RED,
};

laser_style_t const LASER_STYLE_PLAYER = {
    .duration = 0.12f,
    .range    = 80.0f,
    .argb     = LASER_BLUE,
};

void laser_submit_beam(vec3_t muzzle, vec3_t target, float t, float t_fire, laser_style_t const* style) {
    if (!laser_lit(t, t_fire, style)) return;
    vec3_t      d   = v3_sub(target, muzzle);
    float const len = v3_len(d);
    if (len < 1e-4f) return;
    if (len > style->range) d = v3_scale(d, style->range / len);
    vec3_t const end = v3_add(muzzle, d);
    scene_line(muzzle.x, muzzle.y, muzzle.z, end.x, end.y, end.z, style->argb);
}
