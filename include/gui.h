#ifndef GUI_H
#define GUI_H

#include <gccore.h>
#include "utils.h"

#define HOTBAR_SLOTS 8
// Screen dimensions — update to match your video mode
#if DISPLAY_WIDESCREEN
  #define SCREEN_W 854
  #define SCREEN_H 480
#else
  #define SCREEN_W 640
  #define SCREEN_H 480
#endif

// Pause menu
#define PAUSE_ITEM_RESUME  0
#define PAUSE_ITEM_RESPAWN 1
#define PAUSE_ITEM_QUIT    2
#define PAUSE_ITEM_COUNT   3

#define INV_SLOTS 8

typedef struct {
    int selectedSlot;
    int health;
    int maxHealth;
    int paused;
    int pauseCursor;

    // Inventory — one slot per hotbar position
    u8  slotBlock[INV_SLOTS];  // block type in each slot
    int slotCount[INV_SLOTS];  // item count in each slot

    // Score
    int score;
} GUIState;

// Call once after GX_Init to set up GUI state
void GUI_Init(GUIState* gui);

// Switch GX to orthographic 2D projection — call before any GUI drawing
void GUI_Begin2D(GXRModeObj* rmode);

// Restore perspective projection — call after all GUI drawing
void GUI_End2D(GXRModeObj* rmode);

// Individual element draws — call between GUI_Begin2D and GUI_End2D
void GUI_DrawCrosshair(GXRModeObj* rmode);
void GUI_DrawHotbar(GXRModeObj* rmode, GUIState* gui);
void GUI_DrawHealth(GXRModeObj* rmode, GUIState* gui);
void GUI_DrawDebug(GXRModeObj* rmode, float x, float y, float z, int fps);
void GUI_DrawRect(int x, int y, int w, int h, u8 r, u8 g, u8 b, u8 a);
void GUI_DrawString(float x, float y, float scale, const char* str, u8 r, u8 g, u8 b);
void GUI_DrawPauseMenu(GXRModeObj* rmode, GUIState* gui);
void GUI_DrawScore(GXRModeObj* rmode, GUIState* gui);

#endif