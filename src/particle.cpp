#include "particle.h"
#include "camera.h"
#include <math.h>
#include "platform_types.h"

static Particle s_particles[MAX_PARTICLES];

// ── LCG random ────────────────────────────────────────────────────────────────
static unsigned int s_pseed = 98765;
static float pfrand() {
    s_pseed = s_pseed * 1664525u + 1013904223u;
    return (float)(s_pseed & 0xFFFF) / 65535.0f; // 0..1
}
static float pfrand_s() { return pfrand() * 2.0f - 1.0f; } // -1..1

// ── Block particle colors (use top-face color for variety) ────────────────────
static void get_particle_color(u8 blockType, u8* r, u8* g, u8* b) {
    switch (blockType) {
        case 1: *r=106; *g=178; *b=80;  break; // grass — green
        case 2: *r=134; *g=96;  *b=67;  break; // dirt  — brown
        case 3: *r=128; *g=128; *b=128; break; // stone — grey
        case 4: *r=160; *g=130; *b=80;  break; // wood  — tan
        case 5: *r=50;  *g=120; *b=35;  break; // leaf  — dark green
        case 9: *r=200; *g=30;  *b=30;  break; // TNT
        default:*r=180; *g=180; *b=180; break;
    }
}

// ── Init ──────────────────────────────────────────────────────────────────────
void Particle_Init(void) {
    for (int i = 0; i < MAX_PARTICLES; i++)
        s_particles[i].active = 0;
}

// ── Spawn block break burst ───────────────────────────────────────────────────
void Particle_SpawnBlockBreak(int bx, int by, int bz, u8 blockType) {
    u8 br, bg, bb;
    get_particle_color(blockType, &br, &bg, &bb);

    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < 8; i++) {
        if (s_particles[i].active) continue;
        Particle* p = &s_particles[i];

        // Spawn inside the block volume with small spread
        p->x = bx + 0.2f + pfrand() * 0.6f;
        p->y = by + 0.2f + pfrand() * 0.6f;
        p->z = bz + 0.2f + pfrand() * 0.6f;

        // Random outward velocity
        float speed = 0.04f + pfrand() * 0.06f;
        p->vx = pfrand_s() * speed;
        p->vy = 0.04f + pfrand() * 0.08f; // always pops upward a bit
        p->vz = pfrand_s() * speed;

        // Size: small chunks, 0.04–0.10 blocks
        p->sizeStart = 0.04f + pfrand() * 0.06f;
        p->size      = p->sizeStart;

        p->life   = PARTICLE_LIFETIME;
        p->active = 1;

        // Slight color variation per particle
        int var = (int)(pfrand() * 30) - 15;
        p->r = (u8)((int)br + var < 0 ? 0 : (int)br + var > 255 ? 255 : (int)br + var);
        p->g = (u8)((int)bg + var < 0 ? 0 : (int)bg + var > 255 ? 255 : (int)bg + var);
        p->b = (u8)((int)bb + var < 0 ? 0 : (int)bb + var > 255 ? 255 : (int)bb + var);

        spawned++;
    }
}

// ── Update ────────────────────────────────────────────────────────────────────
void Particle_Update(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &s_particles[i];
        if (!p->active) continue;

        p->life--;
        if (p->life <= 0) { p->active = 0; continue; }

        // Gravity
        p->vy -= 0.006f;

        p->x += p->vx;
        p->y += p->vy;
        p->z += p->vz;

        // Drag (slow down over time)
        p->vx *= 0.92f;
        p->vz *= 0.92f;

        // Shrink to zero as life runs out
        float t = (float)p->life / (float)PARTICLE_LIFETIME;
        p->size = p->sizeStart * t;
    }
}

// ── Render ────────────────────────────────────────────────────────────────────
// Particles are billboard quads facing the camera — simpler and cheaper than cubes
void Particle_Render(void) {
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_NONE);

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &s_particles[i];
        if (!p->active) continue;

        float h = p->size * 0.5f;

        // Billboard: build right/up vectors from view matrix
        // g_viewMatrix rows 0 and 1 give world-space right and up
        float rx = g_viewMatrix[0][0];
        float ry = g_viewMatrix[0][1];
        float rz = g_viewMatrix[0][2];
        float ux = g_viewMatrix[1][0];
        float uy = g_viewMatrix[1][1];
        float uz = g_viewMatrix[1][2];

        // Four corners of the billboard quad
        float ax = p->x + (-rx - ux)*h,  ay = p->y + (-ry - uy)*h,  az = p->z + (-rz - uz)*h;
        float bx = p->x + ( rx - ux)*h,  by = p->y + ( ry - uy)*h,  bz = p->z + ( rz - uz)*h;
        float cx = p->x + ( rx + ux)*h,  cy = p->y + ( ry + uy)*h,  cz = p->z + ( rz + uz)*h;
        float dx = p->x + (-rx + ux)*h,  dy = p->y + (-ry + uy)*h,  dz = p->z + (-rz + uz)*h;

        // Fade alpha with lifetime
        u8 alpha = (u8)(255.0f * (float)p->life / (float)PARTICLE_LIFETIME);

        Mtx mv;
        guMtxCopy(g_viewMatrix, mv);
        GX_LoadPosMtxImm(mv, GX_PNMTX0);

        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
            GX_Position3f32(ax, ay, az); GX_Color4u8(p->r, p->g, p->b, alpha); GX_TexCoord2f32(0,0);
            GX_Position3f32(bx, by, bz); GX_Color4u8(p->r, p->g, p->b, alpha); GX_TexCoord2f32(0,0);
            GX_Position3f32(cx, cy, cz); GX_Color4u8(p->r, p->g, p->b, alpha); GX_TexCoord2f32(0,0);
            GX_Position3f32(dx, dy, dz); GX_Color4u8(p->r, p->g, p->b, alpha); GX_TexCoord2f32(0,0);
        GX_End();
    }

    // Restore state
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_BACK);
}