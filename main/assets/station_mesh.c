// =====================================================================
//  Showreel asset  --  the station's geometry (see station_mesh.h)
// =====================================================================

#include "assets/station_mesh.h"

#define HUB_SIDES   16
#define RING_SEGS   48
#define DOCK_R      1.2f
#define DOCK_LEN    1.0f
#define DOCK_SIDES  12
// Model units per texture repeat.
#define HULL_REPEAT 4.0f
// The ring walls map exactly one repeat across their depth, so the
// texture's window band (rows 26-37 of 64) sits in the middle of them.
#define RING_REPEAT (2.0f * STATION_RING_HALF_Z)

void station_build_mesh(mesh_t* m) {
    mesh_init(m);
    m->name = "station";

    // Hub, capped both ends.
    mesh_cylinder(m, STATION_HUB_R, -STATION_HUB_HALF_Z, STATION_HUB_HALF_Z, HUB_SIDES, true, true, STATION_MAT_HULL,
                  STATION_MAT_HULL, HULL_REPEAT);

    // Docking port: a short, narrower drum on the hub's +z face, its mouth
    // dark. (Closed at both ends; the inner cap sits flush inside the hub.)
    int first = m->vn;
    mesh_cylinder(m, DOCK_R, 0.0f, DOCK_LEN, DOCK_SIDES, true, true, STATION_MAT_HULL, STATION_MAT_DOCK, HULL_REPEAT);
    xform_t const to_face = {mat3_from_ypr(0.0f, 0.0f, 0.0f), v3(0.0f, 0.0f, STATION_HUB_HALF_Z), 1.0f};
    mesh_transform_from(m, first, &to_face);

    // Spokes: square beams from just inside the hub to just inside the
    // ring's inner wall, so neither joint shows a gap.
    for (int k = 0; k < STATION_SPOKES; k++) {
        first = m->vn;
        mesh_box(m, v3(-STATION_SPOKE_HALF, STATION_HUB_R - 0.2f, -STATION_SPOKE_HALF),
                 v3(STATION_SPOKE_HALF, STATION_RING_R_IN + 0.2f, STATION_SPOKE_HALF), STATION_MAT_SPOKE, 2.0f);
        // Built along +y; turn it to angle k (+y sits at angle pi/2).
        xform_t const rot = {mat3_rot_z(station_spoke_angle(k) - 1.5707963f), v3(0.0f, 0.0f, 0.0f), 1.0f};
        mesh_transform_from(m, first, &rot);
    }

    // The ring, built at z = 0 .. depth (so the wall texture's v runs 0..1
    // across it), then centred on the wheel plane.
    first = m->vn;
    mesh_ring(m, STATION_RING_R_IN, STATION_RING_R_OUT, 0.0f, 2.0f * STATION_RING_HALF_Z, RING_SEGS, STATION_MAT_RING,
              STATION_MAT_RING, STATION_MAT_HULL, RING_REPEAT);
    xform_t const centre = {mat3_from_ypr(0.0f, 0.0f, 0.0f), v3(0.0f, 0.0f, -STATION_RING_HALF_Z), 1.0f};
    mesh_transform_from(m, first, &centre);
}
