#include "anim.h"
#include "utils.h"
#include "textures.h"
#include "atlas_regions.h"
#include "chunk.h"
#include <math.h>
#include <gccore.h>

#define PI      3.14159265f
#define DEG2RAD (PI / 180.0f)

static inline float lerpf(float a, float b, float t) { return a+(b-a)*t; }
static inline float clampf(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }
static inline float smoothstep(float t){ t=clampf(t,0,1); return t*t*(3-2*t); }

static inline int is_tool_item(u8 block) { return block == BLOCK_FLINT_STEEL; }
static inline int tool_tex_id(u8 block) { (void)block; return TEX_FLINT_STEEL; }

// ── Box draw — flat color, no texture ────────────────────────────────────────
static void draw_box(float w, float h, float d, u8 r, u8 g, u8 b)
{
    float hw=w*.5f, hh=h*.5f, hd=d*.5f;
    u8 rt=r,            gt_=g,            bt_=b;
    u8 rf=(u8)(r*.80f), gf=(u8)(g*.80f),  bf=(u8)(b*.80f);
    u8 rs=(u8)(r*.62f), gs=(u8)(g*.62f),  bs_=(u8)(b*.62f);
    u8 rb=(u8)(r*.44f), gb=(u8)(g*.44f),  bb=(u8)(b*.44f);

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
    anim->arm.pos   = (guVector){0,0,0};
    anim->arm.rot   = (guVector){0,0,0};
    anim->arm.scale = (guVector){1,1,1};
    anim->hand.pos   = (guVector){0,0,0};
    anim->hand.rot   = (guVector){0,0,0};
    anim->hand.scale = (guVector){1,1,1};
    anim->bobPhase     = 0.0f;
    anim->bobIntensity = 0.0f;
    anim->lookDX       = 0.0f;
    anim->lookDY       = 0.0f;
    anim->heldBlock    = 0;
}

void Anim_SetBob(HandAnim* anim, float phase, float intensity) {
    anim->bobPhase     = phase;
    anim->bobIntensity = intensity;
}

void Anim_SetLook(HandAnim* anim, float deltaYaw, float deltaPitch) {
    float smooth = 0.15f;
    anim->lookDX = lerpf(anim->lookDX, deltaYaw,   smooth);
    anim->lookDY = lerpf(anim->lookDY, deltaPitch, smooth);
}

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

    float swDY=0, swRX=0;
    if (anim->state == ANIM_SWING) {
        float phase = clampf(anim->timer / 14.0f, 0.0f, 1.0f);
        if (phase < 0.4f) {
            float t = smoothstep(phase / 0.4f);
            swDY = lerpf(0.0f, -0.07f, t);
            swRX = lerpf(0.0f,  60.0f, t);
        } else {
            float t = smoothstep((phase-0.4f)/0.6f);
            swDY = lerpf(-0.07f, 0.0f, t);
            swRX = lerpf( 60.0f, 0.0f, t);
        }
        if (phase >= 1.0f) anim->state = ANIM_IDLE;
    }

    float eqDY=0, eqRX=0;
    if (anim->state == ANIM_EQUIP) {
        float t = smoothstep(clampf(anim->timer/8.0f,0,1));
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

    // Sway and look offsets
    float swayX  =  sinf(anim->bobPhase) * anim->bobIntensity * 0.04f;
    float swayY  = -fabsf(sinf(anim->bobPhase)) * anim->bobIntensity * 0.03f;
    float swayRX =  sinf(anim->bobPhase) * anim->bobIntensity * 3.0f;
    float swayRZ =  cosf(anim->bobPhase * 0.5f) * anim->bobIntensity * 1.5f;
    float lookOffX = -anim->lookDX * 0.04f;
    float lookOffY =  anim->lookDY * 0.03f;
    float lookRollZ  = -anim->lookDX * 2.8f;
    float lookPitchX = -anim->lookDY * 3.5f;

    if (anim->heldBlock != 0) {
        // ── Held BLOCK ────────────────────────────────────────────────────
        float ex =  0.38f + anim->arm.pos.x + swayX + lookOffX;
        float ey = -0.26f + anim->arm.pos.y + swayY + lookOffY;
        float ez = -0.90f;

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

        Mtx shift, final;
        guMtxIdentity(shift);
        guMtxTransApply(shift, shift, 0.10f, -0.14f, 0.0f);
        guMtxConcat(eyeMtx, shift, final);
        GX_LoadPosMtxImm(final, GX_PNMTX0);

        GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        Tex_BindAtlas();

        if (is_tool_item(anim->heldBlock)) {
            // Tool sprite: own matrix, smaller, rotated diagonal
            float tex = 0.30f + anim->arm.pos.x + swayX + lookOffX;
            float tey = -0.20f + anim->arm.pos.y + swayY + lookOffY;
            float tez = -0.85f;
            Mtx try_, trz_, ttmp_;
            guMtxRotRad(try_, 'y', -30.0f * DEG2RAD);
            guMtxRotRad(trz_, 'z', -30.0f * DEG2RAD + swayRZ * DEG2RAD);
            guMtxConcat(try_, trz_, ttmp_);
            ttmp_[0][3] = tex; ttmp_[1][3] = tey; ttmp_[2][3] = tez;
            GX_LoadPosMtxImm(ttmp_, GX_PNMTX0);
            const TexRegion* tsp = Tex_GetRegion(tool_tex_id(anim->heldBlock));
            float sw = 0.20f, sh = 0.20f;
            GX_Begin(GX_QUADS, GX_VTXFMT0, 8);
            GX_Position3f32(-sw, sh,0);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tsp->u0,tsp->v0);
            GX_Position3f32( sw, sh,0);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tsp->u1,tsp->v0);
            GX_Position3f32( sw,-sh,0);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tsp->u1,tsp->v1);
            GX_Position3f32(-sw,-sh,0);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tsp->u0,tsp->v1);
            GX_Position3f32( sw, sh,0);GX_Color4u8(200,200,200,255);GX_TexCoord2f32(tsp->u0,tsp->v0);
            GX_Position3f32(-sw, sh,0);GX_Color4u8(200,200,200,255);GX_TexCoord2f32(tsp->u1,tsp->v0);
            GX_Position3f32(-sw,-sh,0);GX_Color4u8(200,200,200,255);GX_TexCoord2f32(tsp->u1,tsp->v1);
            GX_Position3f32( sw,-sh,0);GX_Color4u8(200,200,200,255);GX_TexCoord2f32(tsp->u0,tsp->v1);
            GX_End();
            GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
            GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        } else {
            // Textured block cube
            float s = 0.22f;
            const TexRegion* tt  = Tex_GetRegion(block_face_tex_pub(anim->heldBlock, 0));
            const TexRegion* tbt = Tex_GetRegion(block_face_tex_pub(anim->heldBlock, 1));
            const TexRegion* tn  = Tex_GetRegion(block_face_tex_pub(anim->heldBlock, 2));
            const TexRegion* tss = Tex_GetRegion(block_face_tex_pub(anim->heldBlock, 3));
            const TexRegion* tw  = Tex_GetRegion(block_face_tex_pub(anim->heldBlock, 4));
            const TexRegion* te  = Tex_GetRegion(block_face_tex_pub(anim->heldBlock, 5));
            GX_Begin(GX_QUADS, GX_VTXFMT0, 24);
            GX_Position3f32(-s,s,-s);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tt->u0,tt->v0);
            GX_Position3f32( s,s,-s);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tt->u1,tt->v0);
            GX_Position3f32( s,s, s);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tt->u1,tt->v1);
            GX_Position3f32(-s,s, s);GX_Color4u8(255,255,255,255);GX_TexCoord2f32(tt->u0,tt->v1);
            GX_Position3f32(-s,-s, s);GX_Color4u8(130,130,130,255);GX_TexCoord2f32(tbt->u0,tbt->v0);
            GX_Position3f32( s,-s, s);GX_Color4u8(130,130,130,255);GX_TexCoord2f32(tbt->u1,tbt->v0);
            GX_Position3f32( s,-s,-s);GX_Color4u8(130,130,130,255);GX_TexCoord2f32(tbt->u1,tbt->v1);
            GX_Position3f32(-s,-s,-s);GX_Color4u8(130,130,130,255);GX_TexCoord2f32(tbt->u0,tbt->v1);
            GX_Position3f32( s,s,-s);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tn->u0,tn->v0);
            GX_Position3f32(-s,s,-s);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tn->u1,tn->v0);
            GX_Position3f32(-s,-s,-s);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tn->u1,tn->v1);
            GX_Position3f32( s,-s,-s);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tn->u0,tn->v1);
            GX_Position3f32(-s,s, s);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tss->u0,tss->v0);
            GX_Position3f32( s,s, s);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tss->u1,tss->v0);
            GX_Position3f32( s,-s, s);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tss->u1,tss->v1);
            GX_Position3f32(-s,-s, s);GX_Color4u8(210,210,210,255);GX_TexCoord2f32(tss->u0,tss->v1);
            GX_Position3f32(-s,s,-s);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(tw->u0,tw->v0);
            GX_Position3f32(-s,s, s);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(tw->u1,tw->v0);
            GX_Position3f32(-s,-s, s);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(tw->u1,tw->v1);
            GX_Position3f32(-s,-s,-s);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(tw->u0,tw->v1);
            GX_Position3f32( s,s, s);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(te->u0,te->v0);
            GX_Position3f32( s,s,-s);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(te->u1,te->v0);
            GX_Position3f32( s,-s,-s);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(te->u1,te->v1);
            GX_Position3f32( s,-s, s);GX_Color4u8(180,180,180,255);GX_TexCoord2f32(te->u0,te->v1);
            GX_End();
            GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
            GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        } // end block cube

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