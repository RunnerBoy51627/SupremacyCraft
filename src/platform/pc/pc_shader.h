#pragma once
// pc_shader.h — loads, compiles and manages the GLSL shader program

#ifdef __APPLE__
  #include <OpenGL/gl.h>
#else
  #include <GL/gl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Global shader state ───────────────────────────────────────────────────────
extern GLuint g_shader_prog;
extern GLint  g_uni_tev_mode;
extern GLint  g_uni_fog_color;
extern GLint  g_uni_fog_enabled;
extern GLint  g_uni_fog_start;
extern GLint  g_uni_fog_end;
extern GLint  g_uni_tex;

// ── Load and compile shaders ──────────────────────────────────────────────────
static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open shader: %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char* buf = (char*)malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);
    return buf;
}

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
    }
    return s;
}

static inline void PC_Shader_Init(const char* vert_path, const char* frag_path) {
    char* vsrc = read_file(vert_path);
    char* fsrc = read_file(frag_path);
    if (!vsrc || !fsrc) {
        fprintf(stderr, "Failed to load shaders — using fixed function\n");
        free(vsrc); free(fsrc);
        return;
    }

    GLuint vert = compile_shader(GL_VERTEX_SHADER,   vsrc);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, fsrc);
    free(vsrc); free(fsrc);

    g_shader_prog = glCreateProgram();
    glAttachShader(g_shader_prog, vert);
    glAttachShader(g_shader_prog, frag);
    glLinkProgram(g_shader_prog);

    GLint ok; glGetProgramiv(g_shader_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(g_shader_prog, 512, NULL, log);
        fprintf(stderr, "Shader link error:\n%s\n", log);
        g_shader_prog = 0;
        return;
    }
    glDeleteShader(vert);
    glDeleteShader(frag);

    glUseProgram(g_shader_prog);

    // Cache uniform locations
    g_uni_tev_mode    = glGetUniformLocation(g_shader_prog, "tev_mode");
    g_uni_fog_color   = glGetUniformLocation(g_shader_prog, "fog_color");
    g_uni_fog_enabled = glGetUniformLocation(g_shader_prog, "fog_enabled");
    g_uni_fog_start   = glGetUniformLocation(g_shader_prog, "fog_start");
    g_uni_fog_end     = glGetUniformLocation(g_shader_prog, "fog_end");
    g_uni_tex         = glGetUniformLocation(g_shader_prog, "tex");

    // Defaults
    glUniform1i(g_uni_tex,         0);
    glUniform1i(g_uni_tev_mode,    1); // MODULATE
    glUniform1i(g_uni_fog_enabled, 0);
    glUniform4f(g_uni_fog_color,   0x87/255.0f, 0xCE/255.0f, 0xEB/255.0f, 1.0f);
    glUniform1f(g_uni_fog_start,   20.0f);
    glUniform1f(g_uni_fog_end,     80.0f);

    fprintf(stderr, "Shaders loaded OK\n");
}