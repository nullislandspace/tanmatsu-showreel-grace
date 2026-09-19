// =====================================================================
//  Showreel  --  the second solar system (see system2.h)
// =====================================================================

#include "space/scenes/system2.h"
#include "camera.h"
#include "common/starfield.h"
#include "space/assets/planet.h"
#include "synthengine3d.h"

// The giant: far enough that the few hundred units a scene covers barely
// move it, big enough to fill a good part of a forward view.
#define GIANT_DIST   2600.0f
#define GIANT_RADIUS 650.0f
#define GIANT_SPIN   0.004f  // rad/s

void system2_init(void) {
    planet_init();
    starfield_init();
}

void system2_shutdown(void) {
    planet_shutdown();
}

void system2_light(void) {
    // From -x, a little up and ahead: the side the scenes' cameras mostly
    // look from, so the ships and the rock show their lit faces; the giant
    // (off to -x as well) is lit from beyond its outer edge.
    se_light_set(&(se_light_t){.x = -900.0f, .y = 250.0f, .z = -250.0f, .brightness = 0.95f, .two_sided = true});
}

void system2_submit_sky(float t) {
    mat3_t const sky = mat3_from_ypr(2.2f, -0.7f, 1.1f);
    starfield_submit_turned(&sky);
    // Ahead-left and a little up, fixed in the world: the ships fly -z.
    vec3_t const  dir   = v3_norm(v3(-0.55f, 0.18f, -1.0f));
    // Tilted poles, as a giant's would be.
    mat3_t const  tilt  = mat3_rot_z(0.35f);
    mat3_t const  spin  = mat3_rot_y(GIANT_SPIN * t);
    xform_t const giant = {mat3_mul(&tilt, &spin), v3_scale(dir, GIANT_DIST), GIANT_RADIUS};
    planet_submit(&giant, PLANET_GAS);
}
