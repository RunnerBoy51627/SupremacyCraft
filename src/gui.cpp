#include "gui.h"
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
        gui->slotBlock[i] = BLOCK_AIR;  // survival: all slots empty
        gui->slotCount[i] = 0;
    }
}

// ─── Projection Switch ───────────────────────────────────────────────────────

void GUI_Begin2D(GXRModeObj* rmode) {
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
    guOrtho(ortho, 0, SCREEN_H, 0, SCREEN_W, 0, 1); // SCREEN_W/H set by widescreen config
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
    float cx = SCREEN_W / 2.0f;
    float cy = SCREEN_H / 2.0f;
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

static void get_block_colors(u8 block,
    u8* tr, u8* tg, u8* tb,   // top face
    u8* lr, u8* lg, u8* lb,   // left face
    u8* rr, u8* rg, u8* rb)   // right face
{
    switch(block) {
        case BLOCK_GRASS:
            *tr=106; *tg=178; *tb=80;   // green top
            *lr=100; *lg=70;  *lb=40;   // dirt left (darker)
            *rr=120; *rg=85;  *rb=50;   // dirt right
            break;
        case BLOCK_DIRT:
            *tr=134; *tg=96;  *tb=67;
            *lr=100; *lg=70;  *lb=45;
            *rr=120; *rg=85;  *rb=55;
            break;
        case BLOCK_STONE:
            *tr=128; *tg=128; *tb=128;
            *lr=90;  *lg=90;  *lb=90;
            *rr=110; *rg=110; *rb=110;
            break;
        case BLOCK_WOOD:
            *tr=160; *tg=130; *tb=80;
            *lr=100; *lg=65;  *lb=30;
            *rr=120; *rg=80;  *rb=40;
            break;
        case BLOCK_LEAF:
            *tr=50;  *tg=120; *tb=35;
            *lr=35;  *lg=90;  *lb=25;
            *rr=42;  *rg=105; *rb=30;
            break;
        default:
            *tr=*tg=*tb=180;
            *lr=*lg=*lb=120;
            *rr=*rg=*rb=150;
    }
}

static void draw_block_icon(float x, float y, float size, u8 block) {
    // Layout: top face takes top 1/3, two side faces fill the bottom 2/3.
    // Total height = topH + sideH = size, so icon always fits within its box.
    float topH  = size * 0.34f;  // top (squished) face height
    float sideH = size - topH;   // side faces — fills the rest exactly
    float hw    = size * 0.5f;   // each face is half the total width

    u8 tr,tg,tb, lr,lg,lb, rr,rg,rb;
    get_block_colors(block, &tr,&tg,&tb, &lr,&lg,&lb, &rr,&rg,&rb);

    // Top face
    draw_rect(x,    y,       size, topH,  tr, tg, tb, 255);
    // Left face
    draw_rect(x,    y+topH,  hw,   sideH, lr, lg, lb, 255);
    // Right face
    draw_rect(x+hw, y+topH,  hw,   sideH, rr, rg, rb, 255);
}

// ─── Hotbar ──────────────────────────────────────────────────────────────────

void GUI_DrawHotbar(GXRModeObj* rmode, GUIState* gui) {
    float slotSize = 40.0f;
    float slotGap  = 4.0f;
    float totalW   = INV_SLOTS * slotSize + (INV_SLOTS - 1) * slotGap;
    float startX   = (SCREEN_W - totalW) / 2.0f;
    float startY   = SCREEN_H - slotSize - 12.0f;

    for (int i = 0; i < INV_SLOTS; i++) {
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
    float totalW   = hearts * (heartSize + gap) - gap;
    float startX   = (SCREEN_W - totalW) / 2.0f;
    float startY   = SCREEN_H - 72.0f; // above hotbar

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
    draw_rect(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, 160);

    // Menu panel
    float panelW = 200.0f, panelH = 160.0f;
    float panelX = (SCREEN_W - panelW) / 2.0f;
    float panelY = (SCREEN_H - panelH) / 2.0f;

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
    static const char* items[] = {"RESUME", "RESPAWN", "QUIT"};
    for (int i = 0; i < PAUSE_ITEM_COUNT; i++) {
        float itemY = panelY + 56.0f + i * 32.0f;
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

// ─── Score display ────────────────────────────────────────────────────────────

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

    float x = SCREEN_W - labelW - numW - 10;
    float y = 8.0f;

    // Shadow
    draw_string(x+1, y+1, scale, "SCORE", 0, 0, 0);
    draw_string(x + labelW + 1, y+1, scale, buf, 0, 0, 0);
    // Text
    draw_string(x, y, scale, "SCORE", 220, 220, 80);
    draw_string(x + labelW, y, scale, buf, 255, 255, 255);
}