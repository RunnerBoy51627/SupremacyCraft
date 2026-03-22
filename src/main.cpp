#ifdef _PC
  #include <stdio.h>
  #include "platform/pc/gc_compat.h"
  #include "platform/platform.h"
#else
  #include <gccore.h>
  #include <ogc/lwp_watchdog.h>
#endif
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
#include "itemdrop.h"
#include "particle.h"
#include "crafting.h"
#include "input.h"
#include "sound.h"
#include "crash_handler.h"
#include "tnt.h"

#define FIFO_SIZE (512 * 1024)   // 512KB FIFO
static void *frameBuffer = NULL;
static GXRModeObj *rmode = NULL;

int main(int argc, char **argv) {
    CrashHandler_Install(); // install before anything else
#ifdef _PC
    printf("Starting...\n"); fflush(stdout);
#endif
#ifdef _PC
    Platform_InitWindow("My3DSCraft", 640, 480);
    rmode = VIDEO_GetPreferredMode(NULL);
#else
    VIDEO_Init();
    Input_Init();
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
#endif

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

    // Init item drop system
    ItemDrop_Init();
    Particle_Init();

    // Init camera — position will be overridden by player each frame
    FreeCam myCam;
    Camera_Init(&myCam);

    // Init GUI
    GUIState gui;
    GUI_Init(&gui);
    Sound_Init();
    // Starting inventory: 32 TNT in slot 0, flint & steel in slot 1
    gui.slotBlock[0] = BLOCK_TNT;
    gui.slotCount[0] = 32;
    gui.slotBlock[1] = BLOCK_FLINT_STEEL;
    gui.slotCount[1] = 1;
    TNT_Init();

    // Hand animation state
    HandAnim handAnim;
    Anim_Init(&handAnim);

    // FPS tracking
    u32 frameCount = 0;
    u32 lastTick   = 0;
    int fps        = 0;

    while(1) {
#ifdef _PC
        Platform_PollInput();
        if (Platform_ShouldQuit()) goto game_exit;
#endif
        Input_Scan();
        u32 down = Input_ButtonsDown();
        static u8 l_prev = 0, r_prev = 0;

        // START toggles pause
        if (down & PAD_BUTTON_START)
            gui.paused = !gui.paused;
        // X opens/closes inventory (drop held item back on close)
        if (down & PAD_BUTTON_X) {
            if (gui.inventoryOpen) {
                // Drop held item back to first free slot
                if (gui.heldItemCount > 0) {
                    for (int si = 0; si < INV_SLOTS; si++) {
                        if (gui.slotCount[si] == 0) {
                            gui.slotBlock[si] = gui.heldItemBlock;
                            gui.slotCount[si] = gui.heldItemCount;
                            gui.heldItemCount = 0;
                            break;
                        }
                    }
                }
            }
            gui.inventoryOpen = !gui.inventoryOpen;
        }



        // Declare all frame variables up front so goto can skip safely
        s8 sX = Input_StickX();
        s8 sY = Input_StickY();
        s8 cX = Input_CStickX();
        s8 cY = Input_CStickY();
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
            l_prev = Input_TriggerL();
            r_prev = Input_TriggerR();
            // Skip game update while paused — jump to render
            goto render_frame;
        }

        // ── Inventory + Crafting input (blocks ALL gameplay) ────────────────
        if (gui.inventoryOpen) {
            // LB switches focus: inventory <-> crafting grid
            if (down & PAD_TRIGGER_L) {
                gui.craftCursorOn = !gui.craftCursorOn;
                if (gui.craftCursorOn) gui.craftCursorIdx = 0;
            }

            if (!gui.craftCursorOn) {
                // ── Inventory navigation ─────────────────────────────────────
                if (down & PAD_BUTTON_UP)
                    gui.invCursorY = (gui.invCursorY > 0) ? gui.invCursorY-1 : INV_ROWS;
                if (down & PAD_BUTTON_DOWN)
                    gui.invCursorY = (gui.invCursorY < INV_ROWS) ? gui.invCursorY+1 : 0;
                if (down & PAD_BUTTON_LEFT)
                    gui.invCursorX = (gui.invCursorX-1+INV_COLS) % INV_COLS;
                if (down & PAD_BUTTON_RIGHT)
                    gui.invCursorX = (gui.invCursorX+1) % INV_COLS;

                // A = grab whole stack / place whole stack / swap
                // B = grab half stack
                {
                    int idx = (gui.invCursorY < INV_ROWS)
                        ? (HOTBAR_SLOTS + gui.invCursorY * INV_COLS + gui.invCursorX)
                        : gui.invCursorX;
                    if (down & PAD_BUTTON_A) {
                        if (gui.heldItemCount == 0) {
                            // Pick up whole stack
                            gui.heldItemBlock=gui.slotBlock[idx]; gui.heldItemCount=gui.slotCount[idx];
                            gui.slotBlock[idx]=BLOCK_AIR; gui.slotCount[idx]=0;
                        } else if (gui.slotCount[idx]==0 || gui.slotBlock[idx]==gui.heldItemBlock) {
                            int space=64-gui.slotCount[idx];
                            int place=(gui.heldItemCount<space)?gui.heldItemCount:space;
                            gui.slotBlock[idx]=gui.heldItemBlock; gui.slotCount[idx]+=place;
                            gui.heldItemCount-=place;
                            if(gui.heldItemCount==0) gui.heldItemBlock=BLOCK_AIR;
                        } else {
                            u8 tb=gui.slotBlock[idx]; int tc=gui.slotCount[idx];
                            gui.slotBlock[idx]=gui.heldItemBlock; gui.slotCount[idx]=gui.heldItemCount;
                            gui.heldItemBlock=tb; gui.heldItemCount=tc;
                        }
                    } else if (down & PAD_BUTTON_B) {
                        if (gui.heldItemCount == 0 && gui.slotCount[idx] > 0) {
                            // Pick up half
                            int half=(gui.slotCount[idx]+1)/2;
                            gui.heldItemBlock=gui.slotBlock[idx]; gui.heldItemCount=half;
                            gui.slotCount[idx]-=half;
                            if(gui.slotCount[idx]==0) gui.slotBlock[idx]=BLOCK_AIR;
                        } else if (gui.heldItemCount > 0) {
                            // Place one
                            if(gui.slotCount[idx]==0||gui.slotBlock[idx]==gui.heldItemBlock) {
                                if(gui.slotCount[idx]<64) {
                                    gui.slotBlock[idx]=gui.heldItemBlock; gui.slotCount[idx]++;
                                    gui.heldItemCount--;
                                    if(gui.heldItemCount==0) gui.heldItemBlock=BLOCK_AIR;
                                }
                            }
                        }
                    }
                }

                // Y = quick-move to other section
                if (down & PAD_BUTTON_Y) {
                    int idx = (gui.invCursorY < INV_ROWS)
                        ? (HOTBAR_SLOTS + gui.invCursorY * INV_COLS + gui.invCursorX)
                        : gui.invCursorX;
                    if (gui.slotCount[idx] > 0) {
                        int start = (gui.invCursorY < INV_ROWS) ? 0 : HOTBAR_SLOTS;
                        int end   = (gui.invCursorY < INV_ROWS) ? HOTBAR_SLOTS : INV_SLOTS;
                        for (int si=start; si<end && gui.slotCount[idx]>0; si++) {
                            if (gui.slotBlock[si]==gui.slotBlock[idx] && gui.slotCount[si]<64) {
                                int mv=64-gui.slotCount[si]; if(mv>gui.slotCount[idx]) mv=gui.slotCount[idx];
                                gui.slotCount[si]+=mv; gui.slotCount[idx]-=mv;
                            }
                        }
                        for (int si=start; si<end && gui.slotCount[idx]>0; si++) {
                            if (gui.slotCount[si]==0) {
                                gui.slotBlock[si]=gui.slotBlock[idx]; gui.slotCount[si]=gui.slotCount[idx];
                                gui.slotBlock[idx]=BLOCK_AIR; gui.slotCount[idx]=0;
                            }
                        }
                    }
                }
            } else {
                // ── Crafting grid navigation ──────────────────────────────────
                // Slot layout:  0 1
                //               2 3  ==>  4 (output)
                if (down & PAD_BUTTON_UP)
                    gui.craftCursorIdx = (gui.craftCursorIdx==2)?0:(gui.craftCursorIdx==3)?1:(gui.craftCursorIdx==4)?1:gui.craftCursorIdx;
                if (down & PAD_BUTTON_DOWN)
                    gui.craftCursorIdx = (gui.craftCursorIdx==0)?2:(gui.craftCursorIdx==1)?3:(gui.craftCursorIdx==4)?3:gui.craftCursorIdx;
                if (down & PAD_BUTTON_LEFT)
                    gui.craftCursorIdx = (gui.craftCursorIdx==1)?0:(gui.craftCursorIdx==3)?2:(gui.craftCursorIdx==4)?1:gui.craftCursorIdx;
                if (down & PAD_BUTTON_RIGHT)
                    gui.craftCursorIdx = (gui.craftCursorIdx==0)?1:(gui.craftCursorIdx==2)?3:(gui.craftCursorIdx==1||gui.craftCursorIdx==3)?4:gui.craftCursorIdx;

                // A = grab/place whole  B = grab half
                {
                    int ci = gui.craftCursorIdx;
                    u8*  cBlock = (ci<4) ? &gui.craftGrid[ci]  : &gui.craftResult;
                    int* cCount = (ci<4) ? &gui.craftCount[ci] : &gui.craftResultCount;
                    int  isOutput = (ci == 4);

                    // recheck is inlined below as CRAFT_RECHECK

                    if (down & PAD_BUTTON_A) {
                        if (isOutput) {
                            if(gui.craftResult!=0 && gui.heldItemCount==0) {
                                gui.heldItemBlock=gui.craftResult; gui.heldItemCount=gui.craftResultCount;
                                Crafting_Consume(gui.craftGrid,gui.craftCount,4); { CraftResult _cr; if(Crafting_Check2x2(gui.craftGrid,&_cr)){ gui.craftResult=_cr.outBlock; gui.craftResultCount=_cr.outCount; } else { gui.craftResult=0; gui.craftResultCount=0; } }
                            }
                        } else if (gui.heldItemCount==0) {
                            gui.heldItemBlock=gui.craftGrid[ci]; gui.heldItemCount=gui.craftCount[ci];
                            gui.craftGrid[ci]=0; gui.craftCount[ci]=0; { CraftResult _cr; if(Crafting_Check2x2(gui.craftGrid,&_cr)){ gui.craftResult=_cr.outBlock; gui.craftResultCount=_cr.outCount; } else { gui.craftResult=0; gui.craftResultCount=0; } }
                        } else if (gui.craftCount[ci]==0||gui.craftGrid[ci]==gui.heldItemBlock) {
                            int space=64-gui.craftCount[ci];
                            int place=(gui.heldItemCount<space)?gui.heldItemCount:space;
                            gui.craftGrid[ci]=gui.heldItemBlock; gui.craftCount[ci]+=place;
                            gui.heldItemCount-=place; if(gui.heldItemCount==0) gui.heldItemBlock=0;
                            { CraftResult _cr; if(Crafting_Check2x2(gui.craftGrid,&_cr)){ gui.craftResult=_cr.outBlock; gui.craftResultCount=_cr.outCount; } else { gui.craftResult=0; gui.craftResultCount=0; } }
                        } else {
                            u8 tb=gui.craftGrid[ci]; int tc=gui.craftCount[ci];
                            gui.craftGrid[ci]=gui.heldItemBlock; gui.craftCount[ci]=gui.heldItemCount;
                            gui.heldItemBlock=tb; gui.heldItemCount=tc; { CraftResult _cr; if(Crafting_Check2x2(gui.craftGrid,&_cr)){ gui.craftResult=_cr.outBlock; gui.craftResultCount=_cr.outCount; } else { gui.craftResult=0; gui.craftResultCount=0; } }
                        }
                    } else if ((down & PAD_BUTTON_B) && !isOutput) {
                        if(gui.heldItemCount==0 && gui.craftCount[ci]>0) {
                            int half=(gui.craftCount[ci]+1)/2;
                            gui.heldItemBlock=gui.craftGrid[ci]; gui.heldItemCount=half;
                            gui.craftCount[ci]-=half;
                            if(gui.craftCount[ci]==0) gui.craftGrid[ci]=0;
                            { CraftResult _cr; if(Crafting_Check2x2(gui.craftGrid,&_cr)){ gui.craftResult=_cr.outBlock; gui.craftResultCount=_cr.outCount; } else { gui.craftResult=0; gui.craftResultCount=0; } }
                        } else if(gui.heldItemCount>0) {
                            if(gui.craftCount[ci]==0||gui.craftGrid[ci]==gui.heldItemBlock) {
                                if(gui.craftCount[ci]<64) {
                                    gui.craftGrid[ci]=gui.heldItemBlock; gui.craftCount[ci]++;
                                    gui.heldItemCount--; if(gui.heldItemCount==0) gui.heldItemBlock=0;
                                    { CraftResult _cr; if(Crafting_Check2x2(gui.craftGrid,&_cr)){ gui.craftResult=_cr.outBlock; gui.craftResultCount=_cr.outCount; } else { gui.craftResult=0; gui.craftResultCount=0; } }
                                }
                            }
                        }
                    }
                }
            }

            // Block ALL gameplay input
            jump = 0;
            l_prev = Input_TriggerL();
            r_prev = Input_TriggerR();
            goto render_frame;
        }

        // ── Dead screen input — A to respawn ─────────────────────────────
        if (myPlayer.dead) {
            if (down & PAD_BUTTON_A)
                Player_Respawn(&myPlayer, &myWorld);
            goto render_frame; // freeze world while dead
        }

        // ── Hotbar / gameplay input ───────────────────────────────────────
        // D-pad left/right cycles hotbar
        if (down & PAD_BUTTON_LEFT)
            gui.selectedSlot = (gui.selectedSlot - 1 + HOTBAR_SLOTS) % HOTBAR_SLOTS;
        if (down & PAD_BUTTON_RIGHT)
            gui.selectedSlot = (gui.selectedSlot + 1) % HOTBAR_SLOTS;

        // D-pad down = drop one item; hold Y while pressing down = drop whole stack
        if (down & PAD_BUTTON_DOWN) {
            int slot = gui.selectedSlot;
            if (gui.slotCount[slot] > 0 && gui.slotBlock[slot] != BLOCK_AIR
                && gui.slotBlock[slot] != BLOCK_FLINT_STEEL) { // tools cant be dropped
                u8 block = gui.slotBlock[slot];
                // Hold Y = drop whole stack, otherwise drop one
                u8 held_y = Input_ButtonsHeld() & PAD_BUTTON_Y;
                int dropCount = (held_y && gui.slotCount[slot] > 1)
                    ? gui.slotCount[slot] : 1;
                gui.slotCount[slot] -= dropCount;
                if (gui.slotCount[slot] <= 0) {
                    gui.slotCount[slot] = 0;
                    gui.slotBlock[slot] = BLOCK_AIR;
                }
                // Spawn drop(s) in front of the player
                float dropX = myPlayer.pos.x + myCam.forward.x * 0.8f;
                float dropY = myPlayer.pos.y + 1.0f;
                float dropZ = myPlayer.pos.z + myCam.forward.z * 0.8f;
                for (int d = 0; d < dropCount; d++)
                    ItemDrop_Spawn(block, dropX, dropY, dropZ, 90);
            }
        }

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
        Camera_Apply(&myCam);
        World_Render(&myWorld);

        // Update + render item drops
        ItemDrop_Update(&myWorld, &myPlayer, &gui);
        ItemDrop_Render();
        Particle_Update();
        Particle_Render();

        // Raycast from camera
        RayResult ray = Raycast(&myWorld, myCam.pos, myCam.forward);
        Raycast_DrawHighlight(&ray, breakProgress);

        // ── Input ────────────────────────────────────────────────────────
        u8 l_now = Input_TriggerL();
        u8 r_now = Input_TriggerR();
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
                    // Spawn item drop at the broken block position
                    // Determine what this block drops
                    u8 dropBlock = broken;
                    if (broken == 1) dropBlock = 2;  // grass -> dirt
                    // Leaves drop nothing (need shears)
                    if (broken != 5) {
                        ItemDrop_Spawn(dropBlock,
                            (float)ray.bx, (float)ray.by, (float)ray.bz, 30);
                    }
                    // Spawn break particles
                    Particle_SpawnBlockBreak(ray.bx, ray.by, ray.bz, broken);
                    Sound_Play(SFX_BLOCK_BREAK);
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

        // ── L trigger = place block / use tool (single press) ──────────────
        if (l_now > 100 && l_prev <= 100 && ray.hit) {
            u8 place = gui.slotBlock[gui.selectedSlot];
            if (gui.slotCount[gui.selectedSlot] > 0 && place != BLOCK_AIR) {
                if (place == BLOCK_FLINT_STEEL) {
                    // Use flint & steel: ignite TNT the ray is pointing at
                    if (World_GetBlock(&myWorld, ray.bx, ray.by, ray.bz) == BLOCK_TNT) {
                        TNT_Ignite(&myWorld, ray.bx, ray.by, ray.bz);
                        triggerSwing = 1;
                    }
                } else {
                    // Normal block placement
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

        TNT_Render();
        GUI_DrawScore(rmode, &gui);
        GUI_DrawCrosshair(rmode);
        if (gui.paused) GUI_DrawPauseMenu(rmode, &gui);
        if (gui.inventoryOpen) GUI_DrawInventory(rmode, &gui);
        GUI_DrawHotbar(rmode, &gui);
        GUI_DrawHealth(rmode, &gui);
        GUI_DrawDebug(rmode,
            myPlayer.pos.x, myPlayer.pos.y, myPlayer.pos.z, fps);
        GUI_End2D(rmode);

        // ── Hand — drawn last so it's always on top of GUI ──────────────────
        // ── Present ─────────────────────────────────────────────────────────
        GX_DrawDone();
#ifdef _PC
        GX_CopyDisp(NULL, GX_TRUE);
#else
        GX_CopyDisp(frameBuffer, GX_TRUE);
        GX_Flush();
        VIDEO_SetNextFramebuffer(frameBuffer);
        VIDEO_Flush();
        VIDEO_WaitVSync();
#endif
        // ── Between frames: update world state (safe to call GX here) ───────
        TNT_Update(&myWorld, &myPlayer);
        World_RebuildDirty(&myWorld);
    }
    game_exit:
    Sound_Shutdown();
#ifdef _PC
    Platform_DestroyWindow();
#endif
    return 0;
}