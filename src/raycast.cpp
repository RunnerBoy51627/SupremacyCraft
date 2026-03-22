#include "raycast.h"
#include "utils.h"
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

    for (float t = 0; t < g_config.reach; t += RAY_STEP) {
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

void Raycast_DrawHighlight(RayResult* ray, float progress) {
    if (!ray->hit) return;

    float x = (float)ray->bx;
    float y = (float)ray->by;
    float z = (float)ray->bz;
    float e = 0.02f; // epsilon — slightly outside the block to avoid z-fighting

    // Use PASSCLR with TEXCOORD0 (not NULL) — TEX0 is always GX_DIRECT in VTXFMT0
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);

    Mtx model, mv;
    guMtxIdentity(model);
    guMtxConcat(g_viewMatrix, model, mv);
    GX_LoadPosMtxImm(mv, GX_PNMTX0);

    float x0=x-e, y0=y-e, z0=z-e;
    float x1=x+1+e, y1=y+1+e, z1=z+1+e;

    // Dark outline like Minecraft
    u8 r=0, g=0, b=0, a=180;

    #define LINE(ax,ay,az, bx,by,bz) \
        GX_Position3f32(ax,ay,az); GX_Color4u8(r,g,b,a); GX_TexCoord2f32(0,0); \
        GX_Position3f32(bx,by,bz); GX_Color4u8(r,g,b,a); GX_TexCoord2f32(0,0);

    GX_Begin(GX_LINES, GX_VTXFMT0, 24);
        LINE(x0,y0,z0, x1,y0,z0)  LINE(x1,y0,z0, x1,y0,z1)
        LINE(x1,y0,z1, x0,y0,z1)  LINE(x0,y0,z1, x0,y0,z0)
        LINE(x0,y1,z0, x1,y1,z0)  LINE(x1,y1,z0, x1,y1,z1)
        LINE(x1,y1,z1, x0,y1,z1)  LINE(x0,y1,z1, x0,y1,z0)
        LINE(x0,y0,z0, x0,y1,z0)  LINE(x1,y0,z0, x1,y1,z0)
        LINE(x1,y0,z1, x1,y1,z1)  LINE(x0,y0,z1, x0,y1,z1)
    GX_End();

    #undef LINE

    // ── Crack overlay ─────────────────────────────────────────────────
    // 8 stages matching Minecraft. Each stage darkens + shrinks the filled
    // faces slightly so the block appears to crack inward.
    if (progress > 0.05f) {
        int stage = (int)(progress * 8.0f);  // 0-7
        if (stage > 7) stage = 7;

        // Alpha and darkness increase with stage
        u8 ca = (u8)(40 + stage * 24);   // 40 .. 208
        u8 cd = (u8)(20 + stage * 10);   // darkness tint on faces

        // Draw semi-transparent dark quads over each visible face
        // (only top, front, right — the three Minecraft shows)
        // Push faces slightly OUTSIDE the block (like the outline epsilon)
        float out = 0.01f + stage * 0.002f;
        float fx0 = x - out,   fy0 = y - out,   fz0 = z - out;
        float fx1 = x+1+out,   fy1 = y+1+out,   fz1 = z+1+out;

        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

        GX_Begin(GX_QUADS, GX_VTXFMT0, 12);
        // Top face
        GX_Position3f32(fx0,fy1,fz0); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_Position3f32(fx1,fy1,fz0); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_Position3f32(fx1,fy1,fz1); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_Position3f32(fx0,fy1,fz1); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        // Front face
        GX_Position3f32(fx0,fy0,fz1); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_Position3f32(fx1,fy0,fz1); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_Position3f32(fx1,fy1,fz1); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_Position3f32(fx0,fy1,fz1); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        // Right face
        GX_Position3f32(fx1,fy0,fz0); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_Position3f32(fx1,fy0,fz1); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_Position3f32(fx1,fy1,fz1); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_Position3f32(fx1,fy1,fz0); GX_Color4u8(cd,cd,cd,ca); GX_TexCoord2f32(0,0);
        GX_End();

        GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    }

    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetCullMode(GX_CULL_BACK);
}