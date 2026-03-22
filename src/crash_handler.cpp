#ifndef _PC
#include <gccore.h>
#include <stdio.h>
#include <string.h>

// ── Simple crash screen using just printf + CON_Init ─────────────────────────
// Shows basic info and waits for button press to reboot.
// ─────────────────────────────────────────────────────────────────────────────

void CrashHandler_ShowScreen(const char* msg) {
    VIDEO_Init();
    GXRModeObj* rmode = VIDEO_GetPreferredMode(NULL);
    void* fb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(fb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    VIDEO_WaitVSync();

    CON_InitEx(rmode, 10, 10, rmode->fbWidth - 20, rmode->xfbHeight - 20);

    printf("\x1b[2J");
    printf("\x1b[1;31m*** MY3DSCRAFT CRASH ***\x1b[0m\n\n");
    printf("%s\n\n", msg);
    printf("\x1b[33mPress any GC button to reboot.\x1b[0m\n");

    PAD_Init();
    while (1) {
        VIDEO_WaitVSync();
        PAD_ScanPads();
        if (PAD_ButtonsDown(0) || PAD_ButtonsDown(1))
            SYS_ResetSystem(SYS_RESTART, 0, 0);
    }
}

// ── Install — no-op for now, call CrashHandler_ShowScreen() manually ─────────
void CrashHandler_Install(void) {
    // Nothing needed — call CrashHandler_ShowScreen() from catch sites
}

void CrashHandler_InstallGX(void) {}

#endif // _PC