// =====================================================================
//  Showreel asset  --  laser bolts (see laser.h)
// =====================================================================

#include "assets/laser.h"
#include "synthengine3d.h"

laser_style_t const LASER_STYLE_MARAUDER = {
    .speed    = 60.0f,
    .length   = 2.0f,
    .lifetime = 1.2f,
    .argb     = LASER_RED,
};

void laser_submit_bolt(vec3_t muzzle, vec3_t dir, float age, laser_style_t const* style) {
    if (age < 0.0f || age > style->lifetime) return;
    // The streak's tail leaves the muzzle first: while it is still
    // growing out of the barrel it is shorter than its full length.
    float const  head = style->speed * age;
    float const  tail = head > style->length ? head - style->length : 0.0f;
    vec3_t const a    = v3_add(muzzle, v3_scale(dir, tail));
    vec3_t const b    = v3_add(muzzle, v3_scale(dir, head));
    scene_line(a.x, a.y, a.z, b.x, b.y, b.z, style->argb);
}
