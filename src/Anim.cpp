#include "anim.h"
#include "utils.h"
#include <math.h>
#include <gccore.h>

#define PI      3.14159265f
#define DEG2RAD (PI / 180.0f)

static inline float lerpf(float a, float b, float t) { return a+(b-a)*t; }
static inline float clampf(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }
static inline float smoothstep(float t){ t=clampf(t,0,1); return t*t*(3-2*t); }

// ── Box draw ──────────────────────────────────────────────────────────────────
static void draw_box(float w, float h, float d, u8 r, u8 g, u8 b)
{
    float hw = w * 0.5f;
    float hh = h * 0.5f;
    float hd = d * 0.5f;

    u8 rt = r,              gt_ = g,              bt_ = b;
    u8 rf = (u8)(r*.80f),   gf  = (u8)(g*.80f),   bf  = (u8)(b*.80f);
    u8 rs = (u8)(r*.62f),   gs  = (u8)(g*.62f),   bs_ = (u8)(b*.62f);
    u8 rb = (u8)(r*.44f),   gb  = (u8)(g*.44f),   bb  = (u8)(b*.44f);

    GX_Begin(GX_QUADS, GX_VTXFMT0, 24);
    // Top
    GX_Position3f32(-hw, hh,-hd); GX_Color4u8(rt,gt_,bt_,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw, hh,-hd); GX_Color4u8(rt,gt_,bt_,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw, hh, hd); GX_Color4u8(rt,gt_,bt_,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw, hh, hd); GX_Color4u8(rt,gt_,bt_,255); GX_TexCoord2f32(0,0);
    // Bottom
    GX_Position3f32(-hw,-hh, hd); GX_Color4u8(rb,gb,bb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw,-hh, hd); GX_Color4u8(rb,gb,bb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw,-hh,-hd); GX_Color4u8(rb,gb,bb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw,-hh,-hd); GX_Color4u8(rb,gb,bb,255); GX_TexCoord2f32(0,0);
    // Front
    GX_Position3f32(-hw,-hh, hd); GX_Color4u8(rf,gf,bf,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw,-hh, hd); GX_Color4u8(rf,gf,bf,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw, hh, hd); GX_Color4u8(rf,gf,bf,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw, hh, hd); GX_Color4u8(rf,gf,bf,255); GX_TexCoord2f32(0,0);
    // Back
    GX_Position3f32( hw,-hh,-hd); GX_Color4u8(rf,gf,bf,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw,-hh,-hd); GX_Color4u8(rf,gf,bf,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw, hh,-hd); GX_Color4u8(rf,gf,bf,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw, hh,-hd); GX_Color4u8(rf,gf,bf,255); GX_TexCoord2f32(0,0);
    // Right
    GX_Position3f32( hw,-hh, hd); GX_Color4u8(rs,gs,bs_,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw,-hh,-hd); GX_Color4u8(rs,gs,bs_,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw, hh,-hd); GX_Color4u8(rs,gs,bs_,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw, hh, hd); GX_Color4u8(rs,gs,bs_,255); GX_TexCoord2f32(0,0);
    // Left
    GX_Position3f32(-hw,-hh,-hd); GX_Color4u8(rs,gs,bs_,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw,-hh, hd); GX_Color4u8(rs,gs,bs_,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw, hh, hd); GX_Color4u8(rs,gs,bs_,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw, hh,-hd); GX_Color4u8(rs,gs,bs_,255); GX_TexCoord2f32(0,0);
    GX_End();
}

// ── Init ──────────────────────────────────────────────────────────────────────
void Anim_Init(HandAnim* anim) {
    anim->state    = ANIM_IDLE;
    anim->timer    = 0.0f;
    anim->blendOut = 0.0f;
    anim->arm.pos   = (guVector){0, 0, 0};
    anim->arm.rot   = (guVector){0, 0, 0};
    anim->arm.scale = (guVector){1, 1, 1};
    anim->hand.pos   = (guVector){0, 0, 0};
    anim->hand.rot   = (guVector){0, 0, 0};
    anim->hand.scale = (guVector){1, 1, 1};
}

// ── Update ────────────────────────────────────────────────────────────────────
void Anim_Update(HandAnim* anim, int triggerSwing, int triggerEquip) {
    if (triggerSwing) {
        anim->state = ANIM_SWING;
        anim->timer = 0.0f;
    } else if (triggerEquip && anim->state == ANIM_IDLE) {
        anim->state = ANIM_EQUIP;
        anim->timer = 0.0f;
    }
    anim->timer += 1.0f;

    float idleT  = anim->timer * 0.03f;
    float idleBY = sinf(idleT) * 0.003f;
    float idleRZ = sinf(idleT * 0.5f) * 0.5f;

    float swDY = 0, swRX = 0;
    if (anim->state == ANIM_SWING) {
        float phase = clampf(anim->timer / 14.0f, 0.0f, 1.0f);
        if (phase < 0.4f) {
            float t = smoothstep(phase / 0.4f);
            swDY = lerpf(0.0f, -0.07f, t);
            swRX = lerpf(0.0f,  60.0f, t);
        } else {
            float t = smoothstep((phase - 0.4f) / 0.6f);
            swDY = lerpf(-0.07f, 0.0f, t);
            swRX = lerpf( 60.0f, 0.0f, t);
        }
        if (phase >= 1.0f) anim->state = ANIM_IDLE;
    }

    float eqDY = 0, eqRX = 0;
    if (anim->state == ANIM_EQUIP) {
        float t = smoothstep(clampf(anim->timer / 8.0f, 0, 1));
        eqDY = lerpf(-0.10f, 0.0f, t);
        eqRX = lerpf( 20.0f, 0.0f, t);
        if (anim->timer >= 8.0f) anim->state = ANIM_IDLE;
    }

    anim->arm.pos.y = idleBY + swDY + eqDY;
    anim->arm.rot.x = swRX + eqRX;
    anim->arm.rot.z = idleRZ;
}

// ── Draw ──────────────────────────────────────────────────────────────────────
void Anim_DrawHand(HandAnim* anim, FreeCam* cam) {
    (void)cam;

    GX_SetFog(GX_FOG_NONE, 0, 0, 0, 0, (GXColor){0,0,0,0});
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_BACK);

    // ── Position (eye-space: X+ right, Y+ up, Z- into screen) ────────────
    // ez=-0.72: frustum half-extents at this depth ±0.672 X / ±0.504 Y
    float ex =  0.44f + anim->arm.pos.x;  // right of centre
    float ey = -0.30f + anim->arm.pos.y;  // below centre
    float ez = -0.82f;                     // further away = smaller

    // ── Rotation ──────────────────────────────────────────────────────────
    float rotY = -45.0f;                     // left or right  — front face toward viewer
    float rotX = -15.0f + anim->arm.rot.x;  // tilt forward and back — shows top face
    float rotZ =           anim->arm.rot.z;  // roll — idle sway only

    Mtx ry, rx, rz, tmp, rot;
    guMtxRotRad(ry, 'y', rotY * DEG2RAD);
    guMtxRotRad(rx, 'x', rotX * DEG2RAD);
    guMtxRotRad(rz, 'z', rotZ * DEG2RAD);
    guMtxConcat(ry, rx, tmp);
    guMtxConcat(tmp, rz, rot);

    Mtx eyeMtx;
    guMtxCopy(rot, eyeMtx);
    eyeMtx[0][3] = ex;
    eyeMtx[1][3] = ey;
    eyeMtx[2][3] = ez;

    // ── Sink arm down local Y so only the top ~35% is on screen ──────────
    Mtx shift, final;
    guMtxIdentity(shift);
    guMtxTransApply(shift, shift, 0.0f, -0.34f, 0.0f);
    guMtxConcat(eyeMtx, shift, final);
    GX_LoadPosMtxImm(final, GX_PNMTX0);

    // ── Scale — Minecraft proportions 0.3 : 1.0 : 0.3 ───────────────────
    draw_box(0.18f, 0.60f, 0.18f, 198, 145, 100);

    // ── Restore 3D state ──────────────────────────────────────────────────
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GX_SetCullMode(GX_CULL_BACK);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    Config_ApplyFog();
}