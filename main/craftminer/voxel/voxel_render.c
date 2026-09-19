// =====================================================================
//  CraftMiner  --  drawing the block world (see voxel_render.h)
// =====================================================================

#include "craftminer/voxel/voxel_render.h"
#include <math.h>
#include <stdio.h>
#include "camera.h"
#include "common/texcache.h"
#include "craftminer/voxel/voxel_mesh.h"
#include "mesh_render.h"

// Each material's texture, and the flat colour used if it fails to load.
static struct {
    char const* file;
    uint32_t    argb;
} const MAT_FILES[VM_COUNT] = {
    [VM_GRASS_TOP]     = {"craftminer/grass_top.png", 0xFF5C9634u},
    [VM_GRASS_SIDE]    = {"craftminer/grass_side.png", 0xFF7A5A3Au},
    [VM_DIRT]          = {"craftminer/dirt.png", 0xFF7A563Au},
    [VM_STONE]         = {"craftminer/stone.png", 0xFF7A7A7Cu},
    [VM_COBBLE]        = {"craftminer/cobble.png", 0xFF767676u},
    [VM_SAND]          = {"craftminer/sand.png", 0xFFD6C896u},
    [VM_WATER]         = {"craftminer/water.png", 0xFF3054C4u},
    [VM_LOG_SIDE]      = {"craftminer/log_side.png", 0xFF644C2Eu},
    [VM_LOG_TOP]       = {"craftminer/log_top.png", 0xFFA88452u},
    [VM_PLANKS]        = {"craftminer/planks.png", 0xFFA4804Eu},
    [VM_LEAVES]        = {"craftminer/leaves.png", 0xFF3A7026u},
    [VM_COAL]          = {"craftminer/coal_ore.png", 0xFF606062u},
    [VM_GLASS]         = {"craftminer/glass.png", 0xFFC8D8DEu},
    [VM_TORCH]         = {"craftminer/torch.png", 0xFF6E502Cu},
    [VM_FLOWER_RED]    = {"craftminer/flower_red.png", 0xFFD62824u},
    [VM_FLOWER_YELLOW] = {"craftminer/flower_yellow.png", 0xFFFAD428u},
    [VM_TALL_GRASS]    = {"craftminer/tall_grass.png", 0xFF5C9634u},
    [VM_LEAVES_FAST]   = {"craftminer/leaves_fast.png", 0xFF305C20u},
};

// A chunk's meshes: fancy (textured, see-through canopies, plants),
// fast (textured with opaque canopies, and in flat colours further off)
// and coarse (half resolution, flat). Each is built when first needed
// and again after an edit changed the chunk.
enum {
    LOD_FANCY,
    LOD_FAST,
    LOD_COARSE,
    LOD_COUNT
};

typedef struct {
    mesh_t lod[LOD_COUNT];
    bool   stale[LOD_COUNT];  // not built yet, or an edit changed the chunk since
    float  top;               // the highest block: the chunk's bounding box is 0..top
    char   name[LOD_COUNT][16];
} chunk_t;

static int        s_users;
static chunk_t    s_chunks[VOX_CHUNKS_X * VOX_CHUNKS_Z];
static mesh_mat_t s_tex_mats[VM_COUNT];
static uint32_t   s_mean[VM_COUNT];

// The meshers' input (voxel_mesh.h): a chunk and a border of one cell.
#define FINE_W   (VOX_CHUNK + 2)
#define FINE_H   (VOX_H + 2)
#define COARSE_W (VOX_CHUNK / 2 + 2)
#define COARSE_H (VOX_H / 2 + 2)
static uint8_t s_grid[FINE_W * FINE_W * FINE_H];

static bool cube(uint8_t b) {
    return voxel_solid(b) && b != VB_TORCH;
}

// The chunk's blocks, copied a column at a time (the world keeps each
// column's blocks together); rock under the world, sky over it.
static void fill_fine(int cx, int cz) {
    for (int dz = 0; dz < FINE_W; dz++) {
        for (int dx = 0; dx < FINE_W; dx++) {
            uint8_t*       col = &s_grid[(dz * FINE_W + dx) * FINE_H];
            uint8_t const* src = voxel_column(cx * VOX_CHUNK + dx - 1, cz * VOX_CHUNK + dz - 1);
            col[0]             = VB_STONE;
            for (int y = 0; y < VOX_H; y++) col[y + 1] = src ? src[y] : VB_STONE;
            col[VOX_H + 1] = VB_AIR;
        }
    }
}

// A cell of the half-resolution world: 2 x 2 x 2 blocks. Solid when at
// least half of them are, and then the commonest block of its upper
// layer (so a grassy slope stays green), else of its lower one.
static uint8_t coarse_cell(int x, int y, int z) {
    uint8_t b[2][4];
    int     solid = 0;
    for (int k = 0; k < 8; k++) {
        uint8_t const v   = voxel_block(2 * x + (k & 1), 2 * y + (k >> 2), 2 * z + ((k >> 1) & 1));
        b[k >> 2][k & 3]  = cube(v) ? v : VB_AIR;
        solid            += cube(v);
    }
    if (solid < 4) return VB_AIR;
    for (int layer = 1; layer >= 0; layer--) {
        int best = -1, best_n = 0;
        for (int i = 0; i < 4; i++) {
            if (b[layer][i] == VB_AIR) continue;
            int n = 0;
            for (int j = 0; j < 4; j++) n += b[layer][j] == b[layer][i];
            if (n > best_n) best_n = n, best = b[layer][i];
        }
        if (best >= 0) return (uint8_t)best;
    }
    return VB_AIR;
}

static void fill_coarse(int cx, int cz) {
    int const n = VOX_CHUNK / 2;
    for (int dz = 0; dz < COARSE_W; dz++) {
        for (int dx = 0; dx < COARSE_W; dx++) {
            uint8_t* col = &s_grid[(dz * COARSE_W + dx) * COARSE_H];
            for (int y = -1; y <= VOX_H / 2; y++) col[y + 1] = coarse_cell(cx * n + dx - 1, y, cz * n + dz - 1);
        }
    }
}

static void build(chunk_t* c, int cx, int cz, int lod) {
    mesh_free(&c->lod[lod]);
    mesh_init(&c->lod[lod]);
    c->lod[lod].name = c->name[lod];
    vox_grid_t g     = {s_grid, VOX_CHUNK, VOX_H, VOX_CHUNK, cx * VOX_CHUNK, cz * VOX_CHUNK, 1, false};
    if (lod == LOD_COARSE) {
        fill_coarse(cx, cz);
        g = (vox_grid_t){s_grid, VOX_CHUNK / 2, VOX_H / 2, VOX_CHUNK / 2, cx * VOX_CHUNK / 2, cz * VOX_CHUNK / 2,
                         2,      true};
    } else {
        fill_fine(cx, cz);
    }
    voxel_mesh_build(&c->lod[lod], &g, lod == LOD_FANCY ? VOX_MESH_FANCY : VOX_MESH_FAST);
    c->stale[lod] = false;
    if (lod == LOD_FAST) {  // the fast mesh has every block the fancy one has, bar plants
        c->top = 0.0f;
        for (int i = 0; i < c->lod[lod].vn; i++) c->top = fmaxf(c->top, c->lod[lod].v[i].y);
    }
}

bool voxel_render_init(void) {
    if (s_users++ > 0) return true;
    if (!voxel_world_init()) return false;
    for (int m = 0; m < VM_COUNT; m++) {
        se_texture_t const* tex = texcache_get(MAT_FILES[m].file);
        s_tex_mats[m]           = (mesh_mat_t){tex, MAT_FILES[m].argb, 0};
        s_mean[m]               = tex ? tex->mean_argb : MAT_FILES[m].argb;
    }
    for (int cz = 0; cz < VOX_CHUNKS_Z; cz++) {
        for (int cx = 0; cx < VOX_CHUNKS_X; cx++) {
            chunk_t*          c                 = &s_chunks[cz * VOX_CHUNKS_X + cx];
            static char const SUFFIX[LOD_COUNT] = {'n', 't', 'c'};
            for (int l = 0; l < LOD_COUNT; l++) {
                snprintf(c->name[l], sizeof(c->name[l]), "chunk%d_%d%c", cx, cz, SUFFIX[l]);
                mesh_init(&c->lod[l]);
                c->stale[l] = true;
            }
            // The fast mesh now: it sets the chunk's bounding box. The
            // others when first drawn.
            build(c, cx, cz, LOD_FAST);
        }
    }
    return true;
}

void voxel_render_shutdown(void) {
    if (s_users == 0 || --s_users > 0) return;
    for (int i = 0; i < VOX_CHUNKS_X * VOX_CHUNKS_Z; i++) {
        for (int l = 0; l < LOD_COUNT; l++) mesh_free(&s_chunks[i].lod[l]);
    }
    voxel_world_shutdown();
}

se_texture_t const* voxel_mat_tex(int mat) {
    return mat >= 0 && mat < VM_COUNT ? s_tex_mats[mat].tex : NULL;
}

uint32_t voxel_mat_argb(int mat) {
    return mat >= 0 && mat < VM_COUNT ? s_mean[mat] : 0xFFFF00FFu;
}

void voxel_cube_mats(uint8_t block, mesh_mat_t out[3]) {
    static vox_face_t const FACE[3] = {VF_TOP, VF_SIDE, VF_BOTTOM};
    for (int i = 0; i < 3; i++) {
        int const m = voxel_face_mat(block, FACE[i]);
        out[i]      = m >= 0 && m != VM_LEAVES ? s_tex_mats[m] : s_tex_mats[VM_LEAVES_FAST];
    }
}

static uint32_t mix_argb(uint32_t a, uint32_t b, float f) {
    uint32_t out = 0xFF000000u;
    for (int s = 0; s < 24; s += 8) {
        float const ca = (float)((a >> s) & 0xFF), cb = (float)((b >> s) & 0xFF);
        out |= (uint32_t)lroundf(ca + (cb - ca) * f) << s;
    }
    return out;
}

// Whether the box lo..hi is wholly outside the view: all eight corners
// beyond one of its planes (behind the near plane, or off one edge of
// the screen).
static bool outside_view(vec3_t lo, vec3_t hi, vec3_t eye, mat3_t const* b) {
    float const kx     = RENDER_HALF_W / RENDER_FOCAL_LEN;
    float const ku     = RENDER_HORIZON_Y / RENDER_FOCAL_LEN;
    float const kd     = ((float)DISPLAY_LOG_H - RENDER_HORIZON_Y) / RENDER_FOCAL_LEN;
    int         out[5] = {0};
    for (int i = 0; i < 8; i++) {
        vec3_t const p = v3(i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z);
        vec3_t const d = v3_sub(p, eye);
        float const  x = v3_dot(d, b->right), y = v3_dot(d, b->up), z = v3_dot(d, b->fwd);
        out[0] += z < RENDER_NEAR_CLIP_Z;
        out[1] += x > kx * z;
        out[2] += x < -kx * z;
        out[3] += y > ku * z;
        out[4] += y < -kd * z;
    }
    for (int k = 0; k < 5; k++) {
        if (out[k] == 8) return true;
    }
    return false;
}

void voxel_render_submit(vox_edit_t const* edits, int n, float t, vox_view_t const* view) {
    if (s_users == 0) return;
    bool dirty[VOX_CHUNKS_X * VOX_CHUNKS_Z] = {false};
    voxel_world_sync(edits, n, t, dirty);

    vec3_t const  eye   = camera_eye();
    mat3_t const  basis = camera_basis();
    xform_t const ident = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {0, 0, 0}, 1.0f};
    mesh_mat_t    flat[VM_COUNT];
    for (int cz = 0; cz < VOX_CHUNKS_Z; cz++) {
        for (int cx = 0; cx < VOX_CHUNKS_X; cx++) {
            chunk_t* c = &s_chunks[cz * VOX_CHUNKS_X + cx];
            if (dirty[cz * VOX_CHUNKS_X + cx]) {
                for (int l = 0; l < LOD_COUNT; l++) c->stale[l] = true;
                build(c, cx, cz, LOD_FAST);  // (the bounding box may have grown)
            }
            vec3_t const lo   = v3((float)(cx * VOX_CHUNK), 0.0f, (float)(cz * VOX_CHUNK));
            vec3_t const hi   = v3(lo.x + VOX_CHUNK, c->top + 1.0f, lo.z + VOX_CHUNK);  // +1: room for a new block
            // Distance from the eye to the nearest point of the box.
            float const  dx   = fmaxf(fmaxf(lo.x - eye.x, eye.x - hi.x), 0.0f);
            float const  dy   = fmaxf(fmaxf(lo.y - eye.y, eye.y - hi.y), 0.0f);
            float const  dz   = fmaxf(fmaxf(lo.z - eye.z, eye.z - hi.z), 0.0f);
            float const  dist = sqrtf(dx * dx + dy * dy + dz * dz);
            if (dist > view->draw_dist || outside_view(lo, hi, eye, &basis)) continue;
            bool const boxed = view->tex_box[2] > view->tex_box[0] && hi.x > view->tex_box[0] &&
                               lo.x < view->tex_box[2] && hi.z > view->tex_box[1] && lo.z < view->tex_box[3];
            int const lod = dist < view->fancy_dist             ? LOD_FANCY
                            : dist < view->coarse_dist || boxed ? LOD_FAST
                                                                : LOD_COARSE;
            if (c->stale[lod]) build(c, cx, cz, lod);
            if (dist < view->tex_dist || boxed) {
                mesh_submit(&c->lod[lod], &ident, s_tex_mats, VM_COUNT);
                continue;
            }
            float const f = smoothstep(view->fog0, view->fog1, dist);
            for (int m = 0; m < VM_COUNT; m++) flat[m] = (mesh_mat_t){NULL, mix_argb(s_mean[m], view->fog_argb, f), 0};
            mesh_submit(&c->lod[lod], &ident, flat, VM_COUNT);
        }
    }
}
