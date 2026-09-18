// =====================================================================
//  Showreel  --  scene camera helpers (see camera.h)
// =====================================================================

#include "camera.h"
#include "synthengine3d.h"

void camera_look_at(vec3_t eye, vec3_t target, float roll) {
    float yaw, pitch;
    look_at_angles(eye, target, &yaw, &pitch);
    render_set_camera_6dof(eye.x, eye.y, eye.z, yaw, pitch, roll);
}

vec3_t camera_eye(void) {
    render_camera_t const c = render_camera();
    return v3(c.x, c.y, c.z);
}
