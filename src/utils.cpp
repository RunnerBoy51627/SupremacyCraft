#include "utils.h"
#include <gccore.h>

// Global config instance
GameConfig g_config;

void Config_Init() {
    g_config.widescreen  = DISPLAY_WIDESCREEN;
    g_config.fov         = RENDER_FOV;
    g_config.fogStart    = FOG_START;
    g_config.fogEnd      = FOG_END;
    g_config.reach       = SETTING_REACH;
    g_config.moveSpeed   = SETTING_MOVE_SPEED;
    g_config.sensitivity = SETTING_SENSITIVITY;
    g_config.skyColor    = {FOG_R, FOG_G, FOG_B, 0xFF};
}

float Config_AspectRatio(GXRModeObj* rmode) {
    if (g_config.widescreen) {
        return 16.0f / 9.0f;
    }
    return (f32)rmode->fbWidth / (f32)rmode->efbHeight;
}

void Config_ApplyDisplay(GXRModeObj* rmode) {
    // Widescreen is handled purely through the projection aspect ratio.
    // Config_AspectRatio() returns 16/9 or the native ratio based on the setting.
    Mtx44 projection;
    guPerspective(projection,
                  g_config.fov,
                  Config_AspectRatio(rmode),
                  RENDER_NEAR,
                  RENDER_FAR);
    GX_LoadProjectionMtx(projection, GX_PERSPECTIVE);
}

void Config_ApplyFog() {
    GX_SetFog(GX_FOG_LIN,
              g_config.fogStart,
              g_config.fogEnd,
              RENDER_NEAR,
              RENDER_FAR,
              g_config.skyColor);
}