#ifndef PLAYER_H
#define PLAYER_H

#include <gccore.h>
#include "world.h"
#include "camera.h"

// Player AABB size (in blocks)
#define PLAYER_WIDTH  0.6f
#define PLAYER_HEIGHT 1.8f
#define PLAYER_EYE_Y  1.6f  // camera height above player feet

#define GRAVITY       -0.010f
#define JUMP_FORCE     0.18f
#define MOVE_SPEED     0.10f

#define PLAYER_MAX_HEALTH    20
#define PLAYER_MAX_AIR       300   // frames of air underwater (~5 seconds at 60fps)
// Fall damage kicks in at ~3 blocks. At terminal velocity (~2.0) a 25-block fall is lethal.
#define FALL_DAMAGE_THRESHOLD 0.241f
#define FALL_DAMAGE_MULT      37.81f

typedef struct {
    guVector pos;
    guVector velocity;
    int      grounded;

    // Survival stats
    int      health;          // 0-20
    int      air;             // frames of air remaining (300 = full)
    int      invincible;      // invincibility frames after taking damage
    float    fall_speed_peak; // track peak downward velocity for fall damage

    // Respawn
    guVector spawn;           // respawn position
    int      dead;            // 1 if player is dead, waits for A press

    // View bobbing
    float    bobPhase;        // walks 0..2PI per step cycle
    float    bobIntensity;    // 0 = still, 1 = full bob (lerps up/down)
} Player;

void Player_Init(Player* player);

// Apply gravity, movement, jumping, collision, survival — call every frame
void Player_Update(Player* player, World* world,
                   s8 stickX, s8 stickY, int jumpPressed,
                   float yaw);

// Deal damage to player (respects invincibility frames)
void Player_Damage(Player* player, int amount);

// Respawn player — finds safe terrain height at spawn x/z, resets all stats
void Player_Respawn(Player* player, World* world);

// Point the FreeCam at the player's eye position
void Player_ApplyToCamera(Player* player, FreeCam* cam);

// Returns 1 if player is currently underwater
int Player_IsUnderwater(Player* player, World* world);

#endif