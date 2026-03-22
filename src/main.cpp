#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <malloc.h>
#include <string.h>
#include "world.h"
#include "camera.h"
#include "player.h"
#include "gui.h"
#include "textures.h"
#include "raycast.h"

#define FIFO_SIZE (256 * 1024)
static void *frameBuffer = NULL;
static GXRModeObj *rmode = NULL;

int main(int argc, char **argv) {
    VIDEO_Init();
    PAD_Init();

    rmode = VIDEO_GetPreferredMode(NULL);
    frameBuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    void *gp_fifo = memalign(32, FIFO_SIZE);
    memset(gp_fifo, 0, FIFO_SIZE);
    GX_Init(gp_fifo, FIFO_SIZE);

    GXColor background = {0x87, 0xCE, 0xEB, 0xff}; // sky blue
    GX_SetCopyClear(background, 0x00ffffff);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ,  GX_F32,   0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST,   GX_F32,   0);
    GX_SetNumChans(1);
    // TEV: modulate texture by vertex color (gives us shading)
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_BACK);

    Mtx44 projection;
    guPerspective(projection, 70,
        (f32)rmode->fbWidth / rmode->efbHeight, 0.05F, 300.0F);
    GX_LoadProjectionMtx(projection, GX_PERSPECTIVE);

    // Hardware fog — blends to sky color at distance
    // GX_FOG_LIN: linear fog from start to end distance
    GXColor fogColor = {0x87, 0xCE, 0xEB, 0xFF}; // match sky blue
    GX_SetFog(GX_FOG_LIN, 20.0f, 80.0f, 0.05f, 300.0f, fogColor);
    // Range adjustment disabled — avoids NULL pointer crash
    GX_SetFogRangeAdj(GX_DISABLE, 0, NULL);

    // Generate world
    static World myWorld;
    World_Generate(&myWorld);

    // Upload textures to GX
    Tex_Init();

    // Init player — spawns above grass layer
    static Player myPlayer;
    Player_Init(&myPlayer);

    // Init camera — position will be overridden by player each frame
    FreeCam myCam;
    Camera_Init(&myCam);

    // Init GUI
    GUIState gui;
    GUI_Init(&gui);

    // FPS tracking
    u32 frameCount = 0;
    u32 lastTick   = 0;
    int fps        = 0;

    while(1) {
        PAD_ScanPads();
        u32 down = PAD_ButtonsDown(0);

        if (down & PAD_BUTTON_START) break;

        // D-pad cycles hotbar
        if (down & PAD_BUTTON_LEFT)
            gui.selectedSlot = (gui.selectedSlot - 1 + HOTBAR_SLOTS) % HOTBAR_SLOTS;
        if (down & PAD_BUTTON_RIGHT)
            gui.selectedSlot = (gui.selectedSlot + 1) % HOTBAR_SLOTS;

        // Inputs
        s8 sX  = PAD_StickX(0);
        s8 sY  = PAD_StickY(0);
        s8 cX  = PAD_SubStickX(0);
        s8 cY  = PAD_SubStickY(0);
        int jump = (PAD_ButtonsHeld(0) & PAD_BUTTON_A) ? 1 : 0;

        // Update look direction first (yaw needed for movement direction)
        Camera_UpdateLook(&myCam, cX, cY);

        // Update player physics + collision
        Player_Update(&myPlayer, &myWorld, sX, sY, jump, myCam.yaw);

        // Sync camera to player eyes
        Player_ApplyToCamera(&myPlayer, &myCam);

        // FPS counter
        frameCount++;
        u32 now = (u32)(ticks_to_secs(gettime()));
        if (now != lastTick) {
            fps        = frameCount;
            frameCount = 0;
            lastTick   = now;
        }

        // ── 3D World ────────────────────────────────────────────────────────
        GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        World_RebuildDirty(&myWorld);
        Camera_Apply(&myCam);
        World_Render(&myWorld);

        // Raycast from camera
        RayResult ray = Raycast(&myWorld, myCam.pos, myCam.forward);
        Raycast_DrawHighlight(&ray);

        // L trigger = break, R trigger = place (fire once per press)
        static u8 l_prev = 0, r_prev = 0;
        u8 l_now = PAD_TriggerL(0);
        u8 r_now = PAD_TriggerR(0);

        if (r_now > 100 && r_prev <= 100 && ray.hit)
            World_SetBlock(&myWorld, ray.bx, ray.by, ray.bz, BLOCK_AIR);

        if (l_now > 100 && l_prev <= 100 && ray.hit) {
            // Map hotbar slot to block type
            static const u8 slot_blocks[] = {
                BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE,
                BLOCK_WOOD,  BLOCK_LEAF, BLOCK_STONE,
                BLOCK_STONE, BLOCK_STONE
            };
            u8 place = slot_blocks[gui.selectedSlot];
            World_SetBlock(&myWorld, ray.px, ray.py, ray.pz, place);
        }

        l_prev = l_now;
        r_prev = r_now;

        // ── 2D GUI ──────────────────────────────────────────────────────────
        // Sync survival stats to GUI
        gui.health    = myPlayer.health;
        gui.maxHealth = PLAYER_MAX_HEALTH;

        GUI_Begin2D(rmode);

        // Death screen overlay
        if (myPlayer.dead) {
            // Dark red overlay
            GUI_DrawRect(0, 0, SCREEN_W, SCREEN_H, 120, 0, 0, 100);
            // Respawn countdown bar
            float pct = 1.0f - (float)myPlayer.death_timer / 180.0f;
            GUI_DrawRect(SCREEN_W/2 - 80, SCREEN_H/2,
                         (int)(160 * pct), 12, 200, 50, 50, 220);
            GUI_DrawRect(SCREEN_W/2 - 80, SCREEN_H/2,
                         160, 12, 0, 0, 0, 0); // outline handled by health bar
        }

        GUI_DrawCrosshair(rmode);
        GUI_DrawHotbar(rmode, &gui);
        GUI_DrawHealth(rmode, &gui);
        GUI_DrawDebug(rmode,
            myPlayer.pos.x, myPlayer.pos.y, myPlayer.pos.z, fps);
        GUI_End2D(rmode);

        // ── Present ─────────────────────────────────────────────────────────
        GX_DrawDone();
        GX_CopyDisp(frameBuffer, GX_TRUE);
        GX_Flush();
        VIDEO_SetNextFramebuffer(frameBuffer);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }
    return 0;
}