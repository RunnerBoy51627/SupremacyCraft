#ifndef TEXTURES_H
#define TEXTURES_H

#include <gccore.h>
#include "atlas_regions.h"

// These IDs must match the FIXED_ORDER in tools/gen_atlas.py:
// 0=grass_top, 1=grass_side, 2=dirt, 3=stone, 4=wood, 5=leaves
// atlas_regions.h is auto-generated and will match this order.

typedef struct {
    float u0, v0, u1, v1;
} TexRegion;

void Tex_Init();
void Tex_BindAtlas();
const TexRegion* Tex_GetRegion(int texID);

#endif