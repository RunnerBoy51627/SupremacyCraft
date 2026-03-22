#define ATLAS_REGIONS_IMPL
#include "textures.h"
#include "atlas_regions.h"
#include <gccore.h>
#include <string.h>
#include <malloc.h>
#include <png.h>

extern const u8  atlas_png[];
extern const u32 atlas_png_size;

static GXTexObj atlasObj;
static int s_atlasW = 64; // default; overwritten by Tex_Init
static int s_atlasH = 32;

typedef struct { const u8* data; u32 pos; u32 size; } PngReader;
static void png_read_mem(png_structp png, png_bytep out, png_size_t count) {
    PngReader* r = (PngReader*)png_get_io_ptr(png);
    if (r->pos + count > r->size) count = r->size - r->pos;
    memcpy(out, r->data + r->pos, count);
    r->pos += count;
}

void Tex_Init() {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info  = png_create_info_struct(png);
    PngReader reader = { atlas_png, 0, atlas_png_size };
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

    GX_InitTexObj(&atlasObj, gxBuf, w, h,
                  GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjFilterMode(&atlasObj, GX_NEAR, GX_NEAR);
}

void Tex_BindAtlas() {
    GX_LoadTexObj(&atlasObj, GX_TEXMAP0);
}

const TexRegion* Tex_GetRegion(int texID) {
    static TexRegion cache[TEX_COUNT];
    static int built = 0;
    if (!built) {
        // Inset each UV region by half a texel to prevent neighbouring
        // tile bleed when the GPU samples near atlas tile boundaries.
        // With nearest-neighbour filtering, even a tiny FP rounding error
        // at a UV edge can fetch the adjacent tile's outermost texel.
        // Pulling each edge inward by 0.5/atlasSize guarantees the sample
        // always lands inside the correct tile.
        float insetU = 0.5f / (float)s_atlasW;
        float insetV = 0.5f / (float)s_atlasH;
        for (int i = 0; i < TEX_COUNT; i++) {
            cache[i] = {
                atlas_uvs[i][0] + insetU,
                atlas_uvs[i][1] + insetV,
                atlas_uvs[i][2] - insetU,
                atlas_uvs[i][3] - insetV
            };
        }
        built = 1;
    }
    return &cache[texID];
}