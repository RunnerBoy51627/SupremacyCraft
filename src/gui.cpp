#include "gui.h"
#include <gccore.h>
#include <stdio.h>

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
    guOrtho(ortho, 0, SCREEN_H, 0, SCREEN_W, 0, 1);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);
}

void GUI_End2D(GXRModeObj* rmode) {
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetCullMode(GX_CULL_BACK);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    // Restore fog for 3D world
    GXColor fogColor = {0x87, 0xCE, 0xEB, 0xFF};
    GX_SetFog(GX_FOG_LIN, 20.0f, 80.0f, 0.05f, 300.0f, fogColor);
    // Restore textured TEV for 3D world
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    Mtx44 projection;
    guPerspective(projection, 60,
        (f32)rmode->fbWidth / rmode->efbHeight, 0.1f, 300.0f);
    GX_LoadProjectionMtx(projection, GX_PERSPECTIVE);
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

// ─── Hotbar ──────────────────────────────────────────────────────────────────

void GUI_DrawHotbar(GXRModeObj* rmode, GUIState* gui) {
    float slotSize  = 40.0f;
    float slotGap   = 4.0f;
    float totalW    = HOTBAR_SLOTS * slotSize + (HOTBAR_SLOTS - 1) * slotGap;
    float startX    = (SCREEN_W - totalW) / 2.0f;
    float startY    = SCREEN_H - slotSize - 12.0f;

    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        float x = startX + i * (slotSize + slotGap);

        // Slot background (dark grey, semi-transparent)
        draw_rect(x, startY, slotSize, slotSize, 40, 40, 40, 180);

        // Slot border — white if selected, grey otherwise
        if (i == gui->selectedSlot) {
            draw_rect_outline(x - 2, startY - 2, slotSize + 4, slotSize + 4, 2,
                              255, 255, 255, 255);
        } else {
            draw_rect_outline(x, startY, slotSize, slotSize, 1,
                              100, 100, 100, 200);
        }
    }
}

// ─── Health ──────────────────────────────────────────────────────────────────

void GUI_DrawHealth(GXRModeObj* rmode, GUIState* gui) {
    // "HP: 20/20" as colored rects forming a simple bar above the hotbar
    float barW    = 160.0f;
    float barH    = 14.0f;
    float barX    = (SCREEN_W - barW) / 2.0f;
    float barY    = SCREEN_H - 100.0f;

    // Background
    draw_rect(barX, barY, barW, barH, 60, 0, 0, 200);

    // Fill based on health ratio
    float ratio = (float)gui->health / (float)gui->maxHealth;
    float fillW = barW * ratio;

    // Color: green -> yellow -> red based on health
    u8 r, g;
    if (ratio > 0.5f) {
        r = (u8)((1.0f - ratio) * 2.0f * 255);
        g = 200;
    } else {
        r = 220;
        g = (u8)(ratio * 2.0f * 180);
    }
    draw_rect(barX, barY, fillW, barH, r, g, 0, 230);

    // Border
    draw_rect_outline(barX, barY, barW, barH, 1, 180, 180, 180, 255);

    // Tiny "HP" label — two small rects forming crude letters
    // (No font system on GC without loading a texture, so we use a pixel bar)
    draw_rect(barX - 24, barY, 6, barH, 200, 200, 200, 255); // H left
    draw_rect(barX - 16, barY, 6, barH, 200, 200, 200, 255); // H right
    draw_rect(barX - 24, barY + barH/2 - 1, 14, 3, 200, 200, 200, 255); // H middle
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