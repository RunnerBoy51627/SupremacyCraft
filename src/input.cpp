#include "input.h"
#include <string.h>

// ── Internal state ────────────────────────────────────────────────────────────
static u32 s_down  = 0;
static u32 s_held  = 0;
static s8  s_sx    = 0, s_sy = 0;
static s8  s_cx    = 0, s_cy = 0;
static u8  s_tl    = 0, s_tr = 0;

// ── Accessors ─────────────────────────────────────────────────────────────────
u32 Input_ButtonsDown(void) { return s_down; }
u32 Input_ButtonsHeld(void) { return s_held; }
s8  Input_StickX(void)      { return s_sx; }
s8  Input_StickY(void)      { return s_sy; }
s8  Input_CStickX(void)     { return s_cx; }
s8  Input_CStickY(void)     { return s_cy; }
u8  Input_TriggerL(void)    { return s_tl; }
u8  Input_TriggerR(void)    { return s_tr; }

// ─────────────────────────────────────────────────────────────────────────────
#ifdef _PC
// ── PC: delegate to existing pc_input globals ─────────────────────────────────
// (pc_input.cpp fills g_pc_buttons_down etc. via Platform_PollInput)
extern u32 g_pc_buttons_down;
extern u32 g_pc_buttons_held;
extern s8  g_pc_stick_x, g_pc_stick_y;
extern s8  g_pc_cstick_x, g_pc_cstick_y;
extern u8  g_pc_trigger_l, g_pc_trigger_r;

void Input_Init(void) {}
void Input_Scan(void) {
    s_down = g_pc_buttons_down;
    s_held = g_pc_buttons_held;
    s_sx   = g_pc_stick_x;  s_sy = g_pc_stick_y;
    s_cx   = g_pc_cstick_x; s_cy = g_pc_cstick_y;
    s_tl   = g_pc_trigger_l; s_tr = g_pc_trigger_r;
}

// ─────────────────────────────────────────────────────────────────────────────
#else
// ── GC/Wii ────────────────────────────────────────────────────────────────────

// Detect if running on Wii at runtime
#include <ogc/machine/processor.h>

static int s_is_wii = 0;

// IR look: track previous position to get delta
static float s_ir_prev_x = 320.0f;
static float s_ir_prev_y = 240.0f;
static int   s_ir_valid  = 0;

// Clamp to s8
static inline s8 clamp_s8(float v) {
    if (v >  127.0f) return  127;
    if (v < -128.0f) return -128;
    return (s8)v;
}

void Input_Init(void) {
    PAD_Init();

#ifndef HW_DOL
    // Check if we're on Wii (mfpvr returns 0x7001 on Broadway, 0x0008 on Gekko)
    u32 pvr = mfpvr();
    s_is_wii = ((pvr >> 16) == 0x0007); // Broadway = 0x7001
    if (s_is_wii) {
        WPAD_Init();
        WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
        WPAD_SetVRes(WPAD_CHAN_0, 640, 480);
    }
#endif
}

void Input_Scan(void) {
    // ── Always scan GC pad ────────────────────────────────────────────────────
    PAD_ScanPads();
    u32 gc_down = PAD_ButtonsDown(0);
    u32 gc_held = PAD_ButtonsHeld(0);
    s8  gc_sx   = PAD_StickX(0);
    s8  gc_sy   = PAD_StickY(0);
    s8  gc_cx   = PAD_SubStickX(0);
    s8  gc_cy   = PAD_SubStickY(0);
    u8  gc_tl   = PAD_TriggerL(0);
    u8  gc_tr   = PAD_TriggerR(0);

    // If GC pad is connected and has input, prefer it
    int gc_active = (gc_held != 0 || gc_sx != 0 || gc_sy != 0 ||
                     gc_cx != 0 || gc_cy != 0);

#ifndef HW_DOL
    if (s_is_wii) {
        WPAD_ScanPads();
        WPADData* wd = WPAD_Data(WPAD_CHAN_0);

        if (wd && wd->err == WPAD_ERR_NONE && !gc_active) {
            // ── Wii buttons → PAD bitmasks ────────────────────────────────────
            u32 wb  = wd->btns_h;  // held
            u32 wbd = wd->btns_d;  // down this frame
            u32 w_held = 0, w_down = 0;

            // Wiimote buttons
            if (wb  & WPAD_BUTTON_A)     w_held |= PAD_BUTTON_A;
            if (wb  & WPAD_BUTTON_B)     w_held |= PAD_TRIGGER_R;  // break
            if (wb  & WPAD_BUTTON_PLUS)  w_held |= PAD_BUTTON_X;   // inventory
            if (wb  & WPAD_BUTTON_MINUS) w_held |= PAD_BUTTON_Y;
            if (wb  & WPAD_BUTTON_HOME)  w_held |= PAD_BUTTON_START;
            if (wb  & WPAD_BUTTON_UP)    w_held |= PAD_BUTTON_UP;
            if (wb  & WPAD_BUTTON_DOWN)  w_held |= PAD_BUTTON_DOWN;
            if (wb  & WPAD_BUTTON_LEFT)  w_held |= PAD_BUTTON_LEFT;
            if (wb  & WPAD_BUTTON_RIGHT) w_held |= PAD_BUTTON_RIGHT;

            if (wbd & WPAD_BUTTON_A)     w_down |= PAD_BUTTON_A;
            if (wbd & WPAD_BUTTON_B)     w_down |= PAD_TRIGGER_R;
            if (wbd & WPAD_BUTTON_PLUS)  w_down |= PAD_BUTTON_X;
            if (wbd & WPAD_BUTTON_MINUS) w_down |= PAD_BUTTON_Y;
            if (wbd & WPAD_BUTTON_HOME)  w_down |= PAD_BUTTON_START;
            if (wbd & WPAD_BUTTON_UP)    w_down |= PAD_BUTTON_UP;
            if (wbd & WPAD_BUTTON_DOWN)  w_down |= PAD_BUTTON_DOWN;
            if (wbd & WPAD_BUTTON_LEFT)  w_down |= PAD_BUTTON_LEFT;
            if (wbd & WPAD_BUTTON_RIGHT) w_down |= PAD_BUTTON_RIGHT;

            // Nunchuk
            if (wd->exp.type == WPAD_EXP_NUNCHUK) {
                joystick_t* nk = &wd->exp.nunchuk.js;
                s_sx = clamp_s8(nk->pos.x * 100.0f / 100.0f);
                s_sy = clamp_s8(nk->pos.y * 100.0f / 100.0f);

                // nunchuk_t only has btns (current state), track prev manually
                static u32 nk_prev = 0;
                u32 nb  = wd->exp.nunchuk.btns;
                u32 nbd = nb & ~nk_prev;
                nk_prev = nb;
                if (nb  & NUNCHUK_BUTTON_Z) { w_held |= PAD_TRIGGER_L; s_tl = 255; }
                if (nb  & NUNCHUK_BUTTON_C) { w_held |= PAD_BUTTON_B; }
                if (nbd & NUNCHUK_BUTTON_Z)   w_down |= PAD_TRIGGER_L;
                if (nbd & NUNCHUK_BUTTON_C)   w_down |= PAD_BUTTON_B;
            } else {
                s_sx = 0; s_sy = 0;
            }

            // IR pointer → look (delta from screen center)
            if (wd->ir.valid) {
                float cx = wd->ir.x, cy = wd->ir.y;
                if (s_ir_valid) {
                    float dx = (cx - s_ir_prev_x) * 0.8f;
                    float dy = (cy - s_ir_prev_y) * 0.8f;
                    s_cx = clamp_s8(dx);
                    s_cy = clamp_s8(-dy); // invert Y
                } else {
                    s_cx = 0; s_cy = 0;
                }
                s_ir_prev_x = cx;
                s_ir_prev_y = cy;
                s_ir_valid  = 1;
            } else {
                s_cx = 0; s_cy = 0;
                s_ir_valid = 0;
            }

            // L trigger from Z nunchuk already set above
            s_tr = (w_held & PAD_TRIGGER_R) ? 255 : 0;
            s_down = w_down;
            s_held = w_held;
            return;
        }
    }
#endif

    // ── Fallback: GC pad ──────────────────────────────────────────────────────
    s_down = gc_down;
    s_held = gc_held;
    s_sx   = gc_sx;  s_sy = gc_sy;
    s_cx   = gc_cx;  s_cy = gc_cy;
    s_tl   = gc_tl;  s_tr = gc_tr;
}

#endif // _PC