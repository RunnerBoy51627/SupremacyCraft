#pragma once
// ── sound.h ──────────────────────────────────────────────────────────────────
// Cross-platform sound system.
// GC/Wii: ASND + raw PCM embedded via bin2s
// PC:     SDL_mixer with MP3 files loaded from disk
// ─────────────────────────────────────────────────────────────────────────────

// Sound IDs
typedef enum {
    SFX_BLOCK_BREAK = 0,
    SFX_PLAYER_HIT,
    SFX_EXPLODE,
    SFX_COUNT
} SoundID;

void Sound_Init(void);
void Sound_Play(SoundID id);
void Sound_Shutdown(void);