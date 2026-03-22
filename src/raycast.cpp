#include "raycast.h"
#include "camera.h"
#include "chunk.h"
#include <math.h>

RayResult Raycast(World* world, guVector origin, guVector dir) {
    RayResult result = {0, 0,0,0, 0,0,0};

    // Normalize direction
    float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (len == 0) return result;
    dir.x /= len; dir.y /= len; dir.z /= len;

    float px = origin.x, py = origin.y, pz = origin.z;
    int last_bx = (int)floorf(px);
    int last_by = (int)floorf(py);
    int last_bz = (int)floorf(pz);

    for (float t = 0; t < RAY_MAX_DIST; t += RAY_STEP) {
        px = origin.x + dir.x * t;
        py = origin.y + dir.y * t;
        pz = origin.z + dir.z * t;

        int bx = (int)floorf(px);
        int by = (int)floorf(py);
        int bz = (int)floorf(pz);

        if (World_GetBlock(world, bx, by, bz) != BLOCK_AIR) {
            result.hit = 1;
            result.bx = bx; result.by = by; result.bz = bz;
            result.px = last_bx; result.py = last_by; result.pz = last_bz;
            return result;
        }

        last_bx = bx; last_by = by; last_bz = bz;
    }

    return result;
}

void Raycast_DrawHighlight(RayResult* ray) {
    if (!ray->hit) return;

    float x = (float)ray->bx;
    float y = (float)ray->by;
    float z = (float)ray->bz;
    float e = 0.01f; // epsilon — slightly outside the block to avoid z-fighting

    // Switch to no-texture passthrough color mode
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE); // don't write to z — overlay only

    // Load identity model matrix (block coords are already in world space via view)
    Mtx model, mv;
    guMtxIdentity(model);
    guMtxConcat(g_viewMatrix, model, mv);
    GX_LoadPosMtxImm(mv, GX_PNMTX0);

    // Draw 12 lines as thin quads forming a box outline
    // Each edge is a very thin quad slightly outside the block face
    float x0=x-e, y0=y-e, z0=z-e;
    float x1=x+1+e, y1=y+1+e, z1=z+1+e;

    u8 r=0, g=0, b=0, a=200; // dark outline

    #define LINE(ax,ay,az, bx,by,bz) \
        GX_Position3f32(ax,ay,az); GX_Color4u8(r,g,b,a); GX_TexCoord2f32(0,0); \
        GX_Position3f32(bx,by,bz); GX_Color4u8(r,g,b,a); GX_TexCoord2f32(0,0);

    GX_Begin(GX_LINES, GX_VTXFMT0, 24);
        // Bottom face edges
        LINE(x0,y0,z0, x1,y0,z0)
        LINE(x1,y0,z0, x1,y0,z1)
        LINE(x1,y0,z1, x0,y0,z1)
        LINE(x0,y0,z1, x0,y0,z0)
        // Top face edges
        LINE(x0,y1,z0, x1,y1,z0)
        LINE(x1,y1,z0, x1,y1,z1)
        LINE(x1,y1,z1, x0,y1,z1)
        LINE(x0,y1,z1, x0,y1,z0)
        // Vertical edges
        LINE(x0,y0,z0, x0,y1,z0)
        LINE(x1,y0,z0, x1,y1,z0)
        LINE(x1,y0,z1, x1,y1,z1)
        LINE(x0,y0,z1, x0,y1,z1)
    GX_End();

    #undef LINE

    // Restore state
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetCullMode(GX_CULL_BACK);
}