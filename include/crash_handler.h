#pragma once
#ifndef _PC
void CrashHandler_Install(void);
void CrashHandler_InstallGX(void);
void CrashHandler_ShowScreen(const char* msg);
#else
static inline void CrashHandler_Install(void) {}
static inline void CrashHandler_InstallGX(void) {}
static inline void CrashHandler_ShowScreen(const char*) {}
#endif