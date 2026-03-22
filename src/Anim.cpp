#include "anim.h"
#include "utils.h"
#include <math.h>
#include <gccore.h>

#define PI      3.14159265f
#define DEG2RAD (PI / 180.0f)

static inline float lerpf(float a, float b, float t) { return a+(b-a)*t; }
static inline float clampf(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }
static inline float smoothstep(float t){ t=clampf(t,0,1); return t*t*(3-2*t); }

// ── Box draw — TEX0 kept GX_DIRECT with dummy coords, TEV=PASSCLR ─────────────
static void draw_box(float w, float h, float d, u8 r, u8 g, u8 b)
{
    float hw=w*.3f, hh=h*.5f, hd=d*.3f;
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
    anim->arm.pos   = (guVector){.2,0,-0.5};
    anim->arm.rot   = (guVector){0,0,0};
    anim->arm.scale = (guVector){0.5,1,0.5};
    anim->hand.pos   = (guVector){.2,0,-0.5};
    anim->hand.rot   = (guVector){0,0,0};
    anim->hand.scale = (guVector){.5,1,.5};
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
// Strategy: load an IDENTITY view matrix, then position the arm in a fixed
// clip-like space so it never moves with the camera.
// We switch the position matrix to identity (no view transform), place the arm
// at a hardcoded eye-space position, and restore afterwards.
// GX_PNMTX0 is loaded per-object so this doesn't break the world render.

void Anim_DrawHand(HandAnim* anim, FreeCam* cam) {
    (void)cam;

    GX_SetFog(GX_FOG_NONE, 0, 0, 0, 0, (GXColor){0,0,0,0});
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetCullMode(GX_CULL_BACK);

    // ── Eye-space anchor ──────────────────────────────────────────────────
    // Coordinate notes (GX eye-space, identity view matrix):
    //   +X = right,  +Y = up,  -Z = into screen (right-handed, NDC near ~0.1)
    //
    // Target pose (Minecraft Beta 1.x first-person):
    //   - Arm visible in bottom-right corner, only top ~35% above screen edge
    //   - Front face faces viewer; narrow side faces screen-left
    //   - Top face slightly tilted toward viewer (nearly horizontal)
    //
    // ex/ey/ez position the arm's *centre* in eye space BEFORE the local
    // downward shift.  Keep ex modest — the post-rotation local Y shift has
    // a rightward component at -45° Y that already pushes the arm right.
    float ex =  0.26f + anim->arm.pos.x * 0.5f;  // right of screen centre
    float ey = -0.20f + anim->arm.pos.y;          // slightly below centre
    float ez = -0.45f;                             // depth (near = larger)

    // Rotations:
    //   Y: -45°  inward tilt — narrow side faces left, front/top face viewer
    //   X: -10°  slight backward tilt so top face reads as nearly horizontal
    //   Z: idle breathing roll (very subtle, from Anim_Update)

    float restRX = 10.0f + anim->arm.rot.x; // left or right?
    float restRY = -95.0f; // upwards and downwards!
    float restRZ = -15.0f + anim->arm.rot.z; // tilt forward and back

    Mtx ry, rx, rz, rot, tmp;
    guMtxRotRad(ry, 'y', restRY * DEG2RAD);
    guMtxRotRad(rx, 'x', restRX * DEG2RAD);
    guMtxRotRad(rz, 'z', restRZ * DEG2RAD);
    guMtxConcat(ry, rx, tmp);
    guMtxConcat(tmp, rz, rot);

    Mtx eyeMtx;
    guMtxCopy(rot, eyeMtx);
    eyeMtx[0][3] = ex;
    eyeMtx[1][3] = ey;
    eyeMtx[2][3] = ez;

    // Local-Y shift: moves along the arm's own long axis (after rotation),
    // sinking the wrist/base below the screen bottom while keeping the
    // top of the arm visible.  At -45° Y the local-Y axis has a +X component
    // in eye space (~0.7x), so keep this shift conservative to avoid sliding
    // the arm off the right edge of the screen.
    Mtx shift, final;
    guMtxIdentity(shift);
    guMtxTransApply(shift, shift, 0.0f, -0.22f, 0.0f);
    guMtxConcat(eyeMtx, shift, final);
    GX_LoadPosMtxImm(final, GX_PNMTX0);

    // Square cross-section arm (4×4×12 game units scaled to eye space).
    draw_box(0.22f, 0.58f, 0.22f, 198, 145, 100);

    // Restore full 3D state
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GX_SetCullMode(GX_CULL_BACK);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    Config_ApplyFog();
}