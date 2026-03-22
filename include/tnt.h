#pragma once
// ── tnt.h ─────────────────────────────────────────────────────────────────────
// TNT ignition, countdown, and explosion system.
// ─────────────────────────────────────────────────────────────────────────────

#include "platform_types.h"

#include "world.h"
#include "player.h"
#include "particle.h"

#define MAX_TNT       16    // max simultaneous lit TNT
#define TNT_FUSE      90    // frames until explosion (~1.5s at 60fps)
#define TNT_RADIUS    4     // blast radius in blocks

typedef struct {
    int   active;
    int   wx, wy, wz;   // world block position
    int   fuse;         // countdown frames
    float flash;        // visual flash phase
} LitTNT;

// Init the TNT system
void TNT_Init(void);

// Ignite a TNT block at world position — removes the block, starts fuse
// Returns 1 if ignited, 0 if position isn't TNT or pool is full
int  TNT_Ignite(World* world, int wx, int wy, int wz);
int  TNT_IgniteDelayed(World* world, int wx, int wy, int wz, int extra_fuse);

// Update all lit TNT each frame
void TNT_Update(World* world, Player* player);

// Render lit TNT (flashing blocks)
void TNT_Render(void);

// Get lit TNT array for rendering
LitTNT* TNT_GetArray(void);
int     TNT_GetCount(void);