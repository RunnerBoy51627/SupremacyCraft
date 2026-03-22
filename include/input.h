#ifndef INPUT_H
#define INPUT_H

// ── input.h ──────────────────────────────────────────────────────────────────
// Unified input layer for GC pad and Wii Remote + Nunchuk.
// Call Input_Init() once, then Input_Scan() each frame.
// Read state via the Input_* accessors — same API regardless of controller.
// ─────────────────────────────────────────────────────────────────────────────

#include "platform_types.h"
#ifndef _PC
  #include <wiiuse/wpad.h>
#endif

// ── Init / scan ───────────────────────────────────────────────────────────────
void Input_Init(void);
void Input_Scan(void);

// ── Button state (mirrors PAD_ButtonsDown / PAD_ButtonsHeld) ─────────────────
// These use the same PAD_BUTTON_* bitmasks so existing code needs no changes
u32  Input_ButtonsDown(void);   // pressed this frame
u32  Input_ButtonsHeld(void);   // held this frame

// ── Analog ────────────────────────────────────────────────────────────────────
s8   Input_StickX(void);        // left stick / nunchuk X  (-128..127)
s8   Input_StickY(void);        // left stick / nunchuk Y  (-128..127)
s8   Input_CStickX(void);       // c-stick / IR look X     (-128..127)
s8   Input_CStickY(void);       // c-stick / IR look Y     (-128..127)
u8   Input_TriggerL(void);      // L trigger / Z nunchuk   (0..255)
u8   Input_TriggerR(void);      // R trigger / B wiimote   (0..255)

#endif