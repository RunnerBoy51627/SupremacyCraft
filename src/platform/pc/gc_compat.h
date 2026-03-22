#pragma once
// ── pc_compat.h ──────────────────────────────────────────────────────────────
// Replaces <gccore.h> and <ogc/lwp_watchdog.h> on PC builds.
// Provides GX / PAD / VIDEO / math stubs that route to SDL2 + OpenGL 2.x.
// The game source files include this via the -I path — no changes needed there.
// ─────────────────────────────────────────────────────────────────────────────

// GLEW — provides GL 2.0+ functions (glCreateShader etc)
#include <GL/glew.h>

// OpenGL 1.3+ constants — must be defined before any GL header is included
#ifndef GL_COMBINE
#define GL_COMBINE            0x8570
#define GL_COMBINE_RGB        0x8571
#define GL_COMBINE_ALPHA      0x8572
#define GL_SOURCE0_RGB        0x8580
#define GL_SOURCE0_ALPHA      0x8588
#define GL_PRIMARY_COLOR      0x8577
#define GL_CLAMP_TO_EDGE      0x812F
#endif
#include <SDL2/SDL.h>
#ifdef __APPLE__
  #include <OpenGL/gl.h>
  #include <OpenGL/glu.h>
#else
  #include <GL/gl.h>
  #include <GL/glu.h>
#endif
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ── Integer types (libogc style) ─────────────────────────────────────────────
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef float    f32;

// ── GX constants ─────────────────────────────────────────────────────────────
#define GX_TRUE   1
#define GX_FALSE  0
#define GX_ENABLE 1
#define GX_DISABLE 0

// Primitive types (mapped to GL)
#define GX_QUADS   GL_QUADS
#define GX_LINES   GL_LINES

// Vertex format (ignored on PC — we use immediate mode)
#define GX_VTXFMT0    0
#define GX_PNMTX0     0
#define GX_VA_POS     0
#define GX_VA_CLR0    1
#define GX_VA_TEX0    2
#define GX_POS_XYZ    0
#define GX_CLR_RGBA   0
#define GX_TEX_ST     0
#define GX_F32        0
#define GX_RGBA8      0
#define GX_DIRECT     0
#define GX_NONE       0
#define GX_TF_RGBA8   GL_RGBA

// TEV ops (mapped to GL texture env)
#define GX_PASSCLR     0
#define GX_MODULATE    1
#define GX_TEVSTAGE0   GL_TEXTURE_ENV
#define GX_TEXCOORD0   0
#define GX_TEXCOORDNULL 0
#define GX_TEXMAP0     0
#define GX_TEXMAP_NULL 0
#define GX_COLOR0A0    0

// Depth test
#define GX_LEQUAL  GL_LEQUAL
#define GX_ALWAYS  GL_ALWAYS

// Cull mode
#define GX_CULL_NONE  0
#define GX_CULL_BACK  1
#define GX_CULL_FRONT 2

// Blend
#define GX_BM_NONE     0
#define GX_BM_BLEND    1
#define GX_BL_ZERO       GL_ZERO
#define GX_BL_ONE        GL_ONE
#define GX_BL_SRCALPHA   GL_SRC_ALPHA
#define GX_BL_INVSRCALPHA GL_ONE_MINUS_SRC_ALPHA
#define GX_LO_CLEAR    0

// Fog
#define GX_FOG_NONE    0
#define GX_FOG_LIN     1

// Texture filter
#define GX_NEAR     GL_NEAREST
#define GX_CLAMP    GL_CLAMP_TO_EDGE

// Projection
#define GX_PERSPECTIVE    0
#define GX_ORTHOGRAPHIC   1

// Misc
#define FIFO_SIZE 0

// ── Types ─────────────────────────────────────────────────────────────────────

typedef struct { u8 r, g, b, a; } GXColor;
typedef struct { f32 x, y, z; } guVector;

// 3x4 matrix (libogc row-major: Mtx[row][col])
typedef f32 Mtx[3][4];
typedef f32 Mtx44[4][4];

typedef struct { int w, h;
    int fbWidth;    // alias for w
    int efbHeight;  // alias for h
} GXRModeObj;
typedef struct { GLuint id; int w, h; } GXTexObj;

#include "pc_shader.h"

// ── Shader globals (defined in pc_window.cpp) ────────────────────────────────
extern GLuint g_shader_prog;
extern GLint  g_uni_tev_mode;
extern GLint  g_uni_fog_color;
extern GLint  g_uni_fog_enabled;
extern GLint  g_uni_fog_start;
extern GLint  g_uni_fog_end;
extern GLint  g_uni_tex;

// ── Global PC state (defined in pc_window.cpp) ────────────────────────────────
extern SDL_Window*   g_pc_window;
extern SDL_GLContext g_pc_glctx;
extern GXRModeObj    g_pc_rmode;  // .fbWidth and .efbHeight mirror .w and .h
extern int           g_pc_tev_mode;   // 0=PASSCLR, 1=MODULATE
extern int           g_pc_cull_mode;

// ── Math — backed by GLM ─────────────────────────────────────────────────────
// GLM uses column-major internally; we convert to/from libogc's row-major
// 3x4 Mtx layout at the GX_LoadPosMtxImm boundary only.
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>

// Helper: copy GLM mat4 into libogc Mtx (3x4 row-major, drop last row)
static inline void glm_to_mtx(const glm::mat4& g, Mtx m) {
    // GLM is column-major: g[col][row]
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++)
            m[r][c] = g[c][r];
}

static inline glm::mat4 mtx_to_glm(const Mtx m) {
    glm::mat4 g(0.0f);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++)
            g[c][r] = m[r][c];
    g[3][3] = 1.0f;
    return g;
}

static inline void guMtxIdentity(Mtx m) {
    glm_to_mtx(glm::mat4(1.0f), m);
}

static inline void guMtxCopy(Mtx src, Mtx dst) {
    memcpy(dst, src, sizeof(Mtx));
}

static inline void guMtxConcat(Mtx a, Mtx b, Mtx out) {
    glm::mat4 result = mtx_to_glm(a) * mtx_to_glm(b);
    glm_to_mtx(result, out);
}

static inline void guMtxTransApply(Mtx src, Mtx dst, f32 x, f32 y, f32 z) {
    glm::mat4 result = glm::translate(mtx_to_glm(src), glm::vec3(x, y, z));
    glm_to_mtx(result, dst);
}

static inline void guMtxRotRad(Mtx m, char axis, f32 angle) {
    glm::vec3 ax;
    switch (axis) {
        case 'x': ax = glm::vec3(1,0,0); break;
        case 'y': ax = glm::vec3(0,1,0); break;
        default:  ax = glm::vec3(0,0,1); break;
    }
    glm_to_mtx(glm::rotate(glm::mat4(1.0f), angle, ax), m);
}

static inline void guVecAdd(guVector* a, guVector* b, guVector* out) {
    glm::vec3 r = glm::vec3(a->x,a->y,a->z) + glm::vec3(b->x,b->y,b->z);
    out->x=r.x; out->y=r.y; out->z=r.z;
}

static inline void guLookAt(Mtx m, guVector* eye, guVector* up, guVector* at) {
    glm::mat4 lk = glm::lookAt(
        glm::vec3(eye->x, eye->y, eye->z),
        glm::vec3(at->x,  at->y,  at->z),
        glm::vec3(up->x,  up->y,  up->z));
    glm_to_mtx(lk, m);
}

static inline void guPerspective(Mtx44 m, f32 fovY, f32 aspect, f32 nearZ, f32 farZ) {
    glm::mat4 p = glm::perspective(glm::radians(fovY), aspect, nearZ, farZ);
    // Store in row-major Mtx44
    for (int r=0; r<4; r++)
        for (int c=0; c<4; c++)
            m[r][c] = p[c][r];
}

static inline void guOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 nearZ, f32 farZ) {
    glm::mat4 o = glm::ortho(l, r, b, t, nearZ, farZ);
    for (int row=0; row<4; row++)
        for (int col=0; col<4; col++)
            m[row][col] = o[col][row];
}

// ── GX render stubs ───────────────────────────────────────────────────────────

static inline void GX_LoadPosMtxImm(Mtx m, int slot) {
    (void)slot;
    // Build 4x4 from 3x4
    GLfloat gl[16] = {
        m[0][0], m[1][0], m[2][0], 0,
        m[0][1], m[1][1], m[2][1], 0,
        m[0][2], m[1][2], m[2][2], 0,
        m[0][3], m[1][3], m[2][3], 1
    };
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(gl);
}

static inline void GX_LoadProjectionMtx(Mtx44 m, int type) {
    GLfloat gl[16];
    if (type == GX_PERSPECTIVE) {
        // Column-major for GL
        gl[0]=m[0][0]; gl[4]=m[0][1]; gl[8]=m[0][2];  gl[12]=m[0][3];
        gl[1]=m[1][0]; gl[5]=m[1][1]; gl[9]=m[1][2];  gl[13]=m[1][3];
        gl[2]=m[2][0]; gl[6]=m[2][1]; gl[10]=m[2][2]; gl[14]=m[2][3];
        gl[3]=m[3][0]; gl[7]=m[3][1]; gl[11]=m[3][2]; gl[15]=m[3][3];
    } else {
        gl[0]=m[0][0]; gl[4]=m[0][1]; gl[8]=m[0][2];  gl[12]=m[0][3];
        gl[1]=m[1][0]; gl[5]=m[1][1]; gl[9]=m[1][2];  gl[13]=m[1][3];
        gl[2]=m[2][0]; gl[6]=m[2][1]; gl[10]=m[2][2]; gl[14]=m[2][3];
        gl[3]=0;        gl[7]=0;        gl[11]=0;        gl[15]=1;
    }
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(gl);
    glMatrixMode(GL_MODELVIEW);
}

static inline void GX_SetViewport(f32 x, f32 y, f32 w, f32 h, f32 n, f32 f2) {
    glViewport((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
    (void)n; (void)f2;
}

static inline void GX_SetZMode(int en, int func, int update) {
    if (en) { glEnable(GL_DEPTH_TEST); glDepthFunc(func); }
    else    glDisable(GL_DEPTH_TEST);
    glDepthMask(update ? GL_TRUE : GL_FALSE);
}

static inline void GX_SetCullMode(int mode) {
    g_pc_cull_mode = mode;
    if (mode == GX_CULL_NONE)  { glDisable(GL_CULL_FACE); return; }
    glEnable(GL_CULL_FACE);
    glCullFace(mode == GX_CULL_BACK ? GL_BACK : GL_FRONT);
}

static inline void GX_SetBlendMode(int bm, int src, int dst, int op) {
    (void)op;
    if (bm == GX_BM_NONE) { glDisable(GL_BLEND); return; }
    glEnable(GL_BLEND);
    glBlendFunc(src, dst);
}

static inline void GX_SetTevOp(int stage, int op) {
    (void)stage;
    g_pc_tev_mode = op;
    if (g_uni_tev_mode >= 0)
        glUniform1i(g_uni_tev_mode, (op == GX_PASSCLR) ? 0 : 1);
}

static inline void GX_SetTevOrder(int stage, int tc, int tm, int cc) {
    (void)stage; (void)tc; (void)cc;
    // Shader handles texturing — just set tev_mode based on whether texture is used
    if (tm == GX_TEXMAP_NULL) {
        if (g_uni_tev_mode >= 0) glUniform1i(g_uni_tev_mode, 0); // PASSCLR
    }
    // If TEXMAP0, tev_mode stays as set by GX_SetTevOp
}

static inline void GX_SetFog(int type, f32 start, f32 end, f32 nearZ, f32 farZ, GXColor col) {
    (void)nearZ; (void)farZ;
    if (g_uni_fog_enabled < 0) return;
    if (type == GX_FOG_NONE) {
        glUniform1i(g_uni_fog_enabled, 0);
        return;
    }
    glUniform1i(g_uni_fog_enabled, 1);
    glUniform1f(g_uni_fog_start, start);
    glUniform1f(g_uni_fog_end, end);
    glUniform4f(g_uni_fog_color,
        col.r/255.0f, col.g/255.0f, col.b/255.0f, col.a/255.0f);
}

static inline void GX_SetFogRangeAdj(int en, int ctr, void* rng) {
    (void)en; (void)ctr; (void)rng;
}

static inline void GX_SetNumChans(int n) { (void)n; }
static inline void GX_ClearVtxDesc(void) {}
static inline void GX_SetVtxDesc(int attr, int type) { (void)attr; (void)type; }
static inline void GX_SetVtxAttrFmt(int fmt, int attr, int cnt, int type, int frac) {
    (void)fmt; (void)attr; (void)cnt; (void)type; (void)frac;
}

static inline void GX_SetCopyClear(GXColor col, u32 depth) {
    (void)depth;
    glClearColor(col.r/255.0f, col.g/255.0f, col.b/255.0f, 1.0f);
}

// ── Immediate mode drawing ────────────────────────────────────────────────────
static inline void GX_Begin(int prim, int fmt, int vcount) {
    (void)fmt; (void)vcount;
    if (prim == GL_LINES) glLineWidth(1.5f);
    glBegin(prim);
}
static inline void GX_End(void) { glEnd(); }
static inline void GX_Position3f32(f32 x, f32 y, f32 z) { glVertex3f(x, y, z); }
static inline void GX_Color4u8(u8 r, u8 g, u8 b, u8 a) {
    glColor4ub(r, g, b, a);
}
static inline void GX_TexCoord2f32(f32 s, f32 t) { glTexCoord2f(s, t); }

static inline void GX_DrawDone(void) { glFlush(); }
static inline void GX_Flush(void) {}
static inline void GX_CopyDisp(void* fb, int clear) {
    (void)fb;
    SDL_GL_SwapWindow(g_pc_window);
    if (clear) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

static inline void* GX_Init(void* fifo, u32 size) { (void)fifo; (void)size; return NULL; }

// ── Texture stubs ─────────────────────────────────────────────────────────────
static inline void GX_InitTexObj(GXTexObj* obj, void* data, u16 w, u16 h,
                                  int fmt, int wrapS, int wrapT, int mipmap) {
    (void)fmt; (void)mipmap;
    glGenTextures(1, &obj->id);
    obj->w = w; obj->h = h;
    glBindTexture(GL_TEXTURE_2D, obj->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
}
static inline void GX_InitTexObjFilterMode(GXTexObj* obj, int min, int mag) {
    glBindTexture(GL_TEXTURE_2D, obj->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
}
static inline void GX_LoadTexObj(GXTexObj* obj, int mapid) {
    (void)mapid;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, obj->id);
    glEnable(GL_TEXTURE_2D);
    if (g_uni_tex >= 0) glUniform1i(g_uni_tex, 0);
    if (g_uni_tev_mode >= 0) glUniform1i(g_uni_tev_mode, 1); // MODULATE
}

// ── VIDEO stubs ───────────────────────────────────────────────────────────────
static inline void VIDEO_Init(void) {}
static inline void VIDEO_Configure(GXRModeObj* r) { (void)r; }
static inline void VIDEO_SetNextFramebuffer(void* fb) { (void)fb; }
static inline void VIDEO_SetBlack(int b) { (void)b; }
static inline void VIDEO_Flush(void) {}
static inline void VIDEO_WaitVSync(void) { SDL_Delay(16); } // ~60fps
static inline GXRModeObj* VIDEO_GetPreferredMode(void* p) { (void)p; return &g_pc_rmode; }
static inline void* SYS_AllocateFramebuffer(GXRModeObj* r) { (void)r; return NULL; }
static inline void* MEM_K0_TO_K1(void* p) { return p; }

// ── Timing stubs ──────────────────────────────────────────────────────────────
static inline u32 gettime(void) { return SDL_GetTicks(); }
static inline u32 ticks_to_secs(u32 t) { return t / 1000; }

// ── PAD stubs — implemented in pc_input.cpp ───────────────────────────────────
// Button bitmasks matching GC layout
#define PAD_BUTTON_A      0x0100
#define PAD_BUTTON_B      0x0200
#define PAD_BUTTON_X      0x0400
#define PAD_BUTTON_Y      0x0800
#define PAD_BUTTON_START  0x1000
#define PAD_BUTTON_UP     0x0008
#define PAD_BUTTON_DOWN   0x0004
#define PAD_BUTTON_LEFT   0x0002
#define PAD_BUTTON_RIGHT  0x0001
#define PAD_TRIGGER_L     0x0040
#define PAD_TRIGGER_R     0x0020
#define PAD_TRIGGER_Z     0x0010
#define HOTBAR_SCROLL_UP  0x2000  // mouse wheel up  (custom)
#define HOTBAR_SCROLL_DN  0x4000  // mouse wheel down (custom)

extern u32 g_pc_buttons_held;
extern u32 g_pc_buttons_down;
extern s8  g_pc_stick_x;
extern s8  g_pc_stick_y;
extern s8  g_pc_cstick_x;
extern s8  g_pc_cstick_y;
extern u8  g_pc_trigger_l;
extern u8  g_pc_trigger_r;

static inline void PAD_Init(void) {}
static inline void PAD_ScanPads(void) {} // called by Platform_PollInput
static inline u32  PAD_ButtonsDown(int pad) { (void)pad; return g_pc_buttons_down; }
static inline u32  PAD_ButtonsHeld(int pad) { (void)pad; return g_pc_buttons_held; }
static inline s8   PAD_StickX(int pad)      { (void)pad; return g_pc_stick_x; }
static inline s8   PAD_StickY(int pad)      { (void)pad; return g_pc_stick_y; }
static inline s8   PAD_SubStickX(int pad)   { (void)pad; return g_pc_cstick_x; }
static inline s8   PAD_SubStickY(int pad)   { (void)pad; return g_pc_cstick_y; }
static inline u8   PAD_TriggerL(int pad)    { (void)pad; return g_pc_trigger_l; }
static inline u8   PAD_TriggerR(int pad)    { (void)pad; return g_pc_trigger_r; }

// ── Memory stubs ──────────────────────────────────────────────────────────────
#ifndef memalign
  #include <stdlib.h>
  static inline void* memalign(size_t align, size_t size) {
    #ifdef _WIN32
      return _aligned_malloc(size, align);
    #else
      void* p; posix_memalign(&p, align, size); return p;
    #endif
  }
#endif

// GC cache flush — no-op on PC
static inline void DCFlushRange(void* p, u32 size) { (void)p; (void)size; }

// malloc.h not needed on PC
#define MALLOC_H_INCLUDED

// ── PNG loading — use stb_image on PC ────────────────────────────────────────
// textures.cpp uses libpng on GC. On PC we use stb_image instead.
// This is handled by #ifdef _PC in textures.cpp.