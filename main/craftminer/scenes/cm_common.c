// =====================================================================
//  CraftMiner  --  what the scenes share (see cm_common.h)
// =====================================================================

#include "craftminer/scenes/cm_common.h"
#include <math.h>
#include "craftminer/assets/miner.h"
#include "craftminer/voxel/voxel_fx.h"
#include "craftminer/voxel/voxel_sky.h"
#include "synthengine3d.h"

#define STROKES_PER_SEC 1.8f  // the pickaxe, while a block is hit

void cm_init(void) {
    voxel_render_init();
    miner_init();
    voxel_fx_init();
}

void cm_shutdown(void) {
    voxel_fx_shutdown();
    miner_shutdown();
    voxel_render_shutdown();
}

// --- Time of day ---------------------------------------------------------------------

static uint32_t mix(uint32_t a, uint32_t b, float f) {
    uint32_t out = 0xFF000000u;
    for (int s = 0; s < 24; s += 8) {
        float const ca = (float)((a >> s) & 0xFF), cb = (float)((b >> s) & 0xFF);
        out |= (uint32_t)(ca + (cb - ca) * f + 0.5f) << s;
    }
    return out;
}

// Three keys: the afternoon (a high sun in the west-south-west, behind
// the cameras that look east), the sunset (low in the west, the sky
// orange) and the night (the sun under the world, the sky dark blue).
static struct {
    vec3_t   sun;
    uint32_t sky, fog;
    float    light;
} const KEY[3] = {
    {{-700.0f, 1100.0f, -300.0f}, VOX_SKY_ARGB, VOX_SKY_ARGB, 1.0f},
    {{-1000.0f, 90.0f, -250.0f}, 0xFFF2A878u, 0xFFE89468u, 0.6f},
    {{-500.0f, -700.0f, -250.0f}, 0xFF0C1430u, 0xFF101A34u, 0.12f},
};

cm_daylight_t cm_daylight(float day) {
    day           = clampf(day, 0.0f, 1.0f);
    int const   a = day >= 0.5f ? 0 : 1;  // the keys either side
    float const f = day >= 0.5f ? (1.0f - day) * 2.0f : (0.5f - day) * 2.0f;
    return (cm_daylight_t){
        .sun_dir  = v3_norm(v3_lerp(v3_norm(KEY[a].sun), v3_norm(KEY[a + 1].sun), f)),
        .sky_argb = mix(KEY[a].sky, KEY[a + 1].sky, f),
        .fog_argb = mix(KEY[a].fog, KEY[a + 1].fog, f),
        .light    = KEY[a].light + (KEY[a + 1].light - KEY[a].light) * f,
    };
}

void cm_light(cm_daylight_t const* d) {
    // The light has no colour, only a direction and a share: by day the
    // sun lights the tops; at night it is under the world, so every face
    // the camera sees gets only the fill, which drops to 15%.
    float const  b = 0.55f + 0.3f * (1.0f - d->light);
    vec3_t const p = v3_scale(d->sun_dir, 1000.0f);
    se_light_set(&(se_light_t){.x = p.x, .y = p.y, .z = p.z, .brightness = b, .two_sided = false});
}

vox_view_t cm_view(cm_daylight_t const* d) {
    vox_view_t v = VOX_VIEW_DEFAULT;
    v.fog_argb   = d->fog_argb;
    return v;
}

void cm_world_submit(float t, cm_daylight_t const* d, vox_edit_t const* edits, int n_edits, vox_view_t const* view) {
    voxel_sky_submit(t, d->sun_dir, d->fog_argb, d->light);
    voxel_render_submit(edits, n_edits, t, view);
}

float cm_ground(float x, float z) {
    return (float)voxel_ground((int)floorf(x), (int)floorf(z));
}

// --- Digging --------------------------------------------------------------------------

void cm_dig_edits(cm_dig_t const* digs, int n, vox_edit_t* out) {
    for (int i = 0; i < n; i++) out[i] = (vox_edit_t){digs[i].brk, digs[i].x, digs[i].y, digs[i].z, VB_AIR};
}

int cm_dig_current(cm_dig_t const* digs, int n, float t) {
    for (int i = 0; i < n; i++) {
        if (t >= digs[i].start && t < digs[i].brk) return i;
    }
    return -1;
}

float cm_dig_swing(cm_dig_t const* digs, int n, float t) {
    int const i = cm_dig_current(digs, n, t);
    return i < 0 ? 0.0f : miner_stroke(STROKES_PER_SEC * (t - digs[i].start));
}

void cm_dig_submit(cm_dig_t const* digs, int n, float t, vec3_t to) {
    for (int i = 0; i < n; i++) {
        cm_dig_t const* d = &digs[i];
        if (t < d->start) continue;
        if (t < d->brk) {
            voxel_fx_outline(d->x, d->y, d->z);
            voxel_fx_cracks(d->x, d->y, d->z, (t - d->start) / (d->brk - d->start), (unsigned)i + 1u);
            continue;
        }
        float const since = t - d->brk;
        voxel_fx_break(d->x, d->y, d->z, d->block, since, (unsigned)i + 1u);
        // The item falls onto whatever is under the hole now.
        int floor_y = d->y;
        while (floor_y > 0 && !voxel_solid(voxel_block(d->x, floor_y - 1, d->z))) floor_y--;
        voxel_fx_item(d->x, d->y, d->z, d->block, since, (float)floor_y, d->pick > 0.0f ? d->pick : 1.0e9f, to);
    }
}

// --- Building ---------------------------------------------------------------------------

void cm_place_edits(cm_place_t const* places, int n, vox_edit_t* out) {
    for (int i = 0; i < n; i++) {
        out[i] = (vox_edit_t){places[i].t + VOXEL_POP_SECS, places[i].x, places[i].y, places[i].z, places[i].block};
    }
}

void cm_place_submit(cm_place_t const* places, int n, float t) {
    for (int i = 0; i < n; i++) {
        voxel_fx_pop(places[i].x, places[i].y, places[i].z, places[i].block, t - places[i].t);
    }
}
