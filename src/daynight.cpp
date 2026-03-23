#include "daynight.h"
#include "utils.h"
#include "platform_types.h"
#ifndef _PC
#include <gccore.h>
#include <math.h>
#else
#include "platform/pc/gc_compat.h"
#include <math.h>
#endif

DayNight g_daynight;

// ── Sky color keyframes ───────────────────────────────────────────────────────
// Each entry: { time, r, g, b }
struct SkyKey { int time; u8 r, g, b; };

static const SkyKey SKY_KEYS[] = {
    {     0,  10,  10,  30 }, // midnight — deep dark blue
    {  3000,   8,   8,  25 }, // late night
    {  4000,  30,  15,  40 }, // pre-dawn purple
    {  4500,  90,  40,  20 }, // sunrise orange
    {  5500, 135, 100, 180 }, // early morning lavender
    {  7200, 135, 206, 235 }, // full day — Minecraft sky blue
    { 18000, 135, 206, 235 }, // noon
    { 49500, 135, 206, 235 }, // late afternoon
    { 54000, 255, 140,  50 }, // sunset orange
    { 56000, 120,  60,  80 }, // dusk purple
    { 57600,  20,  20,  50 }, // early night
    { 60000,  10,  10,  30 }, // night
    { 72000,  10,  10,  30 }, // wrap
};
static const int SKY_KEY_COUNT = (int)(sizeof(SKY_KEYS) / sizeof(SKY_KEYS[0]));

// ── Ambient light keyframes ───────────────────────────────────────────────────
struct LightKey { int time; u8 level; }; // 0=dark, 255=full bright

static const LightKey LIGHT_KEYS[] = {
    {     0,  30 }, // midnight
    {  3600,  20 }, // deep night
    {  4500,  60 }, // sunrise
    {  6000, 180 }, // morning
    {  7200, 255 }, // full day
    { 49500, 255 }, // afternoon
    { 54000, 200 }, // sunset
    { 56000,  80 }, // dusk
    { 57600,  35 }, // night begins
    { 60000,  30 }, // night
    { 72000,  30 }, // wrap
};
static const int LIGHT_KEY_COUNT = (int)(sizeof(LIGHT_KEYS) / sizeof(LIGHT_KEYS[0]));

// ── Helpers ───────────────────────────────────────────────────────────────────

static u8    lerp_u8(u8 a, u8 b, float t) {
    int v = (int)(a + (b - a) * t);
    return (u8)(v < 0 ? 0 : v > 255 ? 255 : v);
}

static SkyColor sky_at(int time) {
    for (int i = 0; i < SKY_KEY_COUNT - 1; i++) {
        if (time >= SKY_KEYS[i].time && time < SKY_KEYS[i+1].time) {
            float t = (float)(time - SKY_KEYS[i].time) /
                      (float)(SKY_KEYS[i+1].time - SKY_KEYS[i].time);
            SkyColor c;
            c.r = lerp_u8(SKY_KEYS[i].r, SKY_KEYS[i+1].r, t);
            c.g = lerp_u8(SKY_KEYS[i].g, SKY_KEYS[i+1].g, t);
            c.b = lerp_u8(SKY_KEYS[i].b, SKY_KEYS[i+1].b, t);
            return c;
        }
    }
    SkyColor c = { SKY_KEYS[0].r, SKY_KEYS[0].g, SKY_KEYS[0].b };
    return c;
}

static u8 light_at(int time) {
    for (int i = 0; i < LIGHT_KEY_COUNT - 1; i++) {
        if (time >= LIGHT_KEYS[i].time && time < LIGHT_KEYS[i+1].time) {
            float t = (float)(time - LIGHT_KEYS[i].time) /
                      (float)(LIGHT_KEYS[i+1].time - LIGHT_KEYS[i].time);
            return lerp_u8(LIGHT_KEYS[i].level, LIGHT_KEYS[i+1].level, t);
        }
    }
    return LIGHT_KEYS[0].level;
}

// ── Public API ────────────────────────────────────────────────────────────────

void DayNight_Init() {
    g_daynight.time = 4500; // start at sunrise
    DayNight_Update();
}

void DayNight_Update() {
    g_daynight.time++;
    if (g_daynight.time >= DAY_LENGTH)
        g_daynight.time = 0;

    u8 lv = light_at(g_daynight.time);
    g_daynight.light = lv / 255.0f;

    g_daynight.sky     = sky_at(g_daynight.time);
    g_daynight.ambient = { lv, lv, lv };
}

float DayNight_GetLight()      { return g_daynight.light; }
SkyColor DayNight_GetSky()     { return g_daynight.sky; }
SkyColor DayNight_GetAmbient() { return g_daynight.ambient; }

void DayNight_ApplyGX() {
    SkyColor s = g_daynight.sky;

    // Update clear color (sky)
    GXColor bg = { s.r, s.g, s.b, 0xff };
    GX_SetCopyClear(bg, 0x00ffffff);

    // Update fog color to match sky
    GXColor fogCol = { s.r, s.g, s.b, 0xff };
    GX_SetFog(GX_FOG_LIN,
              g_config.fogStart, g_config.fogEnd, 0.1f, 1000.0f,
              fogCol);
}