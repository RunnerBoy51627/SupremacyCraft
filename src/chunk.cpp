#include "chunk.h"
#include "camera.h"
#include "textures.h"
#include "atlas_regions.h"
#include <gccore.h>
#include <math.h>
#include <malloc.h>
#include <string.h>

// ── Face shading ──────────────────────────────────────────────────────────────
// Gives directional lighting feel without actual lighting
#define SHADE_TOP    255   // brightest — faces the sky
#define SHADE_NORTH  210   // -Z
#define SHADE_SOUTH  210   // +Z
#define SHADE_EAST   180   // +X
#define SHADE_WEST   180   // -X
#define SHADE_BOTTOM 130   // darkest — faces the ground

// ── Block queries ─────────────────────────────────────────────────────────────

u8 Chunk_GetBlock(Chunk* chunk, int x, int y, int z) {
    if (!chunk) return BLOCK_AIR;
    if (x < 0 || x >= CHUNK_SIZE ||
        y < 0 || y >= CHUNK_SIZE ||
        z < 0 || z >= CHUNK_SIZE) return BLOCK_AIR;
    return chunk->blocks[x][y][z];
}

bool Chunk_SetBlock(Chunk* chunk, int x, int y, int z, u8 block) {
    if (x < 0 || x >= CHUNK_SIZE ||
        y < 0 || y >= CHUNK_SIZE ||
        z < 0 || z >= CHUNK_SIZE) return false;
    chunk->blocks[x][y][z] = block;
    chunk->dirty = 1;
    return true;
}

// Get block accounting for chunk borders
static u8 get_block(Chunk* c, int x, int y, int z,
                    Chunk* nXN, Chunk* nXP, Chunk* nZN, Chunk* nZP) {
    if (y < 0)           return BLOCK_STONE; // below world = solid, no bottom face drawn
    if (y >= CHUNK_SIZE) return BLOCK_AIR;   // above world = air
    if (x < 0)           return Chunk_GetBlock(nXN, x + CHUNK_SIZE, y, z);
    if (x >= CHUNK_SIZE) return Chunk_GetBlock(nXP, x - CHUNK_SIZE, y, z);
    if (z < 0)           return Chunk_GetBlock(nZN, x, y, z + CHUNK_SIZE);
    if (z >= CHUNK_SIZE) return Chunk_GetBlock(nZP, x, y, z - CHUNK_SIZE);
    return Chunk_GetBlock(c, x, y, z);
}

// ── Texture lookup ────────────────────────────────────────────────────────────
// Face indices: 0=top 1=bottom 2=north(-Z) 3=south(+Z) 4=west(-X) 5=east(+X)

static int block_face_tex(u8 block, int face) {
    switch ((BlockType)block) {
        case BLOCK_GRASS:
            if (face == 0) return TEX_GRASS_TOP;   // green top
            if (face == 1) return TEX_DIRT;         // dirt bottom
            return TEX_GRASS_SIDE;                  // side has green strip
        case BLOCK_DIRT:   return TEX_DIRT;
        case BLOCK_STONE:  return TEX_STONE;
        case BLOCK_WOOD:   return TEX_WOOD;
        case BLOCK_LEAF:   return TEX_LEAVES;
        default:           return TEX_DIRT;
    }
}

// ── Heightmap ─────────────────────────────────────────────────────────────────

static float hash(int x, int z) {
    int n = x * 1619 + z * 31337;
    n = (n << 13) ^ n;
    return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff)
                  / 1073741824.0f;
}

static float noise(float x, float z) {
    int ix = (int)floorf(x), iz = (int)floorf(z);
    float fx = x - ix, fz = z - iz;
    // Smoothstep interpolation
    float ux = fx * fx * (3 - 2 * fx);
    float uz = fz * fz * (3 - 2 * fz);
    return hash(ix,   iz)   * (1-ux) * (1-uz)
         + hash(ix+1, iz)   *    ux  * (1-uz)
         + hash(ix,   iz+1) * (1-ux) *    uz
         + hash(ix+1, iz+1) *    ux  *    uz;
}

static int terrain_height(int wx, int wz, int lx, int lz) {
    float fx = (float)(wx * CHUNK_SIZE + lx);
    float fz = (float)(wz * CHUNK_SIZE + lz);
    // 3 octaves — large hills + medium bumps + small detail
    float h = noise(fx * 0.03f, fz * 0.03f) * 4.0f
            + noise(fx * 0.08f, fz * 0.08f) * 2.0f
            + noise(fx * 0.20f, fz * 0.20f) * 1.0f;
    int height = 18 + (int)floorf(h);
    if (height < 12) height = 12;
    if (height > 26) height = 26;
    return height;
}

// ── Generation ────────────────────────────────────────────────────────────────

static void place_tree(Chunk* chunk, int x, int y, int z) {
    // Trunk: 4-5 blocks tall
    int trunk = 4;
    for (int i = 1; i <= trunk; i++) {
        if (y + i < CHUNK_SIZE)
            chunk->blocks[x][y+i][z] = BLOCK_WOOD;
    }

    // Leaves: sphere-ish shape around top of trunk
    int top = y + trunk;
    for (int ly = -1; ly <= 2; ly++)
    for (int lx = -2; lx <= 2; lx++)
    for (int lz = -2; lz <= 2; lz++) {
        // Skip far corners to round it off
        if (abs(lx) == 2 && abs(lz) == 2) continue;
        if (ly == 2 && (abs(lx) + abs(lz)) > 2) continue;
        int bx = x+lx, by = top+ly, bz = z+lz;
        if (bx < 0 || bx >= CHUNK_SIZE) continue;
        if (by < 0 || by >= CHUNK_SIZE) continue;
        if (bz < 0 || bz >= CHUNK_SIZE) continue;
        if (chunk->blocks[bx][by][bz] == BLOCK_AIR)
            chunk->blocks[bx][by][bz] = BLOCK_LEAF;
    }
}

void Chunk_Generate(Chunk* chunk, int worldX, int worldZ) {
    chunk->worldX  = worldX;
    chunk->worldZ  = worldZ;
    chunk->dirty   = 1;
    chunk->mesh    = NULL;
    chunk->meshVerts = 0;

    // Zero all blocks to AIR
    memset(chunk->blocks, BLOCK_AIR, sizeof(chunk->blocks));

    // Fill terrain
    for (int x = 0; x < CHUNK_SIZE; x++)
    for (int z = 0; z < CHUNK_SIZE; z++) {
        int h = terrain_height(worldX, worldZ, x, z);
        for (int y = 0; y <= h && y < CHUNK_SIZE; y++) {
            if      (y == h)     chunk->blocks[x][y][z] = BLOCK_GRASS;
            else if (y >= h - 4) chunk->blocks[x][y][z] = BLOCK_DIRT;
            else                 chunk->blocks[x][y][z] = BLOCK_STONE;
        }
    }

    // Place trees — keep 2 blocks from edge so leaves don't get clipped
    for (int x = 2; x < CHUNK_SIZE - 2; x++)
    for (int z = 2; z < CHUNK_SIZE - 2; z++) {
        int h = terrain_height(worldX, worldZ, x, z);
        if (h < 1 || chunk->blocks[x][h][z] != BLOCK_GRASS) continue;
        // Deterministic random per world position
        unsigned int seed = (unsigned int)((worldX * CHUNK_SIZE + x) * 73856093)
                          ^ (unsigned int)((worldZ * CHUNK_SIZE + z) * 19349663);
        seed ^= (seed >> 13); seed *= 1376312589u;
        if ((seed % 60) != 0) continue; // ~1 in 60 grass blocks
        place_tree(chunk, x, h, z);
    }
}

// ── Mesh building ─────────────────────────────────────────────────────────────

static inline void push_quad(ChunkVertex* buf, int* n,
    float x0, float y0, float z0,
    float x1, float y1, float z1,
    float x2, float y2, float z2,
    float x3, float y3, float z3,
    u8 shade, float u0, float v0, float u1, float v1)
{
    if (*n + 4 > CHUNK_MAX_VERTS) return; // safety guard
    ChunkVertex* v = buf + *n;
    v[0] = {x0,y0,z0, shade,shade,shade,255, u0,v0};
    v[1] = {x1,y1,z1, shade,shade,shade,255, u1,v0};
    v[2] = {x2,y2,z2, shade,shade,shade,255, u1,v1};
    v[3] = {x3,y3,z3, shade,shade,shade,255, u0,v1};
    *n += 4;
}

void Chunk_BuildMesh(Chunk* chunk,
                     Chunk* nXN, Chunk* nXP,
                     Chunk* nZN, Chunk* nZP)
{
    if (!chunk->dirty) return;

    if (!chunk->mesh)
        chunk->mesh = (ChunkVertex*)memalign(32, sizeof(ChunkVertex) * CHUNK_MAX_VERTS);
    if (!chunk->mesh) return; // out of memory

    int n = 0;

    for (int x = 0; x < CHUNK_SIZE; x++)
    for (int y = 0; y < CHUNK_SIZE; y++)
    for (int z = 0; z < CHUNK_SIZE; z++) {
        u8 block = chunk->blocks[x][y][z];
        if (block == BLOCK_AIR) continue;

        float fx = (float)x, fy = (float)y, fz = (float)z;

        // Get UV region for each face
        #define UV(face) \
            const TexRegion* _r = Tex_GetRegion(block_face_tex(block, face));

        // Top face (+Y) — only if block above is air
        if (get_block(chunk, x, y+1, z, nXN,nXP,nZN,nZP) == BLOCK_AIR) {
            UV(0);
            push_quad(chunk->mesh, &n,
                fx,   fy+1, fz+1,
                fx+1, fy+1, fz+1,
                fx+1, fy+1, fz,
                fx,   fy+1, fz,
                SHADE_TOP, _r->u0,_r->v0, _r->u1,_r->v1);
        }
        // Bottom face (-Y)
        if (get_block(chunk, x, y-1, z, nXN,nXP,nZN,nZP) == BLOCK_AIR) {
            UV(1);
            push_quad(chunk->mesh, &n,
                fx,   fy, fz,
                fx+1, fy, fz,
                fx+1, fy, fz+1,
                fx,   fy, fz+1,
                SHADE_BOTTOM, _r->u0,_r->v0, _r->u1,_r->v1);
        }
        // North face (-Z)
        if (get_block(chunk, x, y, z-1, nXN,nXP,nZN,nZP) == BLOCK_AIR) {
            UV(2);
            push_quad(chunk->mesh, &n,
                fx+1, fy+1, fz,
                fx,   fy+1, fz,
                fx,   fy,   fz,
                fx+1, fy,   fz,
                SHADE_NORTH, _r->u0,_r->v0, _r->u1,_r->v1);
        }
        // South face (+Z)
        if (get_block(chunk, x, y, z+1, nXN,nXP,nZN,nZP) == BLOCK_AIR) {
            UV(3);
            push_quad(chunk->mesh, &n,
                fx,   fy+1, fz+1,
                fx+1, fy+1, fz+1,
                fx+1, fy,   fz+1,
                fx,   fy,   fz+1,
                SHADE_SOUTH, _r->u0,_r->v0, _r->u1,_r->v1);
        }
        // West face (-X)
        if (get_block(chunk, x-1, y, z, nXN,nXP,nZN,nZP) == BLOCK_AIR) {
            UV(4);
            push_quad(chunk->mesh, &n,
                fx, fy+1, fz,
                fx, fy+1, fz+1,
                fx, fy,   fz+1,
                fx, fy,   fz,
                SHADE_WEST, _r->u0,_r->v0, _r->u1,_r->v1);
        }
        // East face (+X)
        if (get_block(chunk, x+1, y, z, nXN,nXP,nZN,nZP) == BLOCK_AIR) {
            UV(5);
            push_quad(chunk->mesh, &n,
                fx+1, fy+1, fz+1,
                fx+1, fy+1, fz,
                fx+1, fy,   fz,
                fx+1, fy,   fz+1,
                SHADE_EAST, _r->u0,_r->v0, _r->u1,_r->v1);
        }
        #undef UV
    }

    chunk->meshVerts = n;
    DCFlushRange(chunk->mesh, sizeof(ChunkVertex) * n);
    chunk->dirty = 0;
}

// ── Render ────────────────────────────────────────────────────────────────────

void Chunk_Render(Chunk* chunk) {
    if (!chunk->mesh || chunk->meshVerts == 0) return;

    // World transform: translate to chunk's position
    Mtx model, mv;
    guMtxIdentity(model);
    guMtxTransApply(model, model,
        (float)(chunk->worldX * CHUNK_SIZE),
        0.0f,
        (float)(chunk->worldZ * CHUNK_SIZE));
    guMtxConcat(g_viewMatrix, model, mv);
    GX_LoadPosMtxImm(mv, GX_PNMTX0);

    GX_SetCullMode(GX_CULL_NONE);
    Tex_BindAtlas();

    GX_Begin(GX_QUADS, GX_VTXFMT0, chunk->meshVerts);
    for (int i = 0; i < chunk->meshVerts; i++) {
        const ChunkVertex& v = chunk->mesh[i];
        GX_Position3f32(v.x, v.y, v.z);
        GX_Color4u8(v.r, v.g, v.b, v.a);
        GX_TexCoord2f32(v.u, v.v);
    }
    GX_End();

    GX_SetCullMode(GX_CULL_BACK);
}