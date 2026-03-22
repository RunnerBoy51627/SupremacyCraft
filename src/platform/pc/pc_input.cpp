#include "gc_compat.h"
#include <stdio.h>

// ── Input state ───────────────────────────────────────────────────────────────
u32 g_pc_buttons_held = 0;
u32 g_pc_buttons_down = 0;
s8  g_pc_stick_x  = 0;
s8  g_pc_stick_y  = 0;
s8  g_pc_cstick_x = 0;
s8  g_pc_cstick_y = 0;
u8  g_pc_trigger_l = 0;
u8  g_pc_trigger_r = 0;

// Mouse look state
static float s_mouse_dx = 0;
static float s_mouse_dy = 0;
static int   s_mouse_captured = 0;
static int   s_quit_requested = 0;

int Platform_ShouldQuit(void) { return s_quit_requested; }

// ── Keyboard → GC mapping ────────────────────────────────────────────────────
// Movement:  WASD or arrow keys   → left stick
// Look:      Mouse                → C-stick (converted to stick units)
// Jump:      Space                → A button
// Attack:    Left click           → R trigger (break)
// Place:     Right click          → L trigger (place)
// Inventory: E                    → X button
// Hotbar:    1-9 or scroll wheel  → D-pad left/right
// Sprint:    Left Shift           → held (affects stick magnitude)
// Pause:     Escape               → START
// LB/craft:  Q                    → L trigger (when inventory open)
// Drop:      G                    → D-pad down
// B button:  F                    → B button (half stack)

#define MOUSE_SENSITIVITY 3.0f

void Platform_PollInput(void) {
    u32 prev_held = g_pc_buttons_held;
    g_pc_buttons_down = 0;
    s_mouse_dx = 0;
    s_mouse_dy = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                s_quit_requested = 1;
                break;

            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    if (s_mouse_captured) {
                        // First escape releases mouse
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        s_mouse_captured = 0;
                    } else {
                        g_pc_buttons_down |= PAD_BUTTON_START;
                    }
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (!s_mouse_captured) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    s_mouse_captured = 1;
                }
                break;

            case SDL_MOUSEMOTION:
                if (s_mouse_captured) {
                    s_mouse_dx += e.motion.xrel;
                    s_mouse_dy += e.motion.yrel;
                }
                break;

            case SDL_MOUSEWHEEL:
                if (e.wheel.y > 0) g_pc_buttons_down |= PAD_BUTTON_LEFT;
                if (e.wheel.y < 0) g_pc_buttons_down |= PAD_BUTTON_RIGHT;
                break;
        }
    }

    // ── Keyboard state → buttons held ────────────────────────────────────────
    const u8* keys = SDL_GetKeyboardState(NULL);
    u32 new_held = 0;

    // Gameplay buttons
    if (keys[SDL_SCANCODE_SPACE])  new_held |= PAD_BUTTON_A;     // jump
    if (keys[SDL_SCANCODE_E])      new_held |= PAD_BUTTON_X;     // inventory
    if (keys[SDL_SCANCODE_Q])      new_held |= PAD_TRIGGER_L;    // LB / craft focus
    if (keys[SDL_SCANCODE_F])      new_held |= PAD_BUTTON_B;     // half stack
    if (keys[SDL_SCANCODE_G])      new_held |= PAD_BUTTON_DOWN;  // drop item
    if (keys[SDL_SCANCODE_RETURN]) new_held |= PAD_BUTTON_START;

    // Inventory navigation (arrow keys when inventory open)
    if (keys[SDL_SCANCODE_UP])    new_held |= PAD_BUTTON_UP;
    if (keys[SDL_SCANCODE_DOWN])  new_held |= PAD_BUTTON_DOWN;
    if (keys[SDL_SCANCODE_LEFT])  new_held |= PAD_BUTTON_LEFT;
    if (keys[SDL_SCANCODE_RIGHT]) new_held |= PAD_BUTTON_RIGHT;

    // Hotbar slots 1-9 → D-pad emulation via direct selectedSlot set
    // (handled as PAD_BUTTON_RIGHT/LEFT for now)

    // Mouse buttons
    int mouse_state = SDL_GetMouseState(NULL, NULL);
    if (s_mouse_captured) {
        if (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT))  new_held |= (1 << 16); // break (R trigger)
        if (mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) new_held |= (1 << 17); // place (L trigger)
        if (mouse_state & SDL_BUTTON(SDL_BUTTON_MIDDLE))new_held |= PAD_BUTTON_A; // pick block
    }

    // Buttons DOWN = newly pressed this frame
    g_pc_buttons_down |= (new_held & ~prev_held);
    g_pc_buttons_held  = new_held;

    // ── Trigger values (0 or 255) ─────────────────────────────────────────────
    g_pc_trigger_r = (new_held & (1<<16)) ? 255 : 0; // left click  = R (break)
    g_pc_trigger_l = (new_held & (1<<17)) ? 255 : 0; // right click = L (place)

    // ── Left stick — WASD movement ────────────────────────────────────────────
    float sx = 0, sy = 0;
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    sy =  1.0f;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  sy = -1.0f;
    if (keys[SDL_SCANCODE_A])                              sx = -1.0f;
    if (keys[SDL_SCANCODE_D])                              sx =  1.0f;
    if (keys[SDL_SCANCODE_LSHIFT]) { sx *= 1.0f; sy *= 1.0f; } // sprint placeholder

    g_pc_stick_x = (s8)(sx * 100);
    g_pc_stick_y = (s8)(sy * 100);

    // ── C-stick — mouse look (convert pixel delta to stick units) ─────────────
    // Mouse → c-stick: scale delta directly, clamp to s8 range
    float cx = s_mouse_dx * MOUSE_SENSITIVITY;
    float cy = -s_mouse_dy * MOUSE_SENSITIVITY;
    if (cx >  127.0f) cx =  127.0f;
    if (cx < -127.0f) cx = -127.0f;
    if (cy >  127.0f) cy =  127.0f;
    if (cy < -127.0f) cy = -127.0f;
    g_pc_cstick_x = (s8)cx;
    g_pc_cstick_y = (s8)cy;
}