#pragma once
// platform_types.h — u8/u16/u32/s8/s16/s32/f32 on all platforms
#ifdef _PC
  #include <stdint.h>
  #include <stdbool.h>
  typedef uint8_t  u8;
  typedef uint16_t u16;
  typedef uint32_t u32;
  typedef int8_t   s8;
  typedef int16_t  s16;
  typedef int32_t  s32;
  typedef float    f32;
  #ifndef NULL
    #define NULL 0
  #endif
#else
  #include <gccore.h>
#endif