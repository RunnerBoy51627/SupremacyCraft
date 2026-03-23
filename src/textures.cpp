#define ATLAS_REGIONS_IMPL
#include "textures.h"
#include "atlas_regions.h"
#include "platform_types.h"
#ifndef _PC
#include <gccore.h>
#include <string.h>
#include <malloc.h>
#include <png.h>
#else
#include "platform/pc/gc_compat.h"
#include <string.h>
#include <stdlib.h>
#include <png.h>
#endif

#include "atlas_packs.h"

extern const u8  atlas_png_default[];
extern const u32 atlas_png_default_size;

static GXTexObj atlasObj;
static int s_atlasW = 64;
static int s_atlasH = 64;

// Multi-pack support
static GXTexObj s_extraObjs[PACK_COUNT];
static int      s_extraW[PACK_COUNT];
static int      s_extraH[PACK_COUNT];
static int      s_extraInited[PACK_COUNT];
static int      s_currentPack = 0;

static TexRegion s_cache[TEX_COUNT];
static int s_cache_built = 0;
static void tex_reset_cache() { s_cache_built = 0; }

typedef struct { const u8* data; u32 pos; u32 size; } PngReader;
static void png_read_mem(png_structp png, png_bytep out, png_size_t count) {
    PngReader* r = (PngReader*)png_get_io_ptr(png);
    if (r->pos + count > r->size) count = r->size - r->pos;
    memcpy(out, r->data + r->pos, count);
    r->pos += count;
}

// Identical to original working Tex_Init, just uses atlas_png_default
void Tex_Init() {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info  = png_create_info_struct(png);
    PngReader reader = { atlas_png_default, 0, atlas_png_default_size };
    png_set_read_fn(png, &reader, png_read_mem);
    png_read_info(png, info);

    int w = png_get_image_width(png, info);
    int h = png_get_image_height(png, info);
    s_atlasW = w;
    s_atlasH = h;
    png_set_strip_16(png);
    png_set_expand(png);
    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_RGB)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(png, info);

    u8* rgba = (u8*)malloc(w * h * 4);
    png_bytep* rows = (png_bytep*)malloc(h * sizeof(png_bytep));
    for (int y = 0; y < h; y++) rows[y] = rgba + y * w * 4;
    png_read_image(png, rows);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);

#ifdef _PC
    GX_InitTexObj(&atlasObj, rgba, w, h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    free(rgba);
#else
    u32 gxSize = w * h * 4;
    u8* gxBuf  = (u8*)memalign(32, gxSize);
    int tileW=(w+3)/4, tileH=(h+3)/4;
    u8* dst = gxBuf;
    for (int ty=0;ty<tileH;ty++) for (int tx=0;tx<tileW;tx++) {
        for (int py=0;py<4;py++) for (int px=0;px<4;px++) {
            int sx=tx*4+px, sy=ty*4+py;
            u8* p=(sx<w&&sy<h)?rgba+(sy*w+sx)*4:(u8*)"\xFF\0\0\xFF";
            *dst++=p[3]; *dst++=p[0];
        }
        for (int py=0;py<4;py++) for (int px=0;px<4;px++) {
            int sx=tx*4+px, sy=ty*4+py;
            u8* p=(sx<w&&sy<h)?rgba+(sy*w+sx)*4:(u8*)"\xFF\0\0\xFF";
            *dst++=p[1]; *dst++=p[2];
        }
    }
    free(rgba);
    DCFlushRange(gxBuf, gxSize);
    GX_InitTexObj(&atlasObj, gxBuf, w, h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
#endif
    GX_InitTexObjFilterMode(&atlasObj, GX_NEAR, GX_NEAR);
}

void Tex_BindAtlas() {
    if (s_currentPack == 0 || !s_extraInited[s_currentPack]) {
        GX_LoadTexObj(&atlasObj, GX_TEXMAP0);
    } else {
        GX_LoadTexObj(&s_extraObjs[s_currentPack], GX_TEXMAP0);
    }
}

void Tex_Switch(int packIndex) {
    if (packIndex < 0 || packIndex >= PACK_COUNT) return;
    if (packIndex == 0) {
        s_currentPack = 0;
        s_atlasW = 64; s_atlasH = 64;
        tex_reset_cache();
        return;
    }
    // Load extra pack on demand using same pattern as Tex_Init
    if (!s_extraInited[packIndex]) {
        const u8* data = g_pack_atlases[packIndex];
        u32 size = g_pack_sizes[packIndex];
        if (!data || size < 8) return;
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        png_infop info  = png_create_info_struct(png);
        PngReader reader = { data, 0, size };
        png_set_read_fn(png, &reader, png_read_mem);
        png_read_info(png, info);
        int w = png_get_image_width(png, info);
        int h = png_get_image_height(png, info);
        s_extraW[packIndex] = w;
        s_extraH[packIndex] = h;
        png_set_strip_16(png); png_set_expand(png);
        if (png_get_color_type(png, info) == PNG_COLOR_TYPE_RGB)
            png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
        png_read_update_info(png, info);
        u8* rgba = (u8*)malloc(w * h * 4);
        png_bytep* rows = (png_bytep*)malloc(h * sizeof(png_bytep));
        for (int y = 0; y < h; y++) rows[y] = rgba + y * w * 4;
        png_read_image(png, rows);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
#ifdef _PC
        GX_InitTexObj(&s_extraObjs[packIndex], rgba, w, h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        free(rgba);
#else
        u32 gxSize = w * h * 4;
        u8* gxBuf  = (u8*)memalign(32, gxSize);
        int tileW=(w+3)/4, tileH=(h+3)/4;
        u8* dst = gxBuf;
        for (int ty=0;ty<tileH;ty++) for (int tx=0;tx<tileW;tx++) {
            for (int py=0;py<4;py++) for (int px=0;px<4;px++) {
                int sx=tx*4+px, sy=ty*4+py;
                u8* p=(sx<w&&sy<h)?rgba+(sy*w+sx)*4:(u8*)"\xFF\0\0\xFF";
                *dst++=p[3]; *dst++=p[0];
            }
            for (int py=0;py<4;py++) for (int px=0;px<4;px++) {
                int sx=tx*4+px, sy=ty*4+py;
                u8* p=(sx<w&&sy<h)?rgba+(sy*w+sx)*4:(u8*)"\xFF\0\0\xFF";
                *dst++=p[1]; *dst++=p[2];
            }
        }
        free(rgba);
        DCFlushRange(gxBuf, gxSize);
        GX_InitTexObj(&s_extraObjs[packIndex], gxBuf, w, h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
#endif
        GX_InitTexObjFilterMode(&s_extraObjs[packIndex], GX_NEAR, GX_NEAR);
        s_extraInited[packIndex] = 1;
    }
    s_currentPack = packIndex;
    s_atlasW = s_extraW[packIndex];
    s_atlasH = s_extraH[packIndex];
    tex_reset_cache();
}

int  Tex_GetPackCount()            { return PACK_COUNT; }
const char* Tex_GetPackName(int i) {
    // Read pack names from static storage in embed script output
    // g_pack_names uses dynamic init which is unreliable on GC
    // so we declare the individual string symbols directly
    if (i < 0 || i >= PACK_COUNT) return "";
    // These are defined as static const char[] in atlas_packs_data.cpp
    // so they have no dynamic initialization
    extern const char* const g_pack_names[];
    // Safety: validate the pointer looks like a real address
    const char* n = g_pack_names[i];
    u32 addr = (u32)(u32*)n;
    if (addr < 0x80000000 || addr > 0x81800000) {
        // Bad pointer — pack names not initialized yet
        static const char def[] = "default";
        static const char tmpl[]= "template";
        static const char test[]= "test";
        const char* safe[3] = {def, tmpl, test};
        return (i < 3) ? safe[i] : "";
    }
    return n;
}
int  Tex_GetCurrentPack()          { return s_currentPack; }

const TexRegion* Tex_GetRegion(int texID) {
    if (!s_cache_built) {
        // Inset by 1.5 texels — prevents atlas UV bleeding on GX hardware
        float insetU = 1.5f / (float)s_atlasW;
        float insetV = 1.5f / (float)s_atlasH;
        for (int i = 0; i < TEX_COUNT; i++) {
            s_cache[i] = {
                atlas_uvs[i][0] + insetU,
                atlas_uvs[i][1] + insetV,
                atlas_uvs[i][2] - insetU,
                atlas_uvs[i][3] - insetV
            };
        }
        s_cache_built = 1;
    }
    return &s_cache[texID];
}