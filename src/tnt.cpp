#include "tnt.h"
#include "sound.h"
#include "chunk.h"
#include "camera.h"
#include "textures.h"
#include <math.h>

#include "platform_types.h"

static LitTNT s_tnt[MAX_TNT];

void TNT_Init(void) {
    for (int i = 0; i < MAX_TNT; i++)
        s_tnt[i].active = 0;
}

int TNT_Ignite(World* world, int wx, int wy, int wz) {
    if (World_GetBlock(world, wx, wy, wz) != BLOCK_TNT) return 0;
    // Find free slot
    for (int i = 0; i < MAX_TNT; i++) {
        if (s_tnt[i].active) continue;
        // Remove block from world, replace with air
        World_SetBlock(world, wx, wy, wz, BLOCK_AIR);
        s_tnt[i].active = 1;
        s_tnt[i].wx     = wx;
        s_tnt[i].wy     = wy;
        s_tnt[i].wz     = wz;
        s_tnt[i].fuse   = TNT_FUSE;
        s_tnt[i].flash  = 0.0f;
        return 1;
    }
    return 0;
}

static void explode(World* world, Player* player, int cx, int cy, int cz) {
    Sound_Play(SFX_EXPLODE);

    // Pass 1: chain reaction
    for (int dx = -TNT_RADIUS; dx <= TNT_RADIUS; dx++)
    for (int dy = -TNT_RADIUS; dy <= TNT_RADIUS; dy++)
    for (int dz = -TNT_RADIUS; dz <= TNT_RADIUS; dz++) {
        float dist = sqrtf((float)(dx*dx + dy*dy + dz*dz));
        if (dist > TNT_RADIUS) continue;
        int bx = cx+dx, by = cy+dy, bz = cz+dz;
        if (World_GetBlock(world, bx, by, bz) == BLOCK_TNT)
            TNT_Ignite(world, bx, by, bz);
    }

    // Pass 2: destroy blocks
    int particlesBudget = 8;
    for (int dx = -TNT_RADIUS; dx <= TNT_RADIUS; dx++)
    for (int dy = -TNT_RADIUS; dy <= TNT_RADIUS; dy++)
    for (int dz = -TNT_RADIUS; dz <= TNT_RADIUS; dz++) {
        float dist = sqrtf((float)(dx*dx + dy*dy + dz*dz));
        if (dist > TNT_RADIUS) continue;
        int bx = cx+dx, by = cy+dy, bz = cz+dz;
        u8 block = World_GetBlock(world, bx, by, bz);
        if (block == BLOCK_AIR) continue;
        if (particlesBudget > 0) {
            Particle_SpawnBlockBreak(bx, by, bz, block);
            particlesBudget--;
        }
        World_SetBlock(world, bx, by, bz, BLOCK_AIR);
    }

    // Player damage
    float pdx = player->pos.x - (cx + 0.5f);
    float pdy = player->pos.y - (cy + 0.5f);
    float pdz = player->pos.z - (cz + 0.5f);
    float pdist = sqrtf(pdx*pdx + pdy*pdy + pdz*pdz);
    if (pdist < TNT_RADIUS + 1) {
        int damage = (int)(40.0f * (1.0f - pdist / (TNT_RADIUS + 1)));
        if (damage > 0) Player_Damage(player, damage);
    }
}

void TNT_Update(World* world, Player* player) {
    for (int i = 0; i < MAX_TNT; i++) {
        if (!s_tnt[i].active) continue;
        s_tnt[i].fuse--;
        s_tnt[i].flash += 0.25f;
        if (s_tnt[i].fuse <= 0) {
            // Mark inactive BEFORE exploding so chain reactions
            // dont double-process this slot
            s_tnt[i].active = 0;
            explode(world, player, s_tnt[i].wx, s_tnt[i].wy, s_tnt[i].wz);
        }
    }
}

void TNT_Render(void) {
#ifdef _PC
    // PC: full flashing TNT render
    for (int i = 0; i < MAX_TNT; i++) {
        if (!s_tnt[i].active) continue;
        float fx = (float)s_tnt[i].wx;
        float fy = (float)s_tnt[i].wy;
        float fz = (float)s_tnt[i].wz;
        float urgency = 1.0f - (float)s_tnt[i].fuse / TNT_FUSE;
        float t = sinf(s_tnt[i].flash * (3.0f + urgency * 8.0f)) * 0.5f + 0.5f;
        u8 r = (u8)(200 + t * 55);
        u8 g2 = (u8)(t * 255);
        u8 b2 = (u8)(t * 255);
        Mtx model, mv;
        guMtxIdentity(model);
        guMtxTransApply(model, model, fx, fy, fz);
        guMtxConcat(g_viewMatrix, model, mv);
        GX_LoadPosMtxImm(mv, GX_PNMTX0);
        GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
        GX_SetCullMode(GX_CULL_BACK);
        Tex_BindAtlas();
        const TexRegion* ts = Tex_GetRegion(TEX_TNT_SIDE);
        const TexRegion* tt = Tex_GetRegion(TEX_TNT_TOP);
        const TexRegion* tb = Tex_GetRegion(TEX_TNT_BOTTOM);
        GX_Begin(GX_QUADS, GX_VTXFMT0, 24);
        GX_Position3f32(0,1,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(tt->u0,tt->v0);
        GX_Position3f32(1,1,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(tt->u1,tt->v0);
        GX_Position3f32(1,1,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(tt->u1,tt->v1);
        GX_Position3f32(0,1,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(tt->u0,tt->v1);
        GX_Position3f32(0,0,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(tb->u0,tb->v0);
        GX_Position3f32(1,0,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(tb->u1,tb->v0);
        GX_Position3f32(1,0,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(tb->u1,tb->v1);
        GX_Position3f32(0,0,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(tb->u0,tb->v1);
        GX_Position3f32(1,1,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u0,ts->v0);
        GX_Position3f32(0,1,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u1,ts->v0);
        GX_Position3f32(0,0,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u1,ts->v1);
        GX_Position3f32(1,0,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u0,ts->v1);
        GX_Position3f32(0,1,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u0,ts->v0);
        GX_Position3f32(1,1,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u1,ts->v0);
        GX_Position3f32(1,0,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u1,ts->v1);
        GX_Position3f32(0,0,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u0,ts->v1);
        GX_Position3f32(0,1,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u0,ts->v0);
        GX_Position3f32(0,1,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u1,ts->v0);
        GX_Position3f32(0,0,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u1,ts->v1);
        GX_Position3f32(0,0,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u0,ts->v1);
        GX_Position3f32(1,1,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u0,ts->v0);
        GX_Position3f32(1,1,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u1,ts->v0);
        GX_Position3f32(1,0,0); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u1,ts->v1);
        GX_Position3f32(1,0,1); GX_Color4u8(r,g2,b2,255); GX_TexCoord2f32(ts->u0,ts->v1);
        GX_End();
    }
#endif // _PC — GC just shows TNT as a static block (no flash render)
}

LitTNT* TNT_GetArray(void) { return s_tnt; }
int     TNT_GetCount(void) { return MAX_TNT; }