#include "gfx.h"

#include <stdlib.h>
#include <string.h>

// Assumptions / notes:
// - Caller has already created a valid OpenGL context and loaded needed
// function
//   pointers (if platform requires). We only use core GL 2.0 era functions.
// - No attempt is made to restore previous GL state (kept minimal). If your
//   engine relies on specific state, set it again after calling gfx_render.

#if defined(_WIN32)
#    include <windows.h>
#endif
#if defined(__APPLE__)
#    include <OpenGL/gl.h>
#else
#    include <GL/gl.h>
#endif

// ---------- Internal Structures ----------

struct GfxLayer {
    int    w, h;
    GLuint tex;
    bool   enabled;
};

// Single shared shader & geometry
static GLuint g_program  = 0;
static GLint  g_attr_pos = -1;
static GLint  g_attr_uv  = -1;
static GLint  g_unif_tex = -1;
static GLuint g_vao      = 0;
static GLuint g_vbo      = 0;
static bool   g_inited   = false;

// ---------- Minimal Shader Setup ----------

static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
#ifdef _DEBUG
        char    log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog(sh, (GLsizei)sizeof(log), &len, log);
#endif
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint create_program(void)
{
    const char* vs_src =
        "#version 120\n"
        "attribute vec2 aPos;"
        "attribute vec2 aUV;"
        "varying vec2 vUV;"
        "void main(){ vUV = aUV; gl_Position = vec4(aPos,0.0,1.0); }";

    const char* fs_src = "#version 120\n"
                         "uniform sampler2D uTex;"
                         "varying vec2 vUV;"
                         "void main(){ gl_FragColor = texture2D(uTex, vUV); }";

    GLuint vs          = compile_shader(GL_VERTEX_SHADER, vs_src);
    if (!vs) {
        return 0;
    }
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ---------- Public API ----------

bool gfx_init(void)
{
    if (g_inited) {
        return true;
    }

    g_program = create_program();
    if (!g_program) {
        return false;
    }

    g_attr_pos = glGetAttribLocation(g_program, "aPos");
    g_attr_uv  = glGetAttribLocation(g_program, "aUV");
    g_unif_tex = glGetUniformLocation(g_program, "uTex");

#if defined(GL_VERTEX_ARRAY_BINDING) || defined(GL_ARRAY_BUFFER)
    if (glGenVertexArrays) {
        glGenVertexArrays(1, &g_vao);
        glBindVertexArray(g_vao);
    }
#endif
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(
        GL_ARRAY_BUFFER, 4 * (sizeof(float) * 4), NULL, GL_DYNAMIC_DRAW);

    if (g_attr_pos >= 0) {
        glEnableVertexAttribArray((GLuint)g_attr_pos);
        glVertexAttribPointer((GLuint)g_attr_pos,
                              2,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(float) * 4,
                              (const void*)0);
    }
    if (g_attr_uv >= 0) {
        glEnableVertexAttribArray((GLuint)g_attr_uv);
        glVertexAttribPointer((GLuint)g_attr_uv,
                              2,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(float) * 4,
                              (const void*)(sizeof(float) * 2));
    }

    g_inited = true;
    return true;
}

void gfx_shutdown(void)
{
    if (!g_inited) {
        return;
    }
    if (g_vbo) {
        glDeleteBuffers(1, &g_vbo);
        g_vbo = 0;
    }
#if defined(GL_VERTEX_ARRAY_BINDING)
    if (g_vao) {
        glDeleteVertexArrays(1, &g_vao);
        g_vao = 0;
    }
#endif
    if (g_program) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
    g_inited = false;
}

static GLuint create_texture(int w, int h, const uint32_t* pixels)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER,
                    GL_NEAREST); // pixel-perfect scaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return tex;
}

GfxLayer* gfx_layer_create(int width, int height, const uint32_t* rgba_pixels)
{
    if (width <= 0 || height <= 0) {
        return NULL;
    }
    GfxLayer* L = (GfxLayer*)calloc(1, sizeof(GfxLayer));
    if (!L) {
        return NULL;
    }
    L->w       = width;
    L->h       = height;
    L->enabled = true;
    L->tex     = create_texture(width, height, rgba_pixels);
    return L;
}

void gfx_layer_destroy(GfxLayer* layer)
{
    if (!layer) {
        return;
    }
    if (layer->tex) {
        glDeleteTextures(1, &layer->tex);
    }
    free(layer);
}

void gfx_layer_set_enabled(GfxLayer* layer, bool enabled)
{
    if (layer) {
        layer->enabled = enabled;
    }
}

bool gfx_layer_is_enabled(const GfxLayer* layer)
{
    return layer ? layer->enabled : false;
}

void gfx_layer_update_pixels(GfxLayer* layer, const uint32_t* rgba_pixels)
{
    if (!layer || !rgba_pixels) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, layer->tex);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    layer->w,
                    layer->h,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    rgba_pixels);
}

bool gfx_layer_resize(GfxLayer*       layer,
                      int             new_w,
                      int             new_h,
                      const uint32_t* rgba_pixels)
{
    if (!layer || new_w <= 0 || new_h <= 0) {
        return false;
    }
    if (layer->tex) {
        glDeleteTextures(1, &layer->tex);
    }
    layer->w   = new_w;
    layer->h   = new_h;
    layer->tex = create_texture(new_w, new_h, rgba_pixels);
    return layer->tex != 0;
}

int gfx_layer_get_width(const GfxLayer* layer) { return layer ? layer->w : 0; }
int gfx_layer_get_height(const GfxLayer* layer) { return layer ? layer->h : 0; }

// ---------- Rendering ----------

void gfx_render(GfxLayer** layers,
                int        layer_count,
                int        window_width,
                int        window_height)
{
    if (!g_inited || window_width <= 0 || window_height <= 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_program);
    if (g_unif_tex >= 0) {
        glUniform1i(g_unif_tex, 0);
    }

#if defined(GL_VERTEX_ARRAY_BINDING)
    if (g_vao) {
        glBindVertexArray(g_vao);
    }
#endif
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

    for (int i = 0; i < layer_count; ++i) {
        GfxLayer* L = layers[i];
        if (!L || !L->enabled) {
            continue;
        }

        float scale_w   = (float)window_width / (float)L->w;
        float scale_h   = (float)window_height / (float)L->h;
        float scale     = scale_w < scale_h ? scale_w : scale_h;

        float sw        = (float)L->w * scale;
        float sh        = (float)L->h * scale;

        float ndc_w     = (sw * 2.0f) / (float)window_width;
        float ndc_h     = (sh * 2.0f) / (float)window_height;

        float hw        = ndc_w * 0.5f;
        float hh        = ndc_h * 0.5f;

        // Interleaved pos(x,y), uv(u,v) for triangle strip (4 verts)
        float verts[16] = {-hw,
                           -hh,
                           0.0f,
                           0.0f,
                           hw,
                           -hh,
                           1.0f,
                           0.0f,
                           -hw,
                           hh,
                           0.0f,
                           1.0f,
                           hw,
                           hh,
                           1.0f,
                           1.0f};

        glBindTexture(GL_TEXTURE_2D, L->tex);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

        if (g_attr_pos >= 0) {
            glVertexAttribPointer((GLuint)g_attr_pos,
                                  2,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(float) * 4,
                                  (const void*)0);
        }
        if (g_attr_uv >= 0) {
            glVertexAttribPointer((GLuint)g_attr_uv,
                                  2,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(float) * 4,
                                  (const void*)(sizeof(float) * 2));
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0,
