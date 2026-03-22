#ifndef PARTICLE_H
#define PARTICLE_H

#include <gccore.h>

#define MAX_PARTICLES    128  // pool size
#define PARTICLE_LIFETIME 22  // frames

typedef struct {
    int   active;
    float x, y, z;
    float vx, vy, vz;   // velocity
    float size;          // starts at max, shrinks to 0
    float sizeStart;
    int   life;          // frames remaining
    u8    r, g, b;       // color
} Particle;

void Particle_Init(void);

// Burst of particles from a broken block at world pos (bx,by,bz)
// blockType matches chunk.h: 1=grass,2=dirt,3=stone,4=wood,5=leaf
void Particle_SpawnBlockBreak(int bx, int by, int bz, u8 blockType);

void Particle_Update(void);
void Particle_Render(void);

#endif