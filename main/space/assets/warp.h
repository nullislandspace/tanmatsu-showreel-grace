#pragma once
// =====================================================================
//  Showreel asset  --  the warp effect (D-29)
// ---------------------------------------------------------------------
//  A ship jumping to hyperspace, as a pure function of time:
//
//    WARP_OUT  from t_warp the ship stretches along its flight line (x6),
//              thins (x0.3) and races away for WARP_STRETCH_SECS; then it
//              is gone, and a white flash -- a burst of short radial
//              lines -- marks where it vanished for WARP_FLASH_SECS.
//    WARP_IN   the same backwards, ending at t_warp with the ship in its
//              normal pose: flash, then a streak that shrinks into the
//              ship as it slows to its own speed.
//
//  The scene gives the pose the ship would have without the warp; this
//  says whether it is drawn and how, and draws the flash. No
//  transparency: the flash fades by darkening to the space backdrop.
// =====================================================================

#include <stdbool.h>
#include "xform.h"

typedef enum {
    WARP_OUT,
    WARP_IN,
} warp_dir_t;

#define WARP_STRETCH_SECS 0.4f
#define WARP_FLASH_SECS   0.25f

// The ship's pose at `t` for a warp at `t_warp`, from its normal pose
// `base`. Returns false while the ship is not there (gone after warping
// out, not yet arrived before warping in); `out` is then untouched.
bool warp_pose(xform_t const* base, float t, float t_warp, warp_dir_t dir, xform_t* out);

// Where the ship vanishes (WARP_OUT) or appears (WARP_IN), from its
// normal pose at the moment the stretch ends or begins.
vec3_t warp_point(xform_t const* base_at_flash, warp_dir_t dir);

// Draw the flash, if it is lit at `t`, centred on `at` (warp_point), for
// a ship `size` units across. `seed` varies the burst.
void warp_submit_flash(vec3_t at, float size, float t, float t_warp, warp_dir_t dir, unsigned seed);

// The flash's time window, for scheduling: [start, end).
void warp_flash_window(float t_warp, warp_dir_t dir, float* start, float* end);
