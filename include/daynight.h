#pragma once
#include "platform_types.h"

// Day length in ticks at 60fps — 20 min real time like Minecraft
#define DAY_LENGTH      72000
#define DAY_HALF        (DAY_LENGTH / 2)

// Time of day constants
#define TIME_SUNRISE    4500    // 6:00 AM
#define TIME_NOON       18000   // 12:00 PM
#define TIME_SUNSET     49500   // 7:30 PM
#define TIME_MIDNIGHT   0       // 0:00 / 12:00 AM

struct SkyColor {
    u8 r, g, b;
};

struct DayNight {
    int   time;       // 0..DAY_LENGTH-1
    float light;      // 0.0 (night) .. 1.0 (day)
    SkyColor sky;     // current sky/fog color
    SkyColor ambient; // vertex color multiplier for world geometry
};

extern DayNight g_daynight;

void DayNight_Init();
void DayNight_Update();         // call once per frame
float DayNight_GetLight();      // 0..1
SkyColor DayNight_GetSky();
SkyColor DayNight_GetAmbient();
void DayNight_ApplyGX();        // updates GX fog + clear color