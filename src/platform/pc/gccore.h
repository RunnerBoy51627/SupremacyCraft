// gccore.h — PC shim
// When building with -D_PC, the compiler finds this file (via -Isrc/platform/pc)
// before any system gccore.h, redirecting all GC includes to the PC compat layer.
#pragma once
#include "gc_compat.h"