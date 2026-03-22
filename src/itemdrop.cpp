#include "itemdrop.h"
#include "chunk.h"
#include "camera.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <gccore.h>

static ItemDrop s_drops[MAX_ITEM_DROPS];

// ── Simple LCG for spread randomness (no stdlib rand needed) ─────────────────
static unsigned int s_seed = 12345;
static float frand() {
    s_seed = s_seed * 1664525u + 1013904223u;
    return (float)(s_seed & 0xFFFF) / 65535.0f; // 0..1
}

// ── Block face colors (top / front / right) ───────────────────────────────────
static void get_drop_colors(u8 block,
    u8* tr, u8* tg, u8* tb,
    u8* fr, u8* fg, u8* fb,
    u8* rr, u8* rg, u8* rb)
{
    switch (block) {
        case 1: // GRASS
            *tr=106;*tg=178;*tb=80;  *fr=100;*fg=70; *fb=40;  *rr=115;*rg=80; *rb=48; break;
        case 2: // DIRT
            *tr=134;*tg=96; *tb=67;  *fr=110;*fg=75; *fb=50;  *rr=125;*rg=88; *rb=58; break;
        case 3: // STONE
            *tr=128;*tg=128;*tb=128; *fr=100;*fg=100;*fb=100; *rr=115;*rg=115;*rb=115; break;
        case 4: // WOOD
            *tr=160;*tg=130;*tb=80;  *fr=120;*fg=85; *fb=45;  *rr=140;*rg=100;*rb=55; break;
        case 5: // LEAF
            *tr=50; *tg=120;*tb=35;  *fr=38; *fg=95; *fb=28;  *rr=44; *rg=108;*rb=32; break;
        case 9: // TNT
            *tr=180;*tg=60;*tb=60; *fr=200;*fg=30;*fb=30; *rr=160;*rg=25;*rb=25; break;
        default:
            *tr=*tg=*tb=180; *fr=*fg=*fb=140; *rr=*rg=*rb=160; break;
    }
}

// ── Draw a small colored cube at origin in world space ────────────────────────
static void draw_drop_cube(float s,
    u8 tr, u8 tg, u8 tb,
    u8 fr, u8 fg, u8 fb,
    u8 rr, u8 rg, u8 rb)
{
    float h = s * 0.5f;
    u8 br=(u8)(rr*.72f), bg=(u8)(rg*.72f), bb=(u8)(rb*.72f);
    u8 lr=(u8)(rr*.85f), lg=(u8)(rg*.85f), lb=(u8)(rb*.85f);
    u8 botr=(u8)(tr*.40f), botg=(u8)(tg*.40f), botb=(u8)(tb*.40f);

    GX_Begin(GX_QUADS, GX_VTXFMT0, 24);
    // Top
    GX_Position3f32(-h, h,-h); GX_Color4u8(tr,tg,tb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h, h,-h); GX_Color4u8(tr,tg,tb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h, h, h); GX_Color4u8(tr,tg,tb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-h, h, h); GX_Color4u8(tr,tg,tb,255); GX_TexCoord2f32(0,0);
    // Bottom
    GX_Position3f32(-h,-h, h); GX_Color4u8(botr,botg,botb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h,-h, h); GX_Color4u8(botr,botg,botb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h,-h,-h); GX_Color4u8(botr,botg,botb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-h,-h,-h); GX_Color4u8(botr,botg,botb,255); GX_TexCoord2f32(0,0);
    // Front (+Z face, winds CCW when viewed from +Z)
    GX_Position3f32( h,-h, h); GX_Color4u8(fr,fg,fb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-h,-h, h); GX_Color4u8(fr,fg,fb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-h, h, h); GX_Color4u8(fr,fg,fb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h, h, h); GX_Color4u8(fr,fg,fb,255); GX_TexCoord2f32(0,0);
    // Back (-Z face)
    GX_Position3f32(-h,-h,-h); GX_Color4u8(br,bg,bb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h,-h,-h); GX_Color4u8(br,bg,bb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h, h,-h); GX_Color4u8(br,bg,bb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-h, h,-h); GX_Color4u8(br,bg,bb,255); GX_TexCoord2f32(0,0);
    // Right (+X face)
    GX_Position3f32( h,-h,-h); GX_Color4u8(rr,rg,rb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h,-h, h); GX_Color4u8(rr,rg,rb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h, h, h); GX_Color4u8(rr,rg,rb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( h, h,-h); GX_Color4u8(rr,rg,rb,255); GX_TexCoord2f32(0,0);
    // Left (-X face)
    GX_Position3f32(-h,-h, h); GX_Color4u8(lr,lg,lb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-h,-h,-h); GX_Color4u8(lr,lg,lb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-h, h,-h); GX_Color4u8(lr,lg,lb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-h, h, h); GX_Color4u8(lr,lg,lb,255); GX_TexCoord2f32(0,0);
    GX_End();
}

// ── Init ──────────────────────────────────────────────────────────────────────
void ItemDrop_Init(void) {
    for (int i = 0; i < MAX_ITEM_DROPS; i++)
        s_drops[i].active = 0;
}

// ── Spawn ─────────────────────────────────────────────────────────────────────
void ItemDrop_Spawn(u8 blockType, float x, float y, float z, int pickupDelay) {
    for (int i = 0; i < MAX_ITEM_DROPS; i++) {
        if (!s_drops[i].active) {
            ItemDrop* d = &s_drops[i];
            d->active      = 1;
            d->blockType   = blockType;
            // Spawn at block centre + small random spread
            d->x           = x + 0.5f + (frand() - 0.5f) * 0.5f;
            d->y           = y + 0.5f;
            d->z           = z + 0.5f + (frand() - 0.5f) * 0.5f;
            d->vy          = 0.04f + frand() * 0.04f; // small upward pop
            d->bobPhase    = frand() * 6.283f;        // random phase offset
            d->attractPhase = 0.0f;
            d->lifetime    = DROP_LIFETIME;
            d->grounded    = 0;
            d->pickupDelay = pickupDelay;
            return;
        }
    }
    // Pool full — silently drop the item (shouldn't happen in normal play)
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static int world_solid_at(World* world, float x, float y, float z) {
    int bx = (int)floorf(x);
    int by = (int)floorf(y);
    int bz = (int)floorf(z);
    if (bx < 0 || bz < 0 || by < 0) return 1;
    if (by >= CHUNK_SIZE) return 0;
    return World_GetBlock(world, bx, by, bz) != BLOCK_AIR;
}

static void inventory_add(GUIState* gui, u8 blockType) {
    // Try hotbar first (slots 0-8), then main inventory (9-35)
    // Within each section: stack existing, then fill empty
    int sections[2][2] = {{0, HOTBAR_SLOTS}, {HOTBAR_SLOTS, INV_SLOTS}};
    for (int s = 0; s < 2; s++) {
        int start = sections[s][0], end = sections[s][1];
        for (int i = start; i < end; i++) {
            if (gui->slotBlock[i] == blockType && gui->slotCount[i] > 0
                && gui->slotCount[i] < 64) {
                gui->slotCount[i]++; return;
            }
        }
    }
    for (int s = 0; s < 2; s++) {
        int start = sections[s][0], end = sections[s][1];
        for (int i = start; i < end; i++) {
            if (gui->slotCount[i] == 0) {
                gui->slotBlock[i] = blockType;
                gui->slotCount[i] = 1; return;
            }
        }
    }
    // Inventory full
}

// ── Update ────────────────────────────────────────────────────────────────────
void ItemDrop_Update(World* world, Player* player, GUIState* gui) {
    float px = player->pos.x;
    float py = player->pos.y + 0.9f; // player centre height
    float pz = player->pos.z;

    for (int i = 0; i < MAX_ITEM_DROPS; i++) {
        ItemDrop* d = &s_drops[i];
        if (!d->active) continue;

        // Lifetime
        d->lifetime--;
        if (d->lifetime <= 0) { d->active = 0; continue; }

        // Distance to player
        float dx = px - d->x;
        float dy = py - d->y;
        float dz = pz - d->z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        // Countdown pickup delay
        if (d->pickupDelay > 0) d->pickupDelay--;

        // ── Pickup ────────────────────────────────────────────────────────
        if (dist < DROP_PICKUP_DIST && d->pickupDelay <= 0) {
            inventory_add(gui, d->blockType);
            d->active = 0;
            gui->score += 5;
            continue;
        }

        // ── Attract toward player when close enough ───────────────────────
        if (dist < DROP_ATTRACT_DIST && d->pickupDelay <= 0) {
            float speed = 0.12f + (DROP_ATTRACT_DIST - dist) * 0.08f;
            float inv = speed / dist;
            d->x += dx * inv;
            d->y += dy * inv;
            d->z += dz * inv;
            d->attractPhase += 0.2f;
            continue; // skip normal physics while attracting
        }
        d->attractPhase = 0.0f;

        // ── Physics ───────────────────────────────────────────────────────
        if (!d->grounded) {
            d->vy -= 0.008f; // gravity
            if (d->vy < -0.3f) d->vy = -0.3f;
            float ny = d->y + d->vy;
            if (world_solid_at(world, d->x, ny - DROP_SIZE*0.5f, d->z)) {
                // Land: snap to top of block
                d->y = floorf(d->y - DROP_SIZE*0.5f) + 1.0f + DROP_SIZE*0.5f;
                d->vy = 0.0f;
                d->grounded = 1;
            } else {
                d->y = ny;
            }
        }

        // ── Idle bob (only when grounded) ─────────────────────────────────
        if (d->grounded) {
            d->bobPhase += 0.05f;
        }
    }
}

// ── Render ────────────────────────────────────────────────────────────────────
void ItemDrop_Render(void) {
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_BACK);

    for (int i = 0; i < MAX_ITEM_DROPS; i++) {
        ItemDrop* d = &s_drops[i];
        if (!d->active) continue;

        u8 tr,tg,tb, fr,fg,fb, rr,rg,rb;
        get_drop_colors(d->blockType, &tr,&tg,&tb, &fr,&fg,&fb, &rr,&rg,&rb);

        // Bob offset
        float bobY = sinf(d->bobPhase) * 0.06f;

        // Spin: rotate around Y axis
        float spin = d->bobPhase * 1.2f; // slightly faster than bob
        float cosS = cosf(spin);
        float sinS = sinf(spin);

        // Build model matrix: translate to world pos + bob, then Y-rotate
        Mtx rot, trans, mv;
        // Y rotation (standard right-hand: rot[0][2]=-sin, rot[2][0]=+sin)
        rot[0][0]= cosS; rot[0][1]=0; rot[0][2]=-sinS; rot[0][3]=0;
        rot[1][0]=0;     rot[1][1]=1; rot[1][2]=0;     rot[1][3]=0;
        rot[2][0]= sinS; rot[2][1]=0; rot[2][2]= cosS; rot[2][3]=0;

        guMtxIdentity(trans);
        guMtxTransApply(trans, trans, d->x, d->y + bobY, d->z);

        guMtxConcat(trans, rot, mv);

        // Combine with view matrix
        Mtx final;
        guMtxConcat(g_viewMatrix, mv, final);
        GX_LoadPosMtxImm(final, GX_PNMTX0);

        draw_drop_cube(DROP_SIZE, tr,tg,tb, fr,fg,fb, rr,rg,rb);
    }

    // Restore state
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_BACK);
}