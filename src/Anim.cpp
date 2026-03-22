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
    anim->bobPhase     = 0.0f;
    anim->bobIntensity = 0.0f;
    anim->lookDX       = 0.0f;
    anim->lookDY       = 0.0f;
    anim->heldBlock    = 0;    // BLOCK_AIR
}

// ── Colored box — each face has its own RGB ──────────────────────────────────
// Face order: top, bottom, front, back, right, left
static void draw_colored_box(float w, float h, float d,
    u8 tr, u8 tg, u8 tb,   // top
    u8 fr, u8 fg, u8 fb,   // front
    u8 rr, u8 rg, u8 rb)   // right  (back/left/bottom derived)
{
    float hw = w*0.5f, hh = h*0.5f, hd = d*0.5f;
    u8 br=(u8)(rr*.72f), bg=(u8)(rg*.72f), bb2=(u8)(rb*.72f); // back = slightly darker than right
    u8 lr=(u8)(rr*.85f), lg=(u8)(rg*.85f), lb=(u8)(rb*.85f);  // left = between front and right
    u8 botr=(u8)(tr*.40f),botg=(u8)(tg*.40f),botb=(u8)(tb*.40f); // bottom = very dark

    GX_Begin(GX_QUADS, GX_VTXFMT0, 24);
    // Top
    GX_Position3f32(-hw,hh,-hd); GX_Color4u8(tr,tg,tb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw,hh,-hd); GX_Color4u8(tr,tg,tb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw,hh, hd); GX_Color4u8(tr,tg,tb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw,hh, hd); GX_Color4u8(tr,tg,tb,255); GX_TexCoord2f32(0,0);
    // Bottom
    GX_Position3f32(-hw,-hh, hd); GX_Color4u8(botr,botg,botb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw,-hh, hd); GX_Color4u8(botr,botg,botb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw,-hh,-hd); GX_Color4u8(botr,botg,botb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw,-hh,-hd); GX_Color4u8(botr,botg,botb,255); GX_TexCoord2f32(0,0);
    // Front
    GX_Position3f32(-hw,-hh,hd); GX_Color4u8(fr,fg,fb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw,-hh,hd); GX_Color4u8(fr,fg,fb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw, hh,hd); GX_Color4u8(fr,fg,fb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw, hh,hd); GX_Color4u8(fr,fg,fb,255); GX_TexCoord2f32(0,0);
    // Back
    GX_Position3f32( hw,-hh,-hd); GX_Color4u8(br,bg,bb2,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw,-hh,-hd); GX_Color4u8(br,bg,bb2,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw, hh,-hd); GX_Color4u8(br,bg,bb2,255); GX_TexCoord2f32(0,0);
    GX_Position3f32( hw, hh,-hd); GX_Color4u8(br,bg,bb2,255); GX_TexCoord2f32(0,0);
    // Right
    GX_Position3f32(hw,-hh, hd); GX_Color4u8(rr,rg,rb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(hw,-hh,-hd); GX_Color4u8(rr,rg,rb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(hw, hh,-hd); GX_Color4u8(rr,rg,rb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(hw, hh, hd); GX_Color4u8(rr,rg,rb,255); GX_TexCoord2f32(0,0);
    // Left
    GX_Position3f32(-hw,-hh,-hd); GX_Color4u8(lr,lg,lb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw,-hh, hd); GX_Color4u8(lr,lg,lb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw, hh, hd); GX_Color4u8(lr,lg,lb,255); GX_TexCoord2f32(0,0);
    GX_Position3f32(-hw, hh,-hd); GX_Color4u8(lr,lg,lb,255); GX_TexCoord2f32(0,0);
    GX_End();
}

// ── Block face colors (matches gui.cpp palette) ───────────────────────────────
static void get_held_block_colors(u8 block,
    u8* tr, u8* tg, u8* tb,
    u8* fr, u8* fg, u8* fb,
    u8* rr, u8* rg, u8* rb)
{
    switch(block) {
        case 1: // GRASS
            *tr=106;*tg=178;*tb=80;  *fr=100;*fg=70;*fb=40;  *rr=115;*rg=80;*rb=48; break;
        case 2: // DIRT
            *tr=134;*tg=96;*tb=67;   *fr=110;*fg=75;*fb=50;  *rr=125;*rg=88;*rb=58; break;
        case 3: // STONE
            *tr=128;*tg=128;*tb=128; *fr=100;*fg=100;*fb=100;*rr=115;*rg=115;*rb=115; break;
        case 4: // WOOD
            *tr=160;*tg=130;*tb=80;  *fr=120;*fg=85;*fb=45;  *rr=140;*rg=100;*rb=55; break;
        case 5: // LEAF
            *tr=50;*tg=120;*tb=35;   *fr=38;*fg=95;*fb=28;   *rr=44;*rg=108;*rb=32; break;
        default:
            *tr=*tg=*tb=180; *fr=*fg=*fb=140; *rr=*rg=*rb=160; break;
    }
}

// ── SetBob ───────────────────────────────────────────────────────────────────
void Anim_SetBob(HandAnim* anim, float phase, float intensity) {
    anim->bobPhase     = phase;
    anim->bobIntensity = intensity;
}

// ── SetLook ──────────────────────────────────────────────────────────────────
// deltaYaw/deltaPitch are raw frame deltas (degrees or stick units).
// We lerp toward them so the hand slides smoothly rather than snapping.
void Anim_SetLook(HandAnim* anim, float deltaYaw, float deltaPitch) {
    // Clamp inputs so big fast swipes don't fly the hand off screen
    if (deltaYaw   >  5.0f) deltaYaw   =  5.0f;
    if (deltaYaw   < -5.0f) deltaYaw   = -5.0f;
    if (deltaPitch >  5.0f) deltaPitch =  5.0f;
    if (deltaPitch < -5.0f) deltaPitch = -5.0f;
    // Snap toward new delta quickly, decay back to zero slowly
    anim->lookDX += (deltaYaw   - anim->lookDX) * 0.25f;
    anim->lookDY += (deltaPitch - anim->lookDY) * 0.25f;
    // Decay: pull toward zero when no input
    anim->lookDX *= 0.80f;
    anim->lookDY *= 0.80f;
}

// ── SetHeldBlock ─────────────────────────────────────────────────────────────
void Anim_SetHeldBlock(HandAnim* anim, u8 block) {
    anim->heldBlock = block;
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

    // ── Shared sway/bob offsets ───────────────────────────────────────────
    float bPhase = anim->bobPhase;
    float bI     = anim->bobIntensity;
    float swayX  =  sinf(bPhase)        *  0.03f  * bI;
    float swayY  =  sinf(bPhase * 2.0f) * -0.025f * bI;
    float swayRZ =  sinf(bPhase)        *  2.5f   * bI;
    float swayRX =  sinf(bPhase * 2.0f) *  4.0f   * bI;

    float lookOffX   = -anim->lookDX * 0.018f;
    float lookOffY   = -anim->lookDY * 0.015f;
    float lookRollZ  = -anim->lookDX * 2.8f;
    float lookPitchX = -anim->lookDY * 3.5f;

    if (anim->heldBlock != 0) {
        // ── Held BLOCK ────────────────────────────────────────────────────
        // Classic Minecraft: block rotated ~-30° Y, 25° X, -25° Z
        // Sits lower-right, larger than the arm
        float ex =  0.30f + anim->arm.pos.x + swayX + lookOffX;
        float ey = -0.20f + anim->arm.pos.y + swayY + lookOffY;
        float ez = -0.70f;

        float rotY = -30.0f;
        float rotX =  25.0f + swayRX + lookPitchX;
        float rotZ = -25.0f + swayRZ + lookRollZ;

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

        // Shift so block sits in lower-right corner
        Mtx shift, final;
        guMtxIdentity(shift);
        guMtxTransApply(shift, shift, 0.14f, -0.18f, 0.0f);
        guMtxConcat(eyeMtx, shift, final);
        GX_LoadPosMtxImm(final, GX_PNMTX0);

        u8 tr,tg,tb, fr,fg,fb, rr,rg,rb;
        get_held_block_colors(anim->heldBlock, &tr,&tg,&tb, &fr,&fg,&fb, &rr,&rg,&rb);
        draw_colored_box(0.28f, 0.28f, 0.28f, tr,tg,tb, fr,fg,fb, rr,rg,rb);

    } else {
        // ── Bare ARM ──────────────────────────────────────────────────────
        float ex =  0.44f + anim->arm.pos.x + swayX + lookOffX;
        float ey = -0.30f + anim->arm.pos.y + swayY + lookOffY;
        float ez = -0.82f;

        float rotY = -45.0f;
        float rotX = -15.0f + anim->arm.rot.x + swayRX + lookPitchX;
        float rotZ =           anim->arm.rot.z + swayRZ + lookRollZ;

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

        Mtx shift, final;
        guMtxIdentity(shift);
        guMtxTransApply(shift, shift, 0.0f, -0.40f, 0.0f);
        guMtxConcat(eyeMtx, shift, final);
        GX_LoadPosMtxImm(final, GX_PNMTX0);

        draw_box(0.18f, 0.60f, 0.18f, 198, 145, 100);
    }

    // ── Restore 3D state ──────────────────────────────────────────────────
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GX_SetCullMode(GX_CULL_BACK);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    Config_ApplyFog();
}