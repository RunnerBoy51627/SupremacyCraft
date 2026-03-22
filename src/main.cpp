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
    // ── Block hardness (frames to break at base speed) ──────────────────
    // Matches Minecraft approximate hand-break times at 60fps
    static const float block_hardness[] = {
        0,    // AIR
        90,   // GRASS   ~1.5s
        75,   // DIRT    ~1.25s
        200,  // STONE   ~3.3s
        150,  // WOOD    ~2.5s
        30,   // LEAF    ~0.5s
    };

    // Break state
    static int   breakX = -1, breakY = -1, breakZ = -1;
    static float breakProgress = 0.0f; // 0..1

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
        static u8 l_prev = 0, r_prev = 0;

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
            // Update trigger state even while paused so edge-detection
            // doesn't fire the moment the player unpauses
            l_prev = PAD_TriggerL(0);
            r_prev = PAD_TriggerR(0);
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
        Raycast_DrawHighlight(&ray, breakProgress);

        // ── Input ────────────────────────────────────────────────────────
        u8 l_now = PAD_TriggerL(0);
        u8 r_now = PAD_TriggerR(0);
        int triggerSwing = 0;

        // ── R trigger held = break block (hold-to-mine) ──────────────────
        if (r_now > 100 && ray.hit) {
            if (ray.bx == breakX && ray.by == breakY && ray.bz == breakZ) {
                // Same block — advance progress
                u8 blockType = World_GetBlock(&myWorld, ray.bx, ray.by, ray.bz);
                int hIdx = (blockType < 6) ? blockType : 1;
                float hardness = block_hardness[hIdx];
                if (hardness > 0) breakProgress += 1.0f / hardness;
                if (breakProgress > 1.0f) breakProgress = 1.0f;
                // Retrigger swing each cycle so the arm keeps swinging
                // while the block is being mined
                if (handAnim.state == ANIM_IDLE || handAnim.timer >= 14.0f)
                    triggerSwing = 1;

                if (breakProgress >= 1.0f) {
                    // Block fully broken!
                    u8 broken = World_GetBlock(&myWorld, ray.bx, ray.by, ray.bz);
                    World_SetBlock(&myWorld, ray.bx, ray.by, ray.bz, BLOCK_AIR);
                    gui.score += 10;
                    breakProgress = 0.0f;
                    breakX = breakY = breakZ = -1;
                    // Add to inventory — find existing stack or empty slot
                    int added = 0;
                    for (int si = 0; si < INV_SLOTS && !added; si++) {
                        if (gui.slotBlock[si] == broken && gui.slotCount[si] < 64) {
                            gui.slotCount[si]++;
                            added = 1;
                        }
                    }
                    // No existing stack — find first empty slot
                    for (int si = 0; si < INV_SLOTS && !added; si++) {
                        if (gui.slotCount[si] == 0) {
                            gui.slotBlock[si] = broken;
                            gui.slotCount[si] = 1;
                            added = 1;
                        }
                    }
                }
            } else {
                // New block targeted — reset and start fresh
                breakX = ray.bx; breakY = ray.by; breakZ = ray.bz;
                breakProgress = 0.0f;
            }
        } else {
            // Trigger released or no target — reset break progress
            breakProgress = 0.0f;
            breakX = breakY = breakZ = -1;
        }

        // ── L trigger = place block (single press) ───────────────────────
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

        // Feed player bob into hand sway
        Anim_SetBob(&handAnim,
            myPlayer.bobPhase     * (3.14159265f / 180.0f),
            myPlayer.bobIntensity);
        // Tell the hand what block is currently held
        {
            u8 heldBlock = gui.slotBlock[gui.selectedSlot];
            int heldCount = gui.slotCount[gui.selectedSlot];
            // Show block only if the selected slot has items AND is not AIR
            u8 held = (heldCount > 0 && heldBlock != BLOCK_AIR) ? heldBlock : 0;
            Anim_SetHeldBlock(&handAnim, held);
        }
        // Feed camera turn delta so hand lags behind when looking around
        {
            float dYaw   = (cX > 15 || cX < -15) ? (cX / 128.0f) * g_config.sensitivity : 0.0f;
            float dPitch = (cY > 15 || cY < -15) ? (cY / 128.0f) * g_config.sensitivity : 0.0f;
            Anim_SetLook(&handAnim, dYaw, dPitch);
        }
        Anim_Update(&handAnim, triggerSwing, triggerEquip);

        l_prev = l_now;
        r_prev = r_now;

        // ── Hand — drawn before GUI so hotbar/hearts render on top ──────────
        if (!myPlayer.dead && !gui.paused)
            Anim_DrawHand(&handAnim, &myCam);
        GX_LoadPosMtxImm(g_viewMatrix, GX_PNMTX0);

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