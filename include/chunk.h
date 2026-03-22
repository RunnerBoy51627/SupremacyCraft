#ifndef CHUNK_H
#define CHUNK_H

#include "platform_types.h"

#define CHUNK_SIZE      32
#define CHUNK_MAX_VERTS 40960

// ── Block types ───────────────────────────────────────────────────────────────
typedef enum {
    BLOCK_AIR         = 0,
    BLOCK_GRASS       = 1,
    BLOCK_DIRT        = 2,
    BLOCK_STONE       = 3,
    BLOCK_WOOD        = 4,   // log
    BLOCK_LEAF        = 5,
    BLOCK_PLANK       = 6,   // wooden planks
    BLOCK_CRAFT       = 8,   // crafting table
    BLOCK_TNT         = 9,
    BLOCK_FLINT_STEEL = 10,  // tool — never placed in world
} BlockType;

// ── Packed vertex: pos(12) + color(4) + uv(8) = 24 bytes ─────────────────────
struct ChunkVertex {
    float x, y, z;
    u8    r, g, b, a;
    float u, v;
};

// ── Chunk ─────────────────────────────────────────────────────────────────────
typedef struct {
    u8           blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]; // [x][y][z]
    int          worldX, worldZ;  // chunk grid position
    ChunkVertex* mesh;            // cached vertex buffer (NULL until built)
    int          meshVerts;       // number of vertices in mesh
    int          dirty;           // needs rebuild?
#ifndef _PC
    void*        dispList;        // GX display list buffer (GC only)
    u32          dispListSize;    // actual used size of display list
#endif
} Chunk;

// Generate block data for this chunk
void Chunk_Generate(Chunk* chunk, int worldX, int worldZ);

// Build cached mesh — needs neighbors for border face culling
void Chunk_BuildMesh(Chunk* chunk,
                     Chunk* nXN, Chunk* nXP,
                     Chunk* nZN, Chunk* nZP);

// Submit cached mesh to GX — fast, no block iteration
void Chunk_Render(Chunk* chunk);

// Block accessors
u8   Chunk_GetBlock(Chunk* chunk, int x, int y, int z);
int  block_face_tex_pub(u8 block, int face); // returns TEX_* id for a block face
bool Chunk_SetBlock(Chunk* chunk, int x, int y, int z, u8 block);

#endif