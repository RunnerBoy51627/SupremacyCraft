#ifndef RAYCAST_H
#define RAYCAST_H

#include <gccore.h>
#include "world.h"

#define RAY_MAX_DIST 5.0f  // max block reach in blocks
#define RAY_STEP     0.05f // step size — smaller = more accurate

typedef struct {
    int hit;          // 1 if a block was found
    int bx, by, bz;   // hit block coords
    int px, py, pz;   // place position (last air block before hit)
} RayResult;

// Cast a ray from origin in direction, return hit info
RayResult Raycast(World* world, guVector origin, guVector dir);

// Draw a wireframe box around the targeted block
void Raycast_DrawHighlight(RayResult* ray);

#endif