#pragma once
// ── platform.h ───────────────────────────────────────────────────────────────
// Cross-platform bootstrap functions.
// main.cpp calls these instead of VIDEO_Init / PAD_Init etc. directly.
// Each platform implements them in its own pc_window.cpp / gc_window.cpp.
// ─────────────────────────────────────────────────────────────────────────────

// Initialise window, GL/GX context, input
void Platform_InitWindow(const char* title, int w, int h);

// Poll input events — updates PAD_ButtonsDown/Held/Stick state
void Platform_PollInput(void);

// Returns 1 if the user requested quit (window close / OS signal)
int Platform_ShouldQuit(void);

// Tear down window and context
void Platform_DestroyWindow(void);