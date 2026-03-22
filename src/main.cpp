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
#include "utils.h"
#include "anim.h"

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

    GXColor background = {FOG_R, FOG_G, FOG_B, 0xff}; // sky color from config
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

    // Initialize config (widescreen, FOV, fog, etc.)
    Config_Init();
    Config_ApplyDisplay(rmode);
    Config_ApplyFog();
    GX_SetFogRangeAdj(GX_DISABLE, 0, NULL);

    // Generate world
    static World myWorld;
    World_Generate(&myWorld);

    // Upload textures to GX
    Tex_Init();

    // Init player — find terrain height at spawn x/z so player lands correctly
    static Player myPlayer;
    Player_Init(&myPlayer);
    Player_Respawn(&myPlayer, &myWorld);

    // Init camera — position will be overridden by player each frame
    FreeCam myCam;
    Camera_Init(&myCam);

    // Init GUI
    GUIState gui;
    GUI_Init(&gui);

    // Hand animation state
    HandAnim handAnim;
    Anim_Init(&handAnim);

    // FPS tracking
    u32 frameCount = 0;
    u32 lastTick   = 0;
    int fps        = 0;

    while(1) {
        PAD_ScanPads();
        u32 down = PAD_ButtonsDown(0);

        // START toggles pause
        if (down & PAD_BUTTON_START)
            gui.paused = !gui.paused;



        // Declare all frame variables up front so goto can skip safely
        s8 sX = PAD_StickX(0);
        s8 sY = PAD_StickY(0);
        s8 cX = PAD_SubStickX(0);
        s8 cY = PAD_SubStickY(0);
        int jump = (down & PAD_BUTTON_A) ? 1 : 0;
        u32 now = (u32)(ticks_to_secs(gettime()));

        // ── Pause menu input ─────────────────────────────────────────────
        if (gui.paused) {
            if (down & PAD_BUTTON_UP)
                gui.pauseCursor = (gui.pauseCursor - 1 + PAUSE_ITEM_COUNT) % PAUSE_ITEM_COUNT;
            if (down & PAD_BUTTON_DOWN)
                gui.pauseCursor = (gui.pauseCursor + 1) % PAUSE_ITEM_COUNT;
            if (down & PAD_BUTTON_A) {
                switch (gui.pauseCursor) {
                    case PAUSE_ITEM_RESUME:
                        gui.paused = 0;
                        break;
                    case PAUSE_ITEM_RESPAWN:
                        Player_Respawn(&myPlayer, &myWorld);
                        gui.paused = 0;
                        break;
                    case PAUSE_ITEM_QUIT:
                        goto game_exit;
                }
            }
            jump = 0; // prevent A press from menu carrying into gameplay
            // Skip game update while paused — jump to render
            goto render_frame;
        }

        // ── Dead screen input — A to respawn ─────────────────────────────
        if (myPlayer.dead) {
            if (down & PAD_BUTTON_A)
                Player_Respawn(&myPlayer, &myWorld);
            goto render_frame; // freeze world while dead
        }

        // ── Hotbar / gameplay input ───────────────────────────────────────
        // D-pad cycles hotbar
        if (down & PAD_BUTTON_LEFT)
            gui.selectedSlot = (gui.selectedSlot - 1 + HOTBAR_SLOTS) % HOTBAR_SLOTS;
        if (down & PAD_BUTTON_RIGHT)
            gui.selectedSlot = (gui.selectedSlot + 1) % HOTBAR_SLOTS;

        // Inputs
        // (sX/sY/cX/cY/jump declared above)

        // Update look direction first (yaw needed for movement direction)
        Camera_UpdateLook(&myCam, cX, cY);

        // Update player physics + collision
        Player_Update(&myPlayer, &myWorld, sX, sY, jump, myCam.yaw);

        // Sync camera to player eyes
        Player_ApplyToCamera(&myPlayer, &myCam);

        // FPS counter
        frameCount++;
        // (now declared above)
        if (now != lastTick) {
            fps        = frameCount;
            frameCount = 0;
            lastTick   = now;
        }

        render_frame:
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

        int triggerSwing = 0;

        if (r_now > 100 && r_prev <= 100 && ray.hit) {
            u8 broken = World_GetBlock(&myWorld, ray.bx, ray.by, ray.bz);
            World_SetBlock(&myWorld, ray.bx, ray.by, ray.bz, BLOCK_AIR);
            gui.score += 10;
            triggerSwing = 1;
            // Add to inventory — find matching slot, cap at 64
            for (int si = 0; si < INV_SLOTS; si++) {
                if (gui.slotBlock[si] == broken) {
                    if (gui.slotCount[si] < 64)
                        gui.slotCount[si]++;
                    break;
                }
            }
        }

        if (l_now > 100 && l_prev <= 100 && ray.hit) {
            u8 place = gui.slotBlock[gui.selectedSlot];
            if (gui.slotCount[gui.selectedSlot] > 0 && place != BLOCK_AIR) {
                float half = PLAYER_WIDTH * 0.5f;
                bool overlapX = (ray.px + 1 > myPlayer.pos.x - half) && (ray.px < myPlayer.pos.x + half);
                bool overlapY = (ray.py + 1 > myPlayer.pos.y)         && (ray.py < myPlayer.pos.y + PLAYER_HEIGHT);
                bool overlapZ = (ray.pz + 1 > myPlayer.pos.z - half)  && (ray.pz < myPlayer.pos.z + half);
                bool insidePlayer = overlapX && overlapY && overlapZ;

                if (!insidePlayer) {
                    if (World_SetBlock(&myWorld, ray.px, ray.py, ray.pz, place)) {
                        triggerSwing = 1;
                        gui.slotCount[gui.selectedSlot]--;
                        if (gui.slotCount[gui.selectedSlot] < 0)
                            gui.slotCount[gui.selectedSlot] = 0;
                    }
                }
            }
        }

        // Trigger equip animation on hotbar slot change
        static int prevSlot = 0;
        int triggerEquip = (gui.selectedSlot != prevSlot) ? 1 : 0;
        prevSlot = gui.selectedSlot;

        Anim_Update(&handAnim, triggerSwing, triggerEquip);

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
            GUI_DrawRect(0, 0, SCREEN_W, SCREEN_H, 120, 0, 0, 130);
            // "YOU DIED" — centred
            float scale = 3.0f;
            float msgW = (float)(8 * 6) * scale; // "YOU DIED" = 8 chars
            GUI_DrawString(SCREEN_W/2 - msgW/2, SCREEN_H/2 - 30, scale,
                           "YOU DIED", 255, 80, 80);
            // "PRESS A" prompt below
            float scale2 = 2.0f;
            float promptW = (float)(7 * 6) * scale2; // "PRESS A" = 7 chars
            GUI_DrawString(SCREEN_W/2 - promptW/2, SCREEN_H/2 + 10, scale2,
                           "PRESS A", 220, 220, 220);
        }

        GUI_DrawScore(rmode, &gui);
        GUI_DrawCrosshair(rmode);
        if (gui.paused) GUI_DrawPauseMenu(rmode, &gui);
        GUI_DrawHotbar(rmode, &gui);
        GUI_DrawHealth(rmode, &gui);
        GUI_DrawDebug(rmode,
            myPlayer.pos.x, myPlayer.pos.y, myPlayer.pos.z, fps);
        GUI_End2D(rmode);

        // ── Hand — drawn last so it's always on top of GUI ──────────────────
        // GUI_End2D already restored perspective projection and depth state.
        // We draw the hand in eye-space (no view matrix), then reload the
        // view matrix so the next frame's world render starts clean.
        if (!myPlayer.dead && !gui.paused)
            Anim_DrawHand(&handAnim, &myCam);
        // Reload view matrix into PNMTX0 so next frame's world render is clean
        GX_LoadPosMtxImm(g_viewMatrix, GX_PNMTX0);

        // ── Present ─────────────────────────────────────────────────────────
        GX_DrawDone();
        GX_CopyDisp(frameBuffer, GX_TRUE);
        GX_Flush();
        VIDEO_SetNextFramebuffer(frameBuffer);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }
    game_exit:
    return 0;
}