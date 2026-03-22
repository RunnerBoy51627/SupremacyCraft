#ifndef WORLD_H
#define WORLD_H

#include "chunk.h"

#define WORLD_W 3
#define WORLD_D 3

typedef struct {
    Chunk chunks[WORLD_W][WORLD_D];
} World;

void  World_Generate(World* world);
void  World_Render(World* world);
void  World_RebuildDirty(World* world);
u8    World_GetBlock(World* world, int wx, int wy, int wz);
bool  World_SetBlock(World* world, int wx, int wy, int wz, u8 block);

#endif