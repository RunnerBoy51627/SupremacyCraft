#ifndef ITEMDROP_H
#define ITEMDROP_H

#include <gccore.h>
#include "world.h"
#include "player.h"
#include "gui.h"

#define MAX_ITEM_DROPS   64
#define DROP_PICKUP_DIST 1.5f   // auto-collect radius (blocks)
#define DROP_ATTRACT_DIST 2.5f  // start flying toward player
#define DROP_LIFETIME    6000   // frames before despawn (~100s at 60fps)
#define DROP_SIZE        0.20f  // rendered cube size

typedef struct {
    int   active;
    u8    blockType;
    float x, y, z;         // world position
    float vy;              // vertical velocity (gravity + bounce)
    float bobPhase;        // 0..2PI, drives idle float animation
    float attractPhase;    // 0 = idle, >0 = flying toward player
    int   lifetime;        // frames remaining
    int   grounded;        // has it landed yet
    int   pickupDelay;     // frames until pickup is allowed (30 = 0.5s)
} ItemDrop;

// Call once at startup
void ItemDrop_Init(void);

// Spawn a drop at world position with a small random spread
void ItemDrop_Spawn(u8 blockType, float x, float y, float z, int pickupDelay);

// Update all active drops — physics, attract, pickup
// Adds to inventory via gui when collected
void ItemDrop_Update(World* world, Player* player, GUIState* gui);

// Render all active drops in 3D world space
void ItemDrop_Render(void);

#endif