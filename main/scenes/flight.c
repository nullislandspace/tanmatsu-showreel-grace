// =====================================================================
//  Showreel  --  ships flying along paths (see flight.h)
// =====================================================================

#include "scenes/flight.h"

xform_t flight_pose(path_t const* path, float t, float scale, float extra_roll) {
    vec3_t const v     = path_vel(path, t);
    vec3_t const f     = v3_norm(v);
    float const  h     = 0.05f;
    vec3_t const acc   = v3_scale(v3_sub(path_vel(path, t + h), path_vel(path, t - h)), 0.5f / h);
    vec3_t const right = v3_norm(v3_cross(v3(0.0f, 1.0f, 0.0f), f));
    // Turning right (acceleration towards +right) dips the right wing:
    // negative roll in this axis system (xform.h).
    float const  bank  = clampf(-FLIGHT_BANK_PER_ACC * v3_dot(acc, right), -FLIGHT_BANK_MAX, FLIGHT_BANK_MAX);
    return (xform_t){mat3_from_fwd_up(f, v3(0.0f, 1.0f, 0.0f), bank + extra_roll), path_pos(path, t), scale};
}

xform_t flight_pose_line(vec3_t at_t0, vec3_t vel, float t0, float t, float scale, float roll) {
    return (xform_t){mat3_from_fwd_up(v3_norm(vel), v3(0.0f, 1.0f, 0.0f), roll), v3_add(at_t0, v3_scale(vel, t - t0)),
                     scale};
}
