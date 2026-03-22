#include "player.h"
#include "chunk.h"
#include "world.h"
#include "camera.h"
#include <math.h>
#include <math.h>
#include <gccore.h>

// ── World block query ────────────────────────────────────────────────────────
static int world_is_solid(World* world, int bx, int by, int bz) {
    if (bx < 0 || bz < 0) return 1; // world edge = solid
    if (bx >= WORLD_W * CHUNK_SIZE || bz >= WORLD_D * CHUNK_SIZE) return 1;
    if (by < 0) return 1;  // below world = solid floor
    if (by >= CHUNK_SIZE) return 0;  // above chunk = air
    return World_GetBlock(world, bx, by, bz) != BLOCK_AIR;
}

// ── AABB sweep helper ────────────────────────────────────────────────────────
// Check if the player AABB at (px,py,pz) overlaps any solid block.
// We test all block positions the AABB could occupy.
static int player_overlaps_solid(World* world, float px, float py, float pz) {
    float half = PLAYER_WIDTH * 0.5f;

    int x0 = (int)floorf(px - half);
    int x1 = (int)floorf(px + half - 0.001f);
    int y0 = (int)floorf(py);
    int y1 = (int)floorf(py + PLAYER_HEIGHT - 0.001f);
    int z0 = (int)floorf(pz - half);
    int z1 = (int)floorf(pz + half - 0.001f);

    for (int x = x0; x <= x1; x++)
    for (int y = y0; y <= y1; y++)
    for (int z = z0; z <= z1; z++)
        if (world_is_solid(world, x, y, z)) return 1;

    return 0;
}

// ── Init ─────────────────────────────────────────────────────────────────────
void Player_Init(Player* player) {
    player->spawn        = (guVector){48.0f, 60.0f, 48.0f};
    player->pos          = player->spawn;
    player->velocity     = (guVector){0.0f, 0.0f, 0.0f};
    player->grounded     = 0;
    player->health       = PLAYER_MAX_HEALTH;
    player->air          = PLAYER_MAX_AIR;
    player->invincible   = 0;
    player->fall_speed_peak = 0.0f;
    player->dead         = 0;
    player->death_timer  = 0;
}

void Player_Damage(Player* player, int amount) {
    if (player->invincible > 0 || player->dead) return;
    player->health -= amount;
    if (player->health < 0) player->health = 0;
    player->invincible = 30; // ~0.5s at 60fps
}

int Player_IsUnderwater(Player* player, World* world) {
    (void)world; // placeholder until water is added
    return 0;
}

// ── Update ───────────────────────────────────────────────────────────────────
void Player_Update(Player* player, World* world,
                   s8 stickX, s8 stickY, int jumpPressed,
                   float yaw)
{
    // ── Death & respawn ───────────────────────────────────────────────────
    if (player->dead) {
        if (--player->death_timer <= 0) {
            player->pos      = player->spawn;
            player->velocity = (guVector){0,0,0};
            player->health   = PLAYER_MAX_HEALTH;
            player->air      = PLAYER_MAX_AIR;
            player->grounded = 0;
            player->dead     = 0;
            player->fall_speed_peak = 0.0f;
        }
        return;
    }
    if (player->health <= 0) {
        player->dead        = 1;
        player->death_timer = 180;
        return;
    }
    if (player->invincible > 0) player->invincible--;

    // 1. Compute movement direction from analog stick + camera yaw
    float moveX = 0.0f, moveZ = 0.0f;
    if (stickY != 0 || stickX != 0) {
        float yawRad = yaw * (3.14159265f / 180.0f);
        // Match camera.cpp: forward = (cosYaw, _, sinYaw)
        float fwdX   =  cosf(yawRad);
        float fwdZ   =  sinf(yawRad);
        float rightX =  sinf(yawRad);
        float rightZ = -cosf(yawRad);

        float sy = stickY / 128.0f;
        float sx = -(stickX / 128.0f); // GC stick X is inverted

        // Dead zone
        if (sy > -0.12f && sy < 0.12f) sy = 0.0f;
        if (sx > -0.12f && sx < 0.12f) sx = 0.0f;

        moveX = (fwdX * sy + rightX * sx) * MOVE_SPEED;
        moveZ = (fwdZ * sy + rightZ * sx) * MOVE_SPEED;
    }

    // 2. Jumping
    if (jumpPressed && player->grounded) {
        player->velocity.y = JUMP_FORCE;
        player->grounded   = 0;
    }

    // 3. Gravity
    player->velocity.y += GRAVITY;
    if (player->velocity.y < -1.0f) player->velocity.y = -1.0f;
    if (player->velocity.y < player->fall_speed_peak)
        player->fall_speed_peak = player->velocity.y;

    // 4. Collision resolution — each axis separately so we slide along walls

    // X axis
    float newX = player->pos.x + moveX;
    if (!player_overlaps_solid(world, newX, player->pos.y, player->pos.z))
        player->pos.x = newX;

    // Z axis
    float newZ = player->pos.z + moveZ;
    if (!player_overlaps_solid(world, player->pos.x, player->pos.y, newZ))
        player->pos.z = newZ;

    // Y axis (gravity + jumping)
    float newY = player->pos.y + player->velocity.y;
    if (!player_overlaps_solid(world, player->pos.x, newY, player->pos.z)) {
        player->pos.y  = newY;
        player->grounded = 0;
    } else {
        if (player->velocity.y < 0) {
            // Fall damage
            float speed = -player->fall_speed_peak;
            if (speed > FALL_DAMAGE_THRESHOLD) {
                int dmg = (int)((speed - FALL_DAMAGE_THRESHOLD) * FALL_DAMAGE_MULT);
                if (dmg > 0) Player_Damage(player, dmg);
            }
            player->fall_speed_peak = 0.0f;
            player->pos.y    = ceilf(newY);
            player->grounded = 1;
        } else {
            // Bumped ceiling — push down below the block above
            player->pos.y = floorf(player->pos.y + PLAYER_HEIGHT) - PLAYER_HEIGHT - 0.001f;
        }
        player->velocity.y = 0.0f;
    }

    // 5. World boundary clamp (don't fall off the edge of loaded chunks)
    float worldMaxX = (float)(3 * CHUNK_SIZE);
    float worldMaxZ = (float)(3 * CHUNK_SIZE);
    if (player->pos.x < 0.5f)         player->pos.x = 0.5f;
    if (player->pos.x > worldMaxX - 0.5f) player->pos.x = worldMaxX - 0.5f;
    if (player->pos.z < 0.5f)         player->pos.z = 0.5f;
    if (player->pos.z > worldMaxZ - 0.5f) player->pos.z = worldMaxZ - 0.5f;
}

// ── Camera sync ──────────────────────────────────────────────────────────────
void Player_ApplyToCamera(Player* player, FreeCam* cam) {
    // Eyes are PLAYER_EYE_Y above feet
    cam->pos.x = player->pos.x;
    cam->pos.y = player->pos.y + PLAYER_EYE_Y;
    cam->pos.z = player->pos.z;
}