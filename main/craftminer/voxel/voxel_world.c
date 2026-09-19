// =====================================================================
//  CraftMiner  --  the block world (see voxel_world.h)
// =====================================================================

#include "craftminer/voxel/voxel_world.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "xform.h"

static uint8_t* s_base;  // the generated terrain
static uint8_t* s_cur;   // the terrain plus the edits applied so far

static vox_edit_t const* s_edits;  // the list applied, and how much of it
static int               s_applied;

#define IDX(x, y, z) (((size_t)(z) * VOX_W + (size_t)(x)) * VOX_H + (size_t)(y))

static bool inside(int x, int y, int z) {
    return x >= 0 && x < VOX_W && y >= 0 && y < VOX_H && z >= 0 && z < VOX_D;
}

uint8_t voxel_block(int x, int y, int z) {
    if (y >= VOX_H) return VB_AIR;
    if (!inside(x, y, z)) return VB_STONE;
    return s_cur[IDX(x, y, z)];
}

uint8_t const* voxel_column(int x, int z) {
    if (x < 0 || x >= VOX_W || z < 0 || z >= VOX_D || !s_cur) return NULL;
    return &s_cur[IDX(x, 0, z)];
}

uint8_t voxel_base_block(int x, int y, int z) {
    if (y >= VOX_H) return VB_AIR;
    if (!inside(x, y, z)) return VB_STONE;
    return s_base[IDX(x, y, z)];
}

bool voxel_solid(uint8_t block) {
    return block != VB_AIR && block != VB_TORCH && block != VB_FLOWER_RED && block != VB_FLOWER_YELLOW &&
           block != VB_TALL_GRASS;
}

int voxel_ground(int x, int z) {
    for (int y = VOX_H - 1; y >= 0; y--) {
        if (voxel_solid(voxel_block(x, y, z))) return y + 1;
    }
    return 0;
}

// --- Generation -------------------------------------------------------------------

float voxel_noise2(float x, float z, float scale, unsigned seed) {
    float const fx = x / scale, fz = z / scale;
    int const   ix = (int)floorf(fx), iz = (int)floorf(fz);
    float const u = smoothstep(0.0f, 1.0f, fx - (float)ix), v = smoothstep(0.0f, 1.0f, fz - (float)iz);
#define LAT(a, b) hash01((a) * 7919 + (b) * 104729, seed)
    float const n0 = LAT(ix, iz) + (LAT(ix + 1, iz) - LAT(ix, iz)) * u;
    float const n1 = LAT(ix, iz + 1) + (LAT(ix + 1, iz + 1) - LAT(ix, iz + 1)) * u;
#undef LAT
    return n0 + (n1 - n0) * v;
}

static float dist2d(float x, float z, float cx, float cz) {
    return sqrtf((x - cx) * (x - cx) + (z - cz) * (z - cz));
}

// Distance from (x, z) to the rectangle [x0, x1] x [z0, z1] (0 inside).
static float dist_rect(float x, float z, float x0, float z0, float x1, float z1) {
    float const dx = fmaxf(fmaxf(x0 - x, x - x1), 0.0f);
    float const dz = fmaxf(fmaxf(z0 - z, z - z1), 0.0f);
    return sqrtf(dx * dx + dz * dz);
}

// The plateau behind the cliff: x from the face eastwards, z round the
// cliff's middle.
#define PLATEAU_X1 84
#define PLATEAU_Z0 (VOX_CLIFF_Z - 14)
#define PLATEAU_Z1 (VOX_CLIFF_Z + 14)

static bool on_plateau(int x, int z) {
    return x >= VOX_CLIFF_X && x <= PLATEAU_X1 && z >= PLATEAU_Z0 && z <= PLATEAU_Z1;
}

// The y of the top block of column (x, z).
static int terrain_height(int x, int z) {
    float const fx = (float)x + 0.5f, fz = (float)z + 0.5f;
    float h = 12.0f + 8.0f * (voxel_noise2(fx, fz, 30.0f, 1u) - 0.5f) + 4.0f * (voxel_noise2(fx, fz, 11.0f, 2u) - 0.5f);

    // The lake: a basin 3 blocks under the water.
    h = h + ((float)(VOX_WATER_LEVEL - 3) - h) * smoothstep(17.0f, 9.0f, dist2d(fx, fz, VOX_LAKE_X, VOX_LAKE_Z));

    // The meadow, the plot and the strip between the meadow and the cliff:
    // flat at the meadow's level, blending out over a few blocks.
    float const flat = fminf(fminf(dist2d(fx, fz, VOX_MEADOW_X, VOX_MEADOW_Z) - 9.0f,
                                   dist_rect(fx, fz, VOX_PLOT_X - 8, VOX_PLOT_Z - 8, VOX_PLOT_X + 8, VOX_PLOT_Z + 8)),
                             dist_rect(fx, fz, VOX_MEADOW_X, PLATEAU_Z0, VOX_CLIFF_X, PLATEAU_Z1));
    h                = h + ((float)VOX_MEADOW_Y - h) * smoothstep(6.0f, 0.0f, flat);

    // The plateau: sheer on its west side (the cliff), rounding off into
    // the hills to the east, north and south.
    if (x >= VOX_CLIFF_X) {
        float const edge = dist_rect(fx, fz, VOX_CLIFF_X, PLATEAU_Z0, PLATEAU_X1, PLATEAU_Z1);
        float const top  = (float)VOX_CLIFF_TOP - 1.2f * (voxel_noise2(fx, fz, 5.0f, 3u) - 0.5f);
        h                = fmaxf(h, h + (top - h) * smoothstep(7.0f, 0.0f, edge));
    }
    int hi = (int)lroundf(h);
    if (hi < 2) hi = 2;
    if (hi > VOX_H - 8) hi = VOX_H - 8;  // room for trees
    return hi;
}

static void put(int x, int y, int z, uint8_t b) {
    if (inside(x, y, z)) s_base[IDX(x, y, z)] = b;
}

static void fill_column(int x, int z, int h) {
    bool const shore = h <= VOX_WATER_LEVEL + 1;
    int const  dirt  = on_plateau(x, z) ? 1 : 3;  // the cliff face is stone
    for (int y = 0; y <= h; y++) {
        uint8_t b = VB_STONE;
        if (y == h)
            b = shore ? VB_SAND : VB_GRASS;
        else if (y > h - 1 - dirt)
            b = shore ? VB_SAND : VB_DIRT;
        put(x, y, z, b);
    }
    for (int y = h + 1; y <= VOX_WATER_LEVEL; y++) put(x, y, z, VB_WATER);
}

// Coal: small lumps round seeded points in the stone.
static void place_coal(void) {
    for (int cz = 2; cz < VOX_D; cz += 6) {
        for (int cx = 2; cx < VOX_W; cx += 6) {
            int const key = cx * 131 + cz;
            if (hash01(key, 20u) > 0.45f) continue;
            int const x = cx + (int)(hash01(key, 21u) * 4.0f);
            int const z = cz + (int)(hash01(key, 22u) * 4.0f);
            int const y = 2 + (int)(hash01(key, 23u) * 14.0f);
            int const n = 3 + (int)(hash01(key, 24u) * 4.0f);
            for (int k = 0; k < n; k++) {
                int const dx = (int)(hash01(key * 7 + k, 25u) * 3.0f) - 1;
                int const dy = (int)(hash01(key * 7 + k, 26u) * 3.0f) - 1;
                int const dz = (int)(hash01(key * 7 + k, 27u) * 3.0f) - 1;
                if (voxel_base_block(x + dx, y + dy, z + dz) == VB_STONE) put(x + dx, y + dy, z + dz, VB_COAL);
            }
        }
    }
    // A seam in the cliff face at head height, where the miner digs in.
    for (int dz = 0; dz < 2; dz++) {
        for (int dy = 0; dy < 2; dy++) put(VOX_CLIFF_X + 1, VOX_MEADOW_Y + 2 + dy, VOX_CLIFF_Z + 1 + dz, VB_COAL);
    }
}

// Where trees may not grow: the open places the scenes use, and the lake.
static bool clear_of_trees(int x, int z) {
    float const fx = (float)x + 0.5f, fz = (float)z + 0.5f;
    return dist2d(fx, fz, VOX_MEADOW_X, VOX_MEADOW_Z) < 14.0f ||
           dist_rect(fx, fz, VOX_PLOT_X - 8, VOX_PLOT_Z - 8, VOX_PLOT_X + 8, VOX_PLOT_Z + 8) < 3.0f ||
           dist_rect(fx, fz, VOX_MEADOW_X, PLATEAU_Z0, VOX_CLIFF_X + 2, PLATEAU_Z1) < 2.0f ||
           dist2d(fx, fz, VOX_LAKE_X, VOX_LAKE_Z) < 16.0f || x < 3 || z < 3 || x > VOX_W - 4 || z > VOX_D - 4;
}

static void place_tree(int x, int z, int key) {
    int const g = voxel_ground(x, z);  // (on s_base: s_cur is not set up yet)
    if (voxel_base_block(x, g - 1, z) != VB_GRASS) return;
    int const trunk = 4 + (int)(hash01(key, 31u) * 2.0f);
    int const top   = g + trunk;  // the first layer above the trunk
    for (int y = top - 3; y <= top + 1; y++) {
        int const r = y >= top ? 1 : 2;
        for (int dz = -r; dz <= r; dz++) {
            for (int dx = -r; dx <= r; dx++) {
                bool const corner = abs(dx) == r && abs(dz) == r;
                if (corner && (r == 1 || hash01(key * 31 + (y * 5 + dz) * 5 + dx, 32u) < 0.6f)) continue;
                if (voxel_base_block(x + dx, y, z + dz) == VB_AIR) put(x + dx, y, z + dz, VB_LEAVES);
            }
        }
    }
    for (int y = g; y < top; y++) put(x, y, z, VB_LOG);
}

static void place_trees(void) {
    for (int cz = 0; cz < VOX_D; cz += 6) {
        for (int cx = 0; cx < VOX_W; cx += 6) {
            int const key = cx * 257 + cz;
            if (hash01(key, 30u) > 0.38f) continue;
            int const x = cx + (int)(hash01(key, 33u) * 4.0f);
            int const z = cz + (int)(hash01(key, 34u) * 4.0f);
            if (!clear_of_trees(x, z)) place_tree(x, z, key);
        }
    }
}

// Plants on the grass: tall grass here and there, flowers rarer -- more
// of both on the meadow. Not where the cabin goes or at the cliff's foot.
static void place_plants(void) {
    for (int z = 1; z < VOX_D - 1; z++) {
        for (int x = 1; x < VOX_W - 1; x++) {
            int const g = voxel_ground(x, z);
            if (g >= VOX_H || voxel_base_block(x, g - 1, z) != VB_GRASS || voxel_base_block(x, g, z) != VB_AIR)
                continue;
            float const fx = (float)x + 0.5f, fz = (float)z + 0.5f;
            if (dist_rect(fx, fz, VOX_PLOT_X - 8, VOX_PLOT_Z - 8, VOX_PLOT_X + 8, VOX_PLOT_Z + 8) < 1.0f ||
                dist_rect(fx, fz, VOX_CLIFF_X - 6, PLATEAU_Z0, VOX_CLIFF_X, PLATEAU_Z1) < 1.0f)
                continue;
            bool const  meadow = dist2d(fx, fz, VOX_MEADOW_X, VOX_MEADOW_Z) < 14.0f;
            float const r      = hash01(z * VOX_W + x, 40u);
            float const grass = meadow ? 0.06f : 0.03f, red = grass + (meadow ? 0.018f : 0.004f),
                        yellow = red + (meadow ? 0.018f : 0.004f);
            if (r < grass)
                put(x, g, z, VB_TALL_GRASS);
            else if (r < red)
                put(x, g, z, VB_FLOWER_RED);
            else if (r < yellow)
                put(x, g, z, VB_FLOWER_YELLOW);
        }
    }
}

bool voxel_world_init(void) {
    if (s_base) return true;
    size_t const n = (size_t)VOX_W * VOX_H * VOX_D;
    s_base         = heap_caps_calloc(n, 1, MALLOC_CAP_SPIRAM);
    s_cur          = heap_caps_calloc(n, 1, MALLOC_CAP_SPIRAM);
    if (!s_base || !s_cur) {
        voxel_world_shutdown();
        return false;
    }
    uint8_t* const cur = s_cur;
    s_cur              = s_base;  // voxel_ground() reads s_cur while generating
    for (int z = 0; z < VOX_D; z++) {
        for (int x = 0; x < VOX_W; x++) fill_column(x, z, terrain_height(x, z));
    }
    place_coal();
    place_trees();
    place_plants();
    s_cur = cur;
    memcpy(s_cur, s_base, n);
    s_edits   = NULL;
    s_applied = 0;
    return true;
}

void voxel_world_shutdown(void) {
    heap_caps_free(s_base);
    heap_caps_free(s_cur);
    s_base = s_cur = NULL;
}

// --- Edits ---------------------------------------------------------------------------

// The chunk holding column (x, z) changed -- and so did a neighbouring
// chunk when the column is on its border (the face between them).
static void mark_one(bool* dirty, int x, int z) {
    if (x < 0 || z < 0 || x >= VOX_W || z >= VOX_D) return;
    dirty[(z / VOX_CHUNK) * VOX_CHUNKS_X + x / VOX_CHUNK] = true;
}

static void mark(bool* dirty, int x, int z) {
    if (!dirty) return;
    mark_one(dirty, x, z);
    mark_one(dirty, x - 1, z);
    mark_one(dirty, x + 1, z);
    mark_one(dirty, x, z - 1);
    mark_one(dirty, x, z + 1);
}

static void set_cell(vox_edit_t const* e, uint8_t b, bool* dirty) {
    if (!inside(e->x, e->y, e->z)) return;
    size_t const i = IDX(e->x, e->y, e->z);
    if (s_cur[i] == b) return;
    s_cur[i] = b;
    mark(dirty, e->x, e->z);
}

void voxel_world_sync(vox_edit_t const* edits, int n, float t, bool* dirty) {
    if (!s_cur) return;
    int k = 0;
    while (k < n && edits[k].t <= t) k++;
    if (edits != s_edits || k < s_applied) {
        // Back to the terrain first: undo everything applied so far.
        for (int i = 0; i < s_applied; i++) {
            vox_edit_t const* e = &s_edits[i];
            if (inside(e->x, e->y, e->z)) set_cell(e, s_base[IDX(e->x, e->y, e->z)], dirty);
        }
        s_edits   = edits;
        s_applied = 0;
    }
    for (int i = s_applied; i < k; i++) set_cell(&edits[i], edits[i].block, dirty);
    s_applied = k;
}
