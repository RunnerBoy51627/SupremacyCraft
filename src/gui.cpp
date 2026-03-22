#include "gui.h"
#include "textures.h"
#include "atlas_regions.h"
#include "chunk.h"

// Runtime screen dimensions — use these instead of compile-time SCREEN_W/H
static int s_sw = SCREEN_W;
static int s_sh = SCREEN_H;

static inline float SW() { return (float)s_sw; }
static inline float SH() { return (float)s_sh; }

// Call when widescreen setting changes
static void gui_update_screen_size(GXRModeObj* rmode) {
    (void)rmode;
    // Always derive from g_config so runtime toggle takes effect immediately
    s_sw = g_config.widescreen ? 854 : 640;
    s_sh = 480;
}

void GUI_UpdateScreenSize(GXRModeObj* rmode) {
    gui_update_screen_size(rmode);
}
#include "utils.h"
#include "chunk.h"
#include <gccore.h>
#include <stdio.h>

// Forward declarations
static void draw_string(float x, float y, float scale, const char* str, u8 r, u8 g, u8 b);

// ─── Helpers ────────────────────────────────────────────────────────────────

// Draw a filled 2D quad at pixel coordinates. z=0 puts it in front of everything.
static void draw_rect(float x, float y, float w, float h,
                      u8 r, u8 g, u8 b, u8 a) {
    Mtx identity;
    guMtxIdentity(identity);
    GX_LoadPosMtxImm(identity, GX_PNMTX0);

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(x,   y,   0); GX_Color4u8(r, g, b, a);
        GX_Position3f32(x+w, y,   0); GX_Color4u8(r, g, b, a);
        GX_Position3f32(x+w, y+h, 0); GX_Color4u8(r, g, b, a);
        GX_Position3f32(x,   y+h, 0); GX_Color4u8(r, g, b, a);
    GX_End();
}

// Draw a rectangle outline (4 thin rects)
static void draw_rect_outline(float x, float y, float w, float h, float thickness,
                               u8 r, u8 g, u8 b, u8 a) {
    draw_rect(x,         y,          w,         thickness, r, g, b, a); // top
    draw_rect(x,         y+h-thickness, w,      thickness, r, g, b, a); // bottom
    draw_rect(x,         y,          thickness, h,         r, g, b, a); // left
    draw_rect(x+w-thickness, y,      thickness, h,         r, g, b, a); // right
}

// ─── Init ────────────────────────────────────────────────────────────────────

void GUI_Init(GUIState* gui) {
    gui->selectedSlot = 0;
    gui->health       = 20;
    gui->maxHealth    = 20;
    gui->paused       = 0;
    gui->pauseCursor  = 0;
    gui->score        = 0;

    // Default inventory: one of each block type
    static const u8 default_blocks[] = {
        BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE,
        BLOCK_WOOD,  BLOCK_LEAF, BLOCK_STONE,
        BLOCK_DIRT,  BLOCK_STONE
    };
    for (int i = 0; i < INV_SLOTS; i++) {
        gui->slotBlock[i] = BLOCK_AIR;
        gui->slotCount[i] = 0;
    }
    gui->inventoryOpen  = 0;
    gui->invCursorX     = 0;
    gui->invCursorY     = 0;
    gui->heldItemBlock  = 0;
    gui->heldItemCount  = 0;
}

// ─── Projection Switch ───────────────────────────────────────────────────────

void GUI_Begin2D(GXRModeObj* rmode) {
    gui_update_screen_size(rmode);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_SetFog(GX_FOG_NONE, 0, 0, 0, 0, (GXColor){0,0,0,0}); // disable fog for GUI
    // GUI uses no texture — switch TEV to pass vertex color only
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);

    // Orthographic projection matching screen pixel coords (0,0) = top-left
    Mtx44 ortho;
    guOrtho(ortho, 0, SH(), 0, SW(), 0, 1);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);
}

void GUI_End2D(GXRModeObj* rmode) {
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetCullMode(GX_CULL_BACK);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    // Restore fog for 3D world
    Config_ApplyFog();
    // Restore textured TEV for 3D world
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    // Restore projection using config (preserves widescreen/FOV settings)
    Config_ApplyDisplay(rmode);
}

// ─── Crosshair ───────────────────────────────────────────────────────────────

void GUI_DrawCrosshair(GXRModeObj* rmode) {
    float cx = SW() / 2.0f;
    float cy = SH() / 2.0f;
    float len = 10.0f;
    float thick = 2.0f;

    // White crosshair with dark outline for visibility
    // Outline
    draw_rect(cx - len - 1, cy - thick - 1, (len * 2) + 2, (thick * 2) + 2, 0, 0, 0, 200);
    draw_rect(cx - thick - 1, cy - len - 1, (thick * 2) + 2, (len * 2) + 2, 0, 0, 0, 200);
    // White fill
    draw_rect(cx - len, cy - thick, len * 2, thick * 2, 255, 255, 255, 255); // horizontal
    draw_rect(cx - thick, cy - len, thick * 2, len * 2, 255, 255, 255, 255); // vertical
}

// ─── Block icon renderer ─────────────────────────────────────────────────────
// Draws a fake isometric block using 3 colored rects (top, left face, right face)

static void draw_block_icon(float x, float y, float size, u8 block) {
    // Tool/item: flat sprite icon
    if (block == BLOCK_FLINT_STEEL) {
        const TexRegion* ts = Tex_GetRegion(TEX_FLINT_STEEL);
        GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        Tex_BindAtlas();
        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(x,      y,      0); GX_Color4u8(255,255,255,255); GX_TexCoord2f32(ts->u0,ts->v0);
        GX_Position3f32(x+size, y,      0); GX_Color4u8(255,255,255,255); GX_TexCoord2f32(ts->u1,ts->v0);
        GX_Position3f32(x+size, y+size, 0); GX_Color4u8(255,255,255,255); GX_TexCoord2f32(ts->u1,ts->v1);
        GX_Position3f32(x,      y+size, 0); GX_Color4u8(255,255,255,255); GX_TexCoord2f32(ts->u0,ts->v1);
        GX_End();
        GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
        return;
    }
    // Isometric block icon — proper 45-degree Y rotation look
    // Top face is a parallelogram, left/right faces are quads
    // Matches classic Minecraft inventory block rendering
    float s  = size;
    float hw = s * 0.5f;     // half width
    float th = s * 0.25f;    // top face height (skewed)
    float sh = s * 0.75f;    // side face height
    // cx = horizontal center of icon
    float cx = x + hw;

    // Key vertices of isometric block:
    //   TL = top-left corner of top face
    //   TR = top-right corner
    //   TM = top-middle (front peak)
    //   BL = bottom-left
    //   BR = bottom-right
    //   BM = bottom-middle
    float TLx=x,    TLy=y+th;
    float TRx=x+s,  TRy=y+th;
    float TMx=cx,   TMy=y;
    float BLx=x,    BLy=y+th+sh;
    float BRx=x+s,  BRy=y+th+sh;
    float BMx=cx,   BMy=y+sh;

    const TexRegion* tt = Tex_GetRegion(block_face_tex_pub(block, 0));
    const TexRegion* tl = Tex_GetRegion(block_face_tex_pub(block, 4));
    const TexRegion* tr2= Tex_GetRegion(block_face_tex_pub(block, 5));

    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    Tex_BindAtlas();

    // Top face — parallelogram: TMx,TMy -> TRx,TRy -> BLx+hw,BLy-sh+th -> TLx,TLy
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(TMx, TMy,   0); GX_Color4u8(255,255,255,255); GX_TexCoord2f32(tt->u0,tt->v0);
    GX_Position3f32(TRx, TRy,   0); GX_Color4u8(255,255,255,255); GX_TexCoord2f32(tt->u1,tt->v0);
    GX_Position3f32(BMx, BMy,   0); GX_Color4u8(255,255,255,255); GX_TexCoord2f32(tt->u1,tt->v1);
    GX_Position3f32(TLx, TLy,   0); GX_Color4u8(255,255,255,255); GX_TexCoord2f32(tt->u0,tt->v1);
    GX_End();

    // Left face — medium shade
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(TLx, TLy,   0); GX_Color4u8(180,180,180,255); GX_TexCoord2f32(tl->u0,tl->v0);
    GX_Position3f32(BMx, BMy,   0); GX_Color4u8(180,180,180,255); GX_TexCoord2f32(tl->u1,tl->v0);
    GX_Position3f32(BMx, BLy,   0); GX_Color4u8(180,180,180,255); GX_TexCoord2f32(tl->u1,tl->v1);
    GX_Position3f32(BLx, BLy,   0); GX_Color4u8(180,180,180,255); GX_TexCoord2f32(tl->u0,tl->v1);
    GX_End();

    // Right face — darker shade
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(BMx, BMy,   0); GX_Color4u8(130,130,130,255); GX_TexCoord2f32(tr2->u0,tr2->v0);
    GX_Position3f32(TRx, TRy,   0); GX_Color4u8(130,130,130,255); GX_TexCoord2f32(tr2->u1,tr2->v0);
    GX_Position3f32(BRx, BRy,   0); GX_Color4u8(130,130,130,255); GX_TexCoord2f32(tr2->u1,tr2->v1);
    GX_Position3f32(BMx, BLy,   0); GX_Color4u8(130,130,130,255); GX_TexCoord2f32(tr2->u0,tr2->v1);
    GX_End();

    // Restore flat color mode
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
}

// ─── Hotbar ──────────────────────────────────────────────────────────────────

void GUI_DrawHotbar(GXRModeObj* rmode, GUIState* gui) {
    float slotSize = 40.0f;
    float slotGap  = 4.0f;
    float totalW   = HOTBAR_SLOTS * slotSize + (HOTBAR_SLOTS - 1) * slotGap;
    float startX   = (SW() - totalW) / 2.0f;
    float startY   = SH() - slotSize - 12.0f;

    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        float x = startX + i * (slotSize + slotGap);

        // Slot background
        draw_rect(x, startY, slotSize, slotSize, 30, 30, 30, 200);

        // Border
        if (i == gui->selectedSlot)
            draw_rect_outline(x-2, startY-2, slotSize+4, slotSize+4, 2, 255,255,255,255);
        else
            draw_rect_outline(x, startY, slotSize, slotSize, 1, 80,80,80,200);

        // Block icon (only if slot has items)
        if (gui->slotCount[i] > 0 && gui->slotBlock[i] != BLOCK_AIR) {
            float iconPad = 4.0f;
            draw_block_icon(x + iconPad, startY + iconPad,
                            slotSize - iconPad*2, gui->slotBlock[i]);
        }

        // Item count — bottom right of slot (drawn as small pixel digits)
        if (gui->slotCount[i] > 0 && gui->slotCount[i] < 999) {
            // Draw count using pixel font — right-align in slot
            char buf[8];
            int n = gui->slotCount[i];
            int len = (n >= 100) ? 3 : (n >= 10) ? 2 : 1;
            buf[0] = (n/100) % 10 + '0';
            buf[1] = (n/10)  % 10 + '0';
            buf[2] = (n)     % 10 + '0';
            buf[3] = 0;
            const char* start = buf + (3 - len);
            float scale = 1.5f;
            float tw = len * 6 * scale;
            // Shadow
            draw_string(x + slotSize - tw - 2 + 1,
                        startY + slotSize - 7*scale - 2 + 1,
                        scale, start, 0, 0, 0);
            // Text
            draw_string(x + slotSize - tw - 2,
                        startY + slotSize - 7*scale - 2,
                        scale, start, 255, 255, 255);
        }
    }
}

// ─── Health Hearts ───────────────────────────────────────────────────────────

static void draw_heart(float x, float y, float size, int filled) {
    // Heart shape built from rectangles:
    // Two bumps on top + a diamond point at bottom
    float s = size;
    float hs = s * 0.5f;
    float qs = s * 0.25f;

    if (filled) {
        // Full heart — bright red with highlight
        // Main body
        draw_rect(x,        y+qs,  s,    s*0.6f, 180, 20, 20, 255);
        // Left bump
        draw_rect(x,        y,     hs,   hs,     180, 20, 20, 255);
        // Right bump
        draw_rect(x+hs,     y,     hs,   hs,     180, 20, 20, 255);
        // Bottom point
        draw_rect(x+qs,     y+s*0.7f, hs, s*0.35f, 180, 20, 20, 255);
        draw_rect(x+qs*1.5f,y+s,   qs,  qs*0.5f, 180, 20, 20, 255);
        // Highlight (top-left of each bump)
        draw_rect(x+qs*0.5f, y+qs*0.5f, qs, qs, 220, 80, 80, 200);
        draw_rect(x+hs+qs*0.5f, y+qs*0.5f, qs, qs, 220, 80, 80, 200);
    } else {
        // Empty heart — dark outline only
        draw_rect(x,        y+qs,  s,    s*0.6f, 60, 10, 10, 200);
        draw_rect(x,        y,     hs,   hs,     60, 10, 10, 200);
        draw_rect(x+hs,     y,     hs,   hs,     60, 10, 10, 200);
        draw_rect(x+qs,     y+s*0.7f, hs, s*0.35f, 60, 10, 10, 200);
        draw_rect(x+qs*1.5f,y+s,   qs,  qs*0.5f, 60, 10, 10, 200);
    }
}

void GUI_DrawHealth(GXRModeObj* rmode, GUIState* gui) {
    int hearts     = 10;
    float heartSize = 14.0f;
    float gap      = 3.0f;
    // Align left edge with hotbar left edge
    float slotSize  = 40.0f;
    float slotGap   = 4.0f;
    float hotbarW   = HOTBAR_SLOTS * slotSize + (HOTBAR_SLOTS - 1) * slotGap;
    float startX    = (SW() - hotbarW) / 2.0f;
    float startY    = SH() - 72.0f; // above hotbar

    // Shadow pass
    for (int i = 0; i < hearts; i++) {
        float x = startX + i * (heartSize + gap);
        draw_rect(x+1, startY+1, heartSize, heartSize*1.1f, 0, 0, 0, 100);
    }

    // Heart pass — 2 HP per heart
    for (int i = 0; i < hearts; i++) {
        float x = startX + i * (heartSize + gap);
        int hp_this_heart = gui->health - i * 2;
        int filled = (hp_this_heart >= 2) ? 1 : 0;
        // Half heart
        if (hp_this_heart == 1) {
            // Draw half: just left side filled
            draw_heart(x, startY, heartSize, 0); // empty base
            // Left half overlay in red
            draw_rect(x,       startY+heartSize*0.25f, heartSize*0.5f, heartSize*0.6f, 180,20,20,255);
            draw_rect(x,       startY,                 heartSize*0.5f, heartSize*0.5f, 180,20,20,255);
            draw_rect(x+heartSize*0.25f, startY+heartSize*0.7f, heartSize*0.25f, heartSize*0.35f, 180,20,20,255);
        } else {
            draw_heart(x, startY, heartSize, filled);
        }
    }
}

// ─── Debug Info ──────────────────────────────────────────────────────────────

void GUI_DrawDebug(GXRModeObj* rmode, float x, float y, float z, int fps) {
    // Without a font we render a small visual FPS indicator:
    // A row of thin bars whose height represents FPS (max 60)
    // This is honest about GameCube's no-font constraint without texture loading.

    float barAreaX = 8.0f;
    float barAreaY = 8.0f;
    float barW     = 3.0f;
    float barGap   = 1.0f;
    float maxH     = 40.0f;
    int   bars     = 20;
    int   fpsClamp = fps > 60 ? 60 : fps;

    // Background panel
    draw_rect(barAreaX - 2, barAreaY - 2,
              bars * (barW + barGap) + 4, maxH + 4,
              0, 0, 0, 140);

    // FPS bars — filled proportion = fps/60
    for (int i = 0; i < bars; i++) {
        float threshold = (float)(i + 1) / bars; // 0.05 to 1.0
        float ratio     = (float)fpsClamp / 60.0f;
        u8 cr = ratio > 0.5f ? (u8)((1.0f - ratio) * 2 * 255) : 220;
        u8 cg = ratio > 0.5f ? 200 : (u8)(ratio * 2 * 200);
        u8 alpha = (threshold <= ratio) ? 230 : 40;
        float bx = barAreaX + i * (barW + barGap);
        draw_rect(bx, barAreaY + (maxH - maxH * threshold),
                  barW, maxH * threshold, cr, cg, 0, alpha);
    }
}

// ─── Public rect helper ───────────────────────────────────────────────────────
void GUI_DrawRect(int x, int y, int w, int h, u8 r, u8 g, u8 b, u8 a) {
    draw_rect((float)x, (float)y, (float)w, (float)h, r, g, b, a);
}

void GUI_DrawString(float x, float y, float scale, const char* str, u8 r, u8 g, u8 b) {
    draw_string(x, y, scale, str, r, g, b);
}

// ─── Pause Menu ───────────────────────────────────────────────────────────────

// Draw a single pixel-art character using small rects
// Each char is 5 wide x 7 tall at given scale
static void draw_char(float x, float y, float scale, char c, u8 r, u8 g, u8 b) {
    // Bitmaps for A-Z and space (5x7 pixels, row 0 = top)
    // Each row is a 5-bit mask
    static const u8 font[][7] = {
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x00}, // A  [0]
        {0x1E,0x11,0x11,0x1E,0x11,0x1E,0x00}, // B  [1]
        {0x0E,0x11,0x10,0x10,0x11,0x0E,0x00}, // C  [2]
        {0x1E,0x11,0x11,0x11,0x11,0x1E,0x00}, // D  [3]
        {0x1F,0x10,0x10,0x1E,0x10,0x1F,0x00}, // E  [4]
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x00}, // F  [5]
        {0x0E,0x11,0x10,0x17,0x11,0x0F,0x00}, // G  [6]
        {0x11,0x11,0x11,0x1F,0x11,0x11,0x00}, // H  [7]
        {0x0E,0x04,0x04,0x04,0x04,0x0E,0x00}, // I  [8]
        {0x01,0x01,0x01,0x01,0x11,0x0E,0x00}, // J  [9]
        {0x11,0x12,0x14,0x18,0x14,0x13,0x00}, // K  [10]
        {0x10,0x10,0x10,0x10,0x10,0x1F,0x00}, // L  [11]
        {0x11,0x1B,0x15,0x11,0x11,0x11,0x00}, // M  [12]
        {0x11,0x19,0x15,0x13,0x11,0x11,0x00}, // N  [13]
        {0x0E,0x11,0x11,0x11,0x11,0x0E,0x00}, // O  [14]
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x00}, // P  [15]
        {0x0E,0x11,0x11,0x15,0x12,0x0D,0x00}, // Q  [16]
        {0x1E,0x11,0x11,0x1E,0x14,0x12,0x00}, // R  [17]
        {0x0F,0x10,0x10,0x0E,0x01,0x1E,0x00}, // S  [18]
        {0x1F,0x04,0x04,0x04,0x04,0x04,0x00}, // T  [19]
        {0x11,0x11,0x11,0x11,0x11,0x0E,0x00}, // U  [20]
        {0x11,0x11,0x11,0x11,0x0A,0x04,0x00}, // V  [21]
        {0x11,0x11,0x11,0x15,0x1B,0x11,0x00}, // W  [22]
        {0x11,0x11,0x0A,0x04,0x0A,0x11,0x00}, // X  [23]
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x00}, // Y  [24]
        {0x1F,0x01,0x02,0x04,0x08,0x1F,0x00}, // Z  [25]
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '[26]
        {0x0E,0x13,0x13,0x15,0x19,0x19,0x0E}, // 0  [27]
        {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1  [28]
        {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, // 2  [29]
        {0x1F,0x01,0x02,0x06,0x01,0x11,0x0E}, // 3  [30]
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4  [31]
        {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5  [32]
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // 6  [33]
        {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7  [34]
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8  [35]
        {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 9  [36]
    };

    int idx = -1;
    if (c >= 'A' && c <= 'Z') idx = c - 'A';
    else if (c >= 'a' && c <= 'z') idx = c - 'a';
    else if (c == ' ') idx = 26;
    else if (c >= '0' && c <= '9') idx = 27 + (c - '0');
    if (idx < 0) return;

    for (int row = 0; row < 7; row++) {
        u8 bits = font[idx][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (0x10 >> col)) {
                draw_rect(x + col*scale, y + row*scale, scale, scale, r, g, b, 255);
            }
        }
    }
}

static void draw_string(float x, float y, float scale, const char* str, u8 r, u8 g, u8 b) {
    float cx = x;
    for (int i = 0; str[i]; i++) {
        draw_char(cx, y, scale, str[i], r, g, b);
        cx += 6 * scale; // 5 wide + 1 gap
    }
}

void GUI_DrawPauseMenu(GXRModeObj* rmode, GUIState* gui) {
    // Semi-transparent dark overlay
    draw_rect(0, 0, (int)SW(), (int)SH(), 0, 0, 0, 160);

    // Menu panel
    float panelW = 200.0f, panelH = 185.0f;
    float panelX = (SW() - panelW) / 2.0f;
    float panelY = (SH() - panelH) / 2.0f;

    // Panel background + border
    draw_rect(panelX, panelY, panelW, panelH, 30, 30, 30, 220);
    draw_rect_outline(panelX, panelY, panelW, panelH, 2, 180, 180, 180, 255);

    // Title: "PAUSED"
    float titleScale = 2.5f;
    float titleW = 6 * 6 * titleScale;
    draw_string(panelX + (panelW - titleW) / 2.0f, panelY + 14.0f,
                titleScale, "PAUSED", 220, 220, 220);

    // Divider
    draw_rect(panelX + 10, panelY + 44, panelW - 20, 2, 120, 120, 120, 200);

    // Menu items
    static const char* items[] = {"RESUME", "SETTINGS", "RESPAWN", "QUIT"};
    for (int i = 0; i < PAUSE_ITEM_COUNT; i++) {
        float itemY = panelY + 56.0f + i * 28.0f;
        float itemScale = 2.0f;

        // Highlight selected item
        if (i == gui->pauseCursor) {
            draw_rect(panelX + 8, itemY - 4, panelW - 16, 7*itemScale + 8,
                      80, 80, 180, 180);
            draw_rect_outline(panelX + 8, itemY - 4, panelW - 16, 7*itemScale + 8,
                              1, 140, 140, 255, 255);
            // Arrow indicator
            draw_rect(panelX + 14, itemY + 6, 4, 4, 255, 255, 100, 255);
        }

        float textW = (float)(__builtin_strlen(items[i])) * 6.0f * itemScale;
        u8 tr = (i == gui->pauseCursor) ? 255 : 180;
        u8 tg = (i == gui->pauseCursor) ? 255 : 180;
        u8 tb = (i == gui->pauseCursor) ? 100 : 180;
        draw_string(panelX + (panelW - textW) / 2.0f, itemY, itemScale,
                    items[i], tr, tg, tb);
    }
}

// ─── Settings menu ───────────────────────────────────────────────────────────

void GUI_DrawSettings(GXRModeObj* rmode, GUIState* gui) {
    (void)rmode;

    draw_rect(0, 0, (int)SW(), (int)SH(), 0, 0, 0, 160);

    float panelW = 260.0f, panelH = 270.0f;
    float panelX = (SW() - panelW) / 2.0f;
    float panelY = (SH() - panelH) / 2.0f;

    draw_rect(panelX, panelY, panelW, panelH, 30, 30, 30, 220);
    draw_rect_outline(panelX, panelY, panelW, panelH, 2, 180, 180, 180, 255);

    float titleScale = 2.5f;
    float titleW = 8 * 6 * titleScale;
    draw_string(panelX + (panelW - titleW) / 2.0f, panelY + 14.0f,
                titleScale, "SETTINGS", 220, 220, 220);
    draw_rect(panelX + 10, panelY + 44, panelW - 20, 2, 120, 120, 120, 200);

    // Setting labels and current values
    struct { const char* label; float val; float min; float max; float step; } settings[] = {
        { "FOG START",   g_config.fogStart,           5.0f,  60.0f,  5.0f  },
        { "FOG END",     g_config.fogEnd,             10.0f, 120.0f, 5.0f  },
        { "SENSITIVITY", g_config.sensitivity,        0.5f,  8.0f,   0.5f  },
        { "MOVE SPEED",  g_config.moveSpeed,          0.05f, 0.30f,  0.01f },
        { "WIDESCREEN",  (float)g_config.widescreen,  0.0f,  1.0f,   1.0f  },
    };

    for (int i = 0; i < SETTING_ITEM_COUNT; i++) {
        float itemY = panelY + 56.0f + i * 34.0f;
        float scale = 1.5f;
        u8 tr = (i == gui->settingsCursor) ? 255 : 180;
        u8 tg = (i == gui->settingsCursor) ? 255 : 180;
        u8 tb = (i == gui->settingsCursor) ? 100 : 180;

        if (i == gui->settingsCursor) {
            draw_rect(panelX + 8, itemY - 4, panelW - 16, 7*scale + 18,
                      80, 80, 180, 160);
            draw_rect_outline(panelX + 8, itemY - 4, panelW - 16, 7*scale + 18,
                              1, 140, 140, 255, 255);
        }

        // Label
        draw_string(panelX + 16.0f, itemY, scale, settings[i].label, tr, tg, tb);

        if (i == SETTING_ITEM_WIDESCREEN) {
            // Toggle: show ON / OFF
            const char* tog = g_config.widescreen ? "ON" : "OFF";
            u8 tor = g_config.widescreen ? 100 : 180;
            u8 tog2 = g_config.widescreen ? 255 : 180;
            float vw = __builtin_strlen(tog) * 6.0f * scale;
            draw_string(panelX + panelW - vw - 16.0f, itemY, scale, tog, tor, tog2, tr);
        } else {
            // Value bar background
            float barX = panelX + 16.0f;
            float barY = itemY + 12.0f;
            float barW = panelW - 32.0f;
            float barH = 6.0f;
            draw_rect(barX, barY, barW, barH, 60, 60, 60, 200);
            float t = (settings[i].val - settings[i].min) / (settings[i].max - settings[i].min);
            if (t < 0) t = 0; if (t > 1) t = 1;
            draw_rect(barX, barY, barW * t, barH, tr, tg, tb, 220);
            char valbuf[16];
            if (settings[i].step >= 0.05f)
                snprintf(valbuf, sizeof(valbuf), "%.1f", settings[i].val);
            else
                snprintf(valbuf, sizeof(valbuf), "%.2f", settings[i].val);
            float vw = __builtin_strlen(valbuf) * 6.0f * scale;
            draw_string(panelX + panelW - vw - 16.0f, itemY, scale, valbuf, tr, tg, tb);
        }
    }

    // Resource pack selector
    {
        int i = SETTING_ITEM_RESOURCE_PACK;
        float itemY = panelY + 56.0f + i * 34.0f;
        float scale = 1.5f;
        u8 tr2 = (i == gui->settingsCursor) ? 255 : 180;
        u8 tg2 = (i == gui->settingsCursor) ? 255 : 180;
        u8 tb2 = (i == gui->settingsCursor) ? 100 : 180;
        if (i == gui->settingsCursor)
            draw_rect(panelX+8, itemY-4, panelW-16, 7*scale+18, 80,80,180,160);
        draw_string(panelX + 16.0f, itemY, scale, "RESOURCE PACK", tr2, tg2, tb2);
        const char* packName = Tex_GetPackName(Tex_GetCurrentPack());
        float vw = __builtin_strlen(packName) * 6.0f * scale;
        draw_string(panelX + panelW - vw - 16.0f, itemY, scale, packName, tr2, tg2, tb2);
        // Left/right arrows hint
        draw_string(panelX + 16.0f, itemY + 12.0f, 1.2f, "< >", 120, 120, 120);
    }

    // Back hint
    draw_string(panelX + 10.0f, panelY + panelH - 20.0f, 1.5f,
                "B/ESC: BACK", 140, 140, 140);
}

// ─── Score display ────────────────────────────────────────────────────────────

// ─── Inventory Screen ────────────────────────────────────────────────────────

void GUI_DrawInventory(GXRModeObj* rmode, GUIState* gui) {
    float slotSize = 36.0f;
    float pad      = 3.0f;
    float gap      = 4.0f;
    float panelW   = INV_COLS * slotSize + 2.0f;
    float hotbarH  = slotSize + 4.0f;
    float mainH    = INV_ROWS * slotSize;
    float panelH   = mainH + gap + hotbarH + 4.0f;
    float panelX   = (SW() - panelW) / 2.0f;
    float panelY   = (SH() - panelH) / 2.0f;

    // Dark overlay
    draw_rect(0, 0, (int)SW(), (int)SH(), 0, 0, 0, 160);

    // ── Main inventory 3x9 ───────────────────────────────────────────
    for (int row = 0; row < INV_ROWS; row++) {
        for (int col = 0; col < INV_COLS; col++) {
            int idx = HOTBAR_SLOTS + row * INV_COLS + col;
            float sx = panelX + 1.0f + col * slotSize;
            float sy = panelY + row * slotSize;
            draw_rect(sx, sy, slotSize, slotSize, 30, 30, 30, 220);
            int isCursor = (gui->invCursorX == col && gui->invCursorY == row);
            if (isCursor)
                draw_rect_outline(sx, sy, slotSize, slotSize, 2, 255,255,255,255);
            else
                draw_rect_outline(sx, sy, slotSize, slotSize, 1, 55,55,55,200);
            if (gui->slotCount[idx] > 0 && gui->slotBlock[idx] != BLOCK_AIR)
                draw_block_icon(sx+pad, sy+pad, slotSize-pad*2, gui->slotBlock[idx]);
            if (gui->slotCount[idx] > 0) {
                char buf[8]; int n = gui->slotCount[idx];
                int len=(n>=100)?3:(n>=10)?2:1;
                buf[0]=(n/100)%10+'0'; buf[1]=(n/10)%10+'0';
                buf[2]=n%10+'0'; buf[3]=0;
                const char* ns=buf+(3-len); float sc=1.0f,tw=len*6*sc;
                draw_string(sx+slotSize-tw-2+1,sy+slotSize-7*sc-2+1,sc,ns,0,0,0);
                draw_string(sx+slotSize-tw-2,  sy+slotSize-7*sc-2,  sc,ns,255,255,255);
            }
        }
    }

    // ── Hotbar row ────────────────────────────────────────────────────
    float barY = panelY + mainH + gap;
    float barX = panelX;
    draw_rect(barX, barY, panelW, hotbarH, 30, 30, 30, 220);
    draw_rect_outline(barX, barY, panelW, hotbarH, 1, 85,85,85,255);
    draw_rect(barX+1, barY+1, panelW-2, 1, 130,130,130,160);
    for (int col = 0; col < HOTBAR_SLOTS; col++) {
        float sx = barX + 1.0f + col * slotSize;
        float sy = barY + 2.0f;
        int isCursor = (gui->invCursorX == col && gui->invCursorY == INV_ROWS);
        if (isCursor)
            draw_rect_outline(sx, sy, slotSize, slotSize, 2, 255,255,255,255);
        else if (col == gui->selectedSlot)
            draw_rect_outline(sx, sy, slotSize, slotSize, 1, 200,200,100,255);
        else
            draw_rect_outline(sx, sy, slotSize, slotSize, 1, 55,55,55,200);
        if (gui->slotCount[col] > 0 && gui->slotBlock[col] != BLOCK_AIR)
            draw_block_icon(sx+pad, sy+pad, slotSize-pad*2, gui->slotBlock[col]);
        if (gui->slotCount[col] > 0) {
            char buf[8]; int n = gui->slotCount[col];
            int len=(n>=100)?3:(n>=10)?2:1;
            buf[0]=(n/100)%10+'0'; buf[1]=(n/10)%10+'0';
            buf[2]=n%10+'0'; buf[3]=0;
            const char* ns=buf+(3-len); float sc=1.0f,tw=len*6*sc;
            draw_string(sx+slotSize-tw-2+1,sy+slotSize-7*sc-2+1,sc,ns,0,0,0);
            draw_string(sx+slotSize-tw-2,  sy+slotSize-7*sc-2,  sc,ns,255,255,255);
        }
    }

    // ── Held item floats above cursor ─────────────────────────────────
    if (gui->heldItemCount > 0 && gui->heldItemBlock != BLOCK_AIR) {
        float cx, cy;
        if (gui->invCursorY < INV_ROWS) {
            cx = panelX + 1.0f + gui->invCursorX * slotSize;
            cy = panelY + gui->invCursorY * slotSize;
        } else {
            cx = barX + 1.0f + gui->invCursorX * slotSize;
            cy = barY + 2.0f;
        }
        draw_block_icon(cx+pad, cy+pad-4, slotSize-pad*2, gui->heldItemBlock);
        char buf[8]; int n = gui->heldItemCount;
        int len=(n>=100)?3:(n>=10)?2:1;
        buf[0]=(n/100)%10+'0'; buf[1]=(n/10)%10+'0';
        buf[2]=n%10+'0'; buf[3]=0;
        const char* ns=buf+(3-len); float sc=1.0f,tw=len*6*sc;
        draw_string(cx+slotSize-tw-2, cy+slotSize-7*sc-6, sc, ns, 255,255,100);
    }

    // ── Labels ────────────────────────────────────────────────────────
    draw_string(panelX+2, panelY-14, 1.5f, "INVENTORY", 200,200,200);
    // ── Crafting grid (top-right of panel) ───────────
    float cSlot=28.0f, cGap=2.0f, arrowW=18.0f, outSz=30.0f;
    float craftW = 2*(cSlot+cGap) + arrowW + outSz + cGap*2;
    float craftX = panelX + panelW - craftW - 4.0f;
    float craftY = panelY - 4.0f - 2*(cSlot+cGap) - 14.0f;
    draw_string(craftX, craftY-12.0f, 1.5f, "CRAFT", 200,200,150);
    for (int row=0;row<2;row++) for (int col=0;col<2;col++) {
        int ci=row*2+col;
        float sx=craftX+col*(cSlot+cGap), sy=craftY+row*(cSlot+cGap);
        draw_rect(sx,sy,cSlot,cSlot,30,30,30,220);
        int isCursor=gui->craftCursorOn&&(gui->craftCursorIdx==ci);
        if(isCursor) draw_rect_outline(sx,sy,cSlot,cSlot,2,255,255,255,255);
        else         draw_rect_outline(sx,sy,cSlot,cSlot,1,55,55,55,200);
        if(gui->craftCount[ci]>0&&gui->craftGrid[ci]!=0)
            draw_block_icon(sx+2,sy+2,cSlot-4,gui->craftGrid[ci]);
        if(gui->craftCount[ci]>0){
            char buf[4]; int n=gui->craftCount[ci];
            buf[0]=(n>=10)?'0'+n/10:'0'+n; buf[1]='0'+n%10; buf[2]=0;
            draw_string(sx+cSlot-8,sy+cSlot-9,1.0f,(n>=10)?buf:buf+1,255,255,255);
        }
    }
    // Arrow
    float arrX=craftX+2*(cSlot+cGap)+cGap, arrY=craftY+cSlot*0.5f;
    draw_rect(arrX,arrY-2,arrowW-6,4,200,200,80,255);
    draw_rect(arrX+arrowW-10,arrY-6,8,12,200,200,80,255);
    // Output slot
    float outX=arrX+arrowW, outY=craftY+(2*(cSlot+cGap)-outSz)*0.5f;
    int outCursor=gui->craftCursorOn&&(gui->craftCursorIdx==4);
    if(gui->craftResult!=0) draw_rect(outX,outY,outSz,outSz,50,50,20,220);
    else                    draw_rect(outX,outY,outSz,outSz,30,30,30,220);
    if(outCursor) draw_rect_outline(outX,outY,outSz,outSz,2,255,255,100,255);
    else          draw_rect_outline(outX,outY,outSz,outSz,1,100,100,55,200);
    if(gui->craftResult!=0){
        draw_block_icon(outX+3,outY+3,outSz-6,gui->craftResult);
        char buf[4]; int n=gui->craftResultCount;
        buf[0]=(n>=10)?'0'+n/10:'0'+n; buf[1]='0'+n%10; buf[2]=0;
        draw_string(outX+outSz-9,outY+outSz-10,1.0f,(n>=10)?buf:buf+1,255,255,100);
    }

    draw_string(panelX+2, panelY+panelH+4, 1.5f,
                "A GRAB  Y MOVE  X CLOSE  LB CRAFT", 140,140,140);
}

void GUI_DrawScore(GXRModeObj* rmode, GUIState* gui) {
    // "SCORE: XXXXXX" top right corner
    char buf[16];
    int s = gui->score;
    // Format score with leading zeros to 6 digits like classic Minecraft
    buf[0] = (s/100000)%10 + '0';
    buf[1] = (s/10000) %10 + '0';
    buf[2] = (s/1000)  %10 + '0';
    buf[3] = (s/100)   %10 + '0';
    buf[4] = (s/10)    %10 + '0';
    buf[5] = (s)       %10 + '0';
    buf[6] = 0;

    float scale = 2.0f;
    float labelW = 6 * 6 * scale;  // "SCORE " = 6 chars
    float numW   = 6 * 6 * scale;

    float x = SW() - labelW - numW - 10;
    float y = 8.0f;

    // Shadow
    draw_string(x+1, y+1, scale, "SCORE", 0, 0, 0);
    draw_string(x + labelW + 1, y+1, scale, buf, 0, 0, 0);
    // Text
    draw_string(x, y, scale, "SCORE", 220, 220, 80);
    draw_string(x + labelW, y, scale, buf, 255, 255, 255);
}