#include "itemdrop.h"
#include "textures.h"
#include "atlas_regions.h"
#include "chunk.h"
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
static void draw_drop_cube_textured(float s, u8 block)
{
    float h = s * 0.5f;
    const TexRegion* tt  = Tex_GetRegion(block_face_tex_pub(block, 0));
    const TexRegion* tbt = Tex_GetRegion(block_face_tex_pub(block, 1));
    const TexRegion* tn  = Tex_GetRegion(block_face_tex_pub(block, 2));
    const TexRegion* tss = Tex_GetRegion(block_face_tex_pub(block, 3));
    const TexRegion* tw  = Tex_GetRegion(block_face_tex_pub(block, 4));
    const TexRegion* te  = Tex_GetRegion(block_face_tex_pub(block, 5));
    GX_Begin(GX_QUADS, GX_VTXFMT0, 24);
    // Top
    GX_Position3f32(-h,h,-h);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tt->u0,tt->v0);
    GX_Position3f32( h,h,-h);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tt->u1,tt->v0);
    GX_Position3f32( h,h, h);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tt->u1,tt->v1);
    GX_Position3f32(-h,h, h);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tt->u0,tt->v1);
    // Bottom
    GX_Position3f32(-h,-h, h);GX_Color4u8(130,130,130,255);GX_TexCoord2f32(tbt->u0,tbt->v0);
    GX_Position3f32( h,-h, h);GX_Color4u8(130,130,130,255);GX_TexCoord2f32(tbt->u1,tbt->v0);
    GX_Position3f32( h,-h,-h);GX_Color4u8(130,130,130,255);GX_TexCoord2f32(tbt->u1,tbt->v1);
    GX_Position3f32(-h,-h,-h);GX_Color4u8(130,130,130,255);GX_TexCoord2f32(tbt->u0,tbt->v1);
    // Front
    GX_Position3f32( h,-h, h);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tss->u0,tss->v0);
    GX_Position3f32(-h,-h, h);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tss->u1,tss->v0);
    GX_Position3f32(-h, h, h);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tss->u1,tss->v1);
    GX_Position3f32( h, h, h);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tss->u0,tss->v1);
    // Back
    GX_Position3f32(-h,-h,-h);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tn->u0,tn->v0);
    GX_Position3f32( h,-h,-h);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tn->u1,tn->v0);
    GX_Position3f32( h, h,-h);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tn->u1,tn->v1);
    GX_Position3f32(-h, h,-h);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tn->u0,tn->v1);
    // Right
    GX_Position3f32( h,-h,-h);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(te->u0,te->v0);
    GX_Position3f32( h,-h, h);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(te->u1,te->v0);
    GX_Position3f32( h, h, h);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(te->u1,te->v1);
    GX_Position3f32( h, h,-h);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(te->u0,te->v1);
    // Left
    GX_Position3f32(-h,-h, h);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(tw->u0,tw->v0);
    GX_Position3f32(-h,-h,-h);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(tw->u1,tw->v0);
    GX_Position3f32(-h, h,-h);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(tw->u1,tw->v1);
    GX_Position3f32(-h, h, h);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(tw->u0,tw->v1);
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
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_BACK);
    Tex_BindAtlas();

    for (int i = 0; i < MAX_ITEM_DROPS; i++) {
        ItemDrop* d = &s_drops[i];
        if (!d->active) continue;


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

        draw_drop_cube_textured(DROP_SIZE, d->blockType);
    }

    // Restore state
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_BACK);
}