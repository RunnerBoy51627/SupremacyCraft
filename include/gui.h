#ifndef GUI_H
#define GUI_H

#include <gccore.h>

#define HOTBAR_SLOTS 8
#define SCREEN_W 640
#define SCREEN_H 480

typedef struct {
    int selectedSlot;  // 0-7, controlled by D-pad
    int health;        // 0-20
    int maxHealth;     // 20
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

#endif