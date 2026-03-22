#ifndef ANIM_H
#define ANIM_H

#include <gccore.h>
#include "camera.h"

// ── Animation states ──────────────────────────────────────────────────────────
typedef enum {
    ANIM_IDLE  = 0,   // gentle idle bob
    ANIM_SWING = 1,   // attack / place swing
    ANIM_EQUIP = 2,   // slot-switch raise
} AnimState;

// ── Per-bone transform (local space) ─────────────────────────────────────────
typedef struct {
    guVector pos;     // translation
    guVector rot;     // euler angles in degrees (pitch=X, yaw=Y, roll=Z)
    guVector scale;   // scale (1,1,1 = normal)
} BoneTransform;

// ── Hand animator state ───────────────────────────────────────────────────────
typedef struct {
    AnimState state;
    float     timer;      // frames elapsed in current state
    float     blendOut;   // 0..1, used to lerp back to idle after swing

    // Solved transforms for this frame (written by Anim_Update, read by Anim_Draw)
    BoneTransform arm;    // upper arm / sleeve bone
    BoneTransform hand;   // hand / fist bone (child of arm)
    // Bob/sway — fed from player each frame via Anim_SetBob
    float bobPhase;        // radians, same source as player view bob
    float bobIntensity;    // 0..1

    // Camera look lag — hand trails behind when you turn
    float lookDX;          // accumulated yaw   delta (smoothed)
    float lookDY;          // accumulated pitch delta (smoothed)

    // Currently held block (BLOCK_AIR = show arm)
    u8    heldBlock;
} HandAnim;

// Call once to zero-initialise
void Anim_Init(HandAnim* anim);

// Feed view-bob state from the player (call before Anim_Update each frame)
void Anim_SetBob(HandAnim* anim, float phase, float intensity);

// Feed camera turn delta so the hand lags behind when you look around
void Anim_SetLook(HandAnim* anim, float deltaYaw, float deltaPitch);

// Set the currently held block (BLOCK_AIR = show bare arm)
void Anim_SetHeldBlock(HandAnim* anim, u8 block);

// Call every gameplay frame (not while paused/dead).
// triggerSwing=1 starts a swing, triggerEquip=1 starts the equip raise.
void Anim_Update(HandAnim* anim, int triggerSwing, int triggerEquip);

// Render the hand in 3D (call while perspective projection is active,
// BEFORE GUI_Begin2D).  cam is used to build the camera-local matrix.
void Anim_DrawHand(HandAnim* anim, FreeCam* cam);

#endif