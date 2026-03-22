#ifndef UTILS_H
#define UTILS_H

#include <gccore.h>

// ── Display settings ──────────────────────────────────────────────────────────
#define DISPLAY_WIDESCREEN  0   // 1 = 16:9, 0 = 4:3

// ── Render settings ───────────────────────────────────────────────────────────
#define RENDER_FOV          70.0f   // field of view in degrees
#define RENDER_NEAR         0.05f   // near clip plane
#define RENDER_FAR          300.0f  // far clip plane

// ── Fog settings ─────────────────────────────────────────────────────────────
#define FOG_START           20.0f   // distance fog begins
#define FOG_END             80.0f   // distance fog is fully opaque
#define FOG_R               0x87    // sky/fog color R
#define FOG_G               0xCE    // sky/fog color G
#define FOG_B               0xEB    // sky/fog color B

// ── Gameplay settings ─────────────────────────────────────────────────────────
#define SETTING_REACH       5.0f    // block interact distance
#define SETTING_MOVE_SPEED  0.10f   // player walk speed
#define SETTING_SENSITIVITY 2.5f    // camera look sensitivity

// ── Config struct — loaded once at startup ────────────────────────────────────
typedef struct {
    int   widescreen;     // 1 = 16:9
    float fov;
    float fogStart;
    float fogEnd;
    float reach;
    float moveSpeed;
    float sensitivity;
    GXColor skyColor;     // also used as fog color
} GameConfig;

// Global config instance — include utils.h and use g_config anywhere
extern GameConfig g_config;

// Initialize config with defaults (call once at startup before anything else)
void Config_Init();

// Apply display/projection settings to GX (call after GX_Init and after any config change)
void Config_ApplyDisplay(GXRModeObj* rmode);

// Apply fog settings to GX
void Config_ApplyFog();

// Compute correct aspect ratio accounting for widescreen
float Config_AspectRatio(GXRModeObj* rmode);

#endif