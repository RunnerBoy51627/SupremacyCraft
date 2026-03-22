#include "gc_compat.h"
#include <stdio.h>

// ── Global PC state ───────────────────────────────────────────────────────────
// Shader globals (declared in gc_compat.h, defined here)
GLuint g_shader_prog  = 0;
GLint  g_uni_tev_mode    = -1;
GLint  g_uni_fog_color   = -1;
GLint  g_uni_fog_enabled = -1;
GLint  g_uni_fog_start   = -1;
GLint  g_uni_fog_end     = -1;
GLint  g_uni_tex         = -1;

SDL_Window*   g_pc_window  = NULL;
SDL_GLContext g_pc_glctx   = NULL;
GXRModeObj    g_pc_rmode   = {640, 480, 640, 480};
int           g_pc_tev_mode = 0;
int           g_pc_cull_mode = 1;

void Platform_InitWindow(const char* title, int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    g_pc_window = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!g_pc_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        exit(1);
    }

    g_pc_glctx = SDL_GL_CreateContext(g_pc_window);
    if (!g_pc_glctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_GL_SetSwapInterval(1); // vsync

    g_pc_rmode.w = w;
    g_pc_rmode.h = h;
    g_pc_rmode.fbWidth   = w;
    g_pc_rmode.efbHeight = h;

    // Base GL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_SMOOTH);
    glClearColor(0x87/255.0f, 0xCE/255.0f, 0xEB/255.0f, 1.0f); // sky blue
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Init GLEW to get GL 2.0+ function pointers
    GLenum err = glewInit();
    if (err != GLEW_OK)
        fprintf(stderr, "GLEW: %s\n", glewGetErrorString(err));

    // Load GLSL shaders — try several paths to find them
    const char* vert_paths[] = {
        "shaders/vertex.glsl",
        "../shaders/vertex.glsl",
        "../../shaders/vertex.glsl",
        NULL
    };
    const char* frag_paths[] = {
        "shaders/fragment.glsl",
        "../shaders/fragment.glsl",
        "../../shaders/fragment.glsl",
        NULL
    };
    for (int i = 0; vert_paths[i]; i++) {
        FILE* f = fopen(vert_paths[i], "r");
        if (f) {
            fclose(f);
            PC_Shader_Init(vert_paths[i], frag_paths[i]);
            break;
        }
    }
}

void Platform_DestroyWindow(void) {
    if (g_pc_glctx) SDL_GL_DeleteContext(g_pc_glctx);
    if (g_pc_window) SDL_DestroyWindow(g_pc_window);
    SDL_Quit();
}