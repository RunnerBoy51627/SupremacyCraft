#include "crafting.h"
#include <string.h>

// Block IDs (must match chunk.h)
#define B_AIR  0
#define B_GRS  1
#define B_DRT  2
#define B_STN  3
#define B_WOD  4
#define B_LF   5
#define B_PLK  6   // planks
#define B_STK  7   // sticks
#define B_CBT  8   // crafting table
#define B_TRH  9   // torch (placeholder, no light yet)

// ── Recipe structs ────────────────────────────────────────────────────────────

typedef struct {
    u8  grid[9];     // pattern (row-major, B_AIR = wildcard-empty)
    int w, h;        // used width/height within the 3x3 (for shaped recipes)
    u8  out;
    int count;
    int shapeless;   // 1 = ingredient order doesn't matter
} Recipe;

// ── Recipe table ──────────────────────────────────────────────────────────────
// All recipes defined in a 3x3 grid layout.
// Shaped recipes must have their pattern in the top-left corner.

// ── Block IDs for TNT ────────────────────────────────────────────────────────
#define B_TNT  9   // TNT block

static const Recipe s_recipes[] = {
    // ── Wood Log → 4 Planks ──────────────────────────────────────────────────
    { {B_WOD,0,0, 0,0,0, 0,0,0}, 1,1, B_PLK, 4, 0 },

    // ── 2 Planks (vertical) → 4 Sticks ──────────────────────────────────────
    { {B_PLK,0,0, B_PLK,0,0, 0,0,0}, 1,2, B_STK, 4, 0 },

    // ── 4 Planks (2x2) → Crafting Table ─────────────────────────────────────
    { {B_PLK,B_PLK,0, B_PLK,B_PLK,0, 0,0,0}, 2,2, B_CBT, 1, 0 },

    // ── TNT: sand surround gunpowder (use stone=gunpowder, dirt=sand) ────────
    // Pattern:  dirt  stone  dirt        (2x2 only fits the centre-4 of this)
    //           stone dirt   stone   →  TNT x1
    // 2x2 version: stone in centre-ish — dirt+stone+stone+dirt diagonals
    { {B_DRT,B_STN,0, B_STN,B_DRT,0, 0,0,0}, 2,2, B_TNT, 1, 0 },
    { {B_STN,B_DRT,0, B_DRT,B_STN,0, 0,0,0}, 2,2, B_TNT, 1, 0 },
};

#define RECIPE_COUNT (int)(sizeof(s_recipes)/sizeof(s_recipes[0]))

// ── Helpers ───────────────────────────────────────────────────────────────────

// Normalize a grid: find the bounding box of non-air cells and
// return the pattern left-aligned in a 3x3 buffer.
static void normalize(const u8* grid, int gridW, u8* out3x3, int* usedW, int* usedH) {
    // Find min/max row and col with non-air
    int minR=99,maxR=-1,minC=99,maxC=-1;
    for (int r=0;r<3;r++) for (int c=0;c<gridW;c++) {
        int idx = r*gridW+c;
        if (idx < gridW*3 && grid[idx] != B_AIR) {
            if (r<minR) minR=r; if (r>maxR) maxR=r;
            if (c<minC) minC=c; if (c>maxC) maxC=c;
        }
    }
    memset(out3x3, 0, 9);
    if (maxR < 0) { *usedW=0; *usedH=0; return; }
    *usedH = maxR-minR+1;
    *usedW = maxC-minC+1;
    for (int r=minR;r<=maxR;r++) for (int c=minC;c<=maxC;c++) {
        int src = r*gridW+c;
        int dst = (r-minR)*3+(c-minC);
        if (src < gridW*3) out3x3[dst] = grid[src];
    }
}

static int match_recipe(const u8* norm, int w, int h, const Recipe* rec) {
    if (rec->w != w || rec->h != h) return 0;
    // Compare only the used cells (top-left w x h of 3x3)
    for (int r=0; r<h; r++) for (int c=0; c<w; c++) {
        if (norm[r*3+c] != rec->grid[r*3+c]) return 0;
    }
    return 1;
}

// ── Public API ────────────────────────────────────────────────────────────────

static int check_grid(const u8* grid, int gridW, CraftResult* result) {
    u8 norm[9];
    int w, h;
    normalize(grid, gridW, norm, &w, &h);
    if (w == 0) return 0;

    for (int i=0; i<RECIPE_COUNT; i++) {
        if (match_recipe(norm, w, h, &s_recipes[i])) {
            result->outBlock = s_recipes[i].out;
            result->outCount = s_recipes[i].count;
            return 1;
        }
    }
    return 0;
}

int Crafting_Check2x2(const u8* grid, CraftResult* result) {
    // Expand 2x2 into a 3x3 for matching
    u8 g3[9] = {
        grid[0], grid[1], B_AIR,
        grid[2], grid[3], B_AIR,
        B_AIR,   B_AIR,   B_AIR
    };
    return check_grid(g3, 3, result);
}

int Crafting_Check3x3(const u8* grid, CraftResult* result) {
    return check_grid(grid, 3, result);
}

void Crafting_Consume(u8* grid, int* counts, int gridSize) {
    for (int i=0; i<gridSize; i++) {
        if (counts[i] > 0) {
            counts[i]--;
            if (counts[i] == 0) grid[i] = B_AIR;
        }
    }
}