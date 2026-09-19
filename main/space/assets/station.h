#pragma once
// =====================================================================
//  Showreel asset  --  the 2001-style wheel station
// ---------------------------------------------------------------------
//  Hub, eight spokes and an outer ring with a band of windows; geometry
//  and dimensions in station_mesh.h. The station does not know about
//  time: a scene spins it by rotating its transform about the station's
//  own z axis (mat3_rot_z), and uses station_spoke_angle() to know where
//  the gaps are.
// =====================================================================

#include "space/assets/station_mesh.h"
#include "xform.h"

// Build the mesh and fetch the textures (texcache). Idempotent.
void station_init(void);
void station_shutdown(void);

// Draw the station posed by `x` (its model space: axis along z).
void station_submit(xform_t const* x);
