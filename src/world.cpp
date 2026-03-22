#include "world.h"
#include "chunk.h"

// Get chunk pointer or NULL if out of bounds
static Chunk* get_chunk(World* world, int cx, int cz) {
    if (cx < 0 || cx >= WORLD_W || cz < 0 || cz >= WORLD_D) return NULL;
    return &world->chunks[cx][cz];
}

static void get_neighbors(World* world, int cx, int cz,
                           Chunk** nXN, Chunk** nXP,
                           Chunk** nZN, Chunk** nZP) {
    *nXN = get_chunk(world, cx-1, cz);
    *nXP = get_chunk(world, cx+1, cz);
    *nZN = get_chunk(world, cx, cz-1);
    *nZP = get_chunk(world, cx, cz+1);
}

void World_Generate(World* world) {
    // Pass 1: generate all block data
    for (int cx = 0; cx < WORLD_W; cx++)
    for (int cz = 0; cz < WORLD_D; cz++)
        Chunk_Generate(&world->chunks[cx][cz], cx, cz);

    // Pass 2: build meshes (needs all neighbors to exist)
    for (int cx = 0; cx < WORLD_W; cx++)
    for (int cz = 0; cz < WORLD_D; cz++) {
        Chunk *nXN, *nXP, *nZN, *nZP;
        get_neighbors(world, cx, cz, &nXN, &nXP, &nZN, &nZP);
        Chunk_BuildMesh(&world->chunks[cx][cz], nXN, nXP, nZN, nZP);
    }

    // Pass 3: rebuild border chunks now that all neighbors have full block data
    // (trees near chunk edges may have modified neighbor-visible faces)
    for (int cx = 0; cx < WORLD_W; cx++)
    for (int cz = 0; cz < WORLD_D; cz++) {
        world->chunks[cx][cz].dirty = 1; // force rebuild
        Chunk *nXN, *nXP, *nZN, *nZP;
        get_neighbors(world, cx, cz, &nXN, &nXP, &nZN, &nZP);
        Chunk_BuildMesh(&world->chunks[cx][cz], nXN, nXP, nZN, nZP);
    }
}

void World_Render(World* world) {
    for (int cx = 0; cx < WORLD_W; cx++)
    for (int cz = 0; cz < WORLD_D; cz++)
        Chunk_Render(&world->chunks[cx][cz]);
}

void World_RebuildDirty(World* world) {
    for (int cx = 0; cx < WORLD_W; cx++)
    for (int cz = 0; cz < WORLD_D; cz++) {
        Chunk* c = &world->chunks[cx][cz];
        if (!c->dirty) continue;
        Chunk *nXN, *nXP, *nZN, *nZP;
        get_neighbors(world, cx, cz, &nXN, &nXP, &nZN, &nZP);
        Chunk_BuildMesh(c, nXN, nXP, nZN, nZP);
    }
}

// Convert world block coords to chunk + local coords and query
u8 World_GetBlock(World* world, int wx, int wy, int wz) {
    int cx = wx / CHUNK_SIZE, lx = wx % CHUNK_SIZE;
    int cz = wz / CHUNK_SIZE, lz = wz % CHUNK_SIZE;
    if (lx < 0) { lx += CHUNK_SIZE; cx--; }
    if (lz < 0) { lz += CHUNK_SIZE; cz--; }
    Chunk* c = get_chunk(world, cx, cz);
    return Chunk_GetBlock(c, lx, wy, lz);
}

bool World_SetBlock(World* world, int wx, int wy, int wz, u8 block) {
    int cx = wx / CHUNK_SIZE, lx = wx % CHUNK_SIZE;
    int cz = wz / CHUNK_SIZE, lz = wz % CHUNK_SIZE;
    if (lx < 0) { lx += CHUNK_SIZE; cx--; }
    if (lz < 0) { lz += CHUNK_SIZE; cz--; }
    Chunk* c = get_chunk(world, cx, cz);
    if (!c) return false;
    if (!Chunk_SetBlock(c, lx, wy, lz, block)) return false;
    // Also mark neighbor dirty if on chunk border
    if (lx == 0 && get_chunk(world, cx-1, cz)) get_chunk(world, cx-1, cz)->dirty = 1;
    if (lx == CHUNK_SIZE-1 && get_chunk(world, cx+1, cz)) get_chunk(world, cx+1, cz)->dirty = 1;
    if (lz == 0 && get_chunk(world, cx, cz-1)) get_chunk(world, cx, cz-1)->dirty = 1;
    if (lz == CHUNK_SIZE-1 && get_chunk(world, cx, cz+1)) get_chunk(world, cx, cz+1)->dirty = 1;
    return true;
}