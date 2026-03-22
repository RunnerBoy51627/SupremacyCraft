#ifndef CRAFTING_H
#define CRAFTING_H

#include <gccore.h>

// Max grid sizes
#define CRAFT_GRID_2x2  4
#define CRAFT_GRID_3x3  9

// Recipe result
typedef struct {
    u8  outBlock;
    int outCount;
} CraftResult;

// Check a 2x2 grid (slots 0-3, row-major) for a matching recipe.
// Returns 1 and fills result if found, 0 otherwise.
int Crafting_Check2x2(const u8* grid, CraftResult* result);

// Check a 3x3 grid (slots 0-8, row-major) for a matching recipe.
int Crafting_Check3x3(const u8* grid, CraftResult* result);

// Consume ingredients for the matched recipe (call after player takes output).
// gridSize = 4 (2x2) or 9 (3x3).
void Crafting_Consume(u8* grid, int* counts, int gridSize);

#endif