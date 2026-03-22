#ifndef GUI_H
#define GUI_H

#include <gccore.h>
#include "utils.h"

#define HOTBAR_SLOTS 9
#define INV_ROWS     3
#define INV_COLS     9
#define INV_MAIN_SLOTS (INV_ROWS * INV_COLS)        // 27 main inventory slots
#define INV_SLOTS    (HOTBAR_SLOTS + INV_MAIN_SLOTS) // 36 total

// Screen dimensions
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

typedef struct {
    int selectedSlot;       // hotbar selected slot (0-8)
    int health;
    int maxHealth;
    int paused;
    int pauseCursor;

    // Full inventory: slots 0-8 = hotbar, slots 9-35 = main inventory
    u8  slotBlock[INV_SLOTS];
    int slotCount[INV_SLOTS];

    // Inventory screen state
    int inventoryOpen;      // 1 = inventory screen visible
    int invCursorX;         // cursor column 0-8
    int invCursorY;         // cursor row: 0-2 = main inv, 3 = hotbar
    u8  heldItemBlock;      // item currently riding the cursor
    int heldItemCount;      // count of held item (0 = nothing held)

    // Crafting grid (2x2 personal, always open in inventory)
    u8  craftGrid[4];     // 2x2 grid blocks
    int craftCount[4];    // 2x2 grid counts
    u8  craftResult;      // current recipe output block
    int craftResultCount; // current recipe output count
    int craftCursorOn;    // 1 = cursor is on craft grid/output
    int craftCursorIdx;   // 0-3 = grid slot, 4 = output slot

    // Score
    int score;
} GUIState;

void GUI_Init(GUIState* gui);
void GUI_Begin2D(GXRModeObj* rmode);
void GUI_End2D(GXRModeObj* rmode);
void GUI_DrawCrosshair(GXRModeObj* rmode);
void GUI_DrawHotbar(GXRModeObj* rmode, GUIState* gui);
void GUI_DrawHealth(GXRModeObj* rmode, GUIState* gui);
void GUI_DrawDebug(GXRModeObj* rmode, float x, float y, float z, int fps);
void GUI_DrawRect(int x, int y, int w, int h, u8 r, u8 g, u8 b, u8 a);
void GUI_DrawString(float x, float y, float scale, const char* str, u8 r, u8 g, u8 b);
void GUI_DrawPauseMenu(GXRModeObj* rmode, GUIState* gui);
void GUI_DrawScore(GXRModeObj* rmode, GUIState* gui);
void GUI_DrawInventory(GXRModeObj* rmode, GUIState* gui);

#endif