#include "gfx.h"

// Assumptions / notes:
// - gfx_init will (on Windows if OS_WINDOWS defined) optionally create a
// minimal
//   hidden OpenGL context if none is current, and load only the GL functions
//   required by this module. If a context is already current, it will be used.
// - No attempt is made to restore previous GL state (kept minimal). If your
//   engine relies on specific state, set it again after calling gfx_render.

#if KORE_OS_MACOS
#    include <OpenGL/gl.h>
#else
#    include <GL/gl.h>
#endif

#include <stdlib.h>

#define APIENTRYP APIENTRY*

// ---------- Internal Structures ----------

struct GfxLayer {
    int       w, h;
    GLuint    tex;
    bool      enabled;
    uint32_t* pixels; // Reference to external pixel buffer
};

// Single shared shader & geometry
static GLuint g_program  = 0;
static GLint  g_attr_pos = -1;
static GLint  g_attr_uv  = -1;
static GLint  g_unif_tex = -1;
static GLuint g_vao      = 0;
static GLuint g_vbo      = 0;
static bool   g_inited   = false;

#if KORE_OS_WINDOWS
// WGL context we may create (only if none exists on init)
static HWND    g_hwnd         = NULL;
static HDC     g_hdc          = NULL;
static HGLRC   g_hglrc        = NULL;
static bool    g_owns_context = false;
static HMODULE g_gl_lib       = NULL;

// Function pointer declarations for required modern GL (>=2.0) functions
// (Core 1.1 functions are provided by opengl32.dll directly.)
typedef char      GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef GLuint(APIENTRYP PFNGLCREATESHADERPROC)(GLenum);
typedef void(APIENTRYP PFNGLSHADERSOURCEPROC)(GLuint,
                                              GLsizei,
                                              const GLchar* const*,
                                              const GLint*);
typedef void(APIENTRYP PFNGLCOMPILESHADERPROC)(GLuint);
typedef void(APIENTRYP PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef void(APIENTRYP PFNGLGETSHADERINFOLOGPROC)(GLuint,
                                                  GLsizei,
                                                  GLsizei*,
                                                  GLchar*);
typedef void(APIENTRYP PFNGLDELETESHADERPROC)(GLuint);
typedef GLuint(APIENTRYP PFNGLCREATEPROGRAMPROC)(void);
typedef void(APIENTRYP PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void(APIENTRYP PFNGLLINKPROGRAMPROC)(GLuint);
typedef void(APIENTRYP PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
typedef void(APIENTRYP PFNGLDELETEPROGRAMPROC)(GLuint);
typedef GLint(APIENTRYP PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);
typedef void(APIENTRYP PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef GLint(APIENTRYP PFNGLGETATTRIBLOCATIONPROC)(GLuint, const GLchar*);
typedef void(APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void(APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC)(
    GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void(APIENTRYP PFNGLUSEPROGRAMPROC)(GLuint);
typedef void(APIENTRYP PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void(APIENTRYP PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void(APIENTRYP PFNGLBUFFERDATAPROC)(GLenum,
                                            GLsizeiptr,
                                            const void*,
                                            GLenum);
typedef void(APIENTRYP PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void(APIENTRYP PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void(APIENTRYP PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void(APIENTRYP PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);

// Pointers
static PFNGLCREATESHADERPROC            p_glCreateShader            = NULL;
static PFNGLSHADERSOURCEPROC            p_glShaderSource            = NULL;
static PFNGLCOMPILESHADERPROC           p_glCompileShader           = NULL;
static PFNGLGETSHADERIVPROC             p_glGetShaderiv             = NULL;
static PFNGLGETSHADERINFOLOGPROC        p_glGetShaderInfoLog        = NULL;
static PFNGLDELETESHADERPROC            p_glDeleteShader            = NULL;
static PFNGLCREATEPROGRAMPROC           p_glCreateProgram           = NULL;
static PFNGLATTACHSHADERPROC            p_glAttachShader            = NULL;
static PFNGLLINKPROGRAMPROC             p_glLinkProgram             = NULL;
static PFNGLGETPROGRAMIVPROC            p_glGetProgramiv            = NULL;
static PFNGLDELETEPROGRAMPROC           p_glDeleteProgram           = NULL;
static PFNGLGETUNIFORMLOCATIONPROC      p_glGetUniformLocation      = NULL;
static PFNGLUNIFORM1IPROC               p_glUniform1i               = NULL;
static PFNGLGETATTRIBLOCATIONPROC       p_glGetAttribLocation       = NULL;
static PFNGLENABLEVERTEXATTRIBARRAYPROC p_glEnableVertexAttribArray = NULL;
static PFNGLVERTEXATTRIBPOINTERPROC     p_glVertexAttribPointer     = NULL;
static PFNGLUSEPROGRAMPROC              p_glUseProgram              = NULL;
static PFNGLGENBUFFERSPROC              p_glGenBuffers              = NULL;
static PFNGLBINDBUFFERPROC              p_glBindBuffer              = NULL;
static PFNGLBUFFERDATAPROC              p_glBufferData              = NULL;
static PFNGLDELETEBUFFERSPROC           p_glDeleteBuffers           = NULL;
static PFNGLGENVERTEXARRAYSPROC         p_glGenVertexArrays         = NULL;
static PFNGLBINDVERTEXARRAYPROC         p_glBindVertexArray         = NULL;
static PFNGLDELETEVERTEXARRAYSPROC      p_glDeleteVertexArrays      = NULL;

// Macro to redirect calls in rest of file to loaded pointers
#    define glCreateShader p_glCreateShader
#    define glShaderSource p_glShaderSource
#    define glCompileShader p_glCompileShader
#    define glGetShaderiv p_glGetShaderiv
#    define glGetShaderInfoLog p_glGetShaderInfoLog
#    define glDeleteShader p_glDeleteShader
#    define glCreateProgram p_glCreateProgram
#    define glAttachShader p_glAttachShader
#    define glLinkProgram p_glLinkProgram
#    define glGetProgramiv p_glGetProgramiv
#    define glDeleteProgram p_glDeleteProgram
#    define glGetUniformLocation p_glGetUniformLocation
#    define glUniform1i p_glUniform1i
#    define glGetAttribLocation p_glGetAttribLocation
#    define glEnableVertexAttribArray p_glEnableVertexAttribArray
#    define glVertexAttribPointer p_glVertexAttribPointer
#    define glUseProgram p_glUseProgram
#    define glGenBuffers p_glGenBuffers
#    define glBindBuffer p_glBindBuffer
#    define glBufferData p_glBufferData
#    define glDeleteBuffers p_glDeleteBuffers
#    define glGenVertexArrays p_glGenVertexArrays
#    define glBindVertexArray p_glBindVertexArray
#    define glDeleteVertexArrays p_glDeleteVertexArrays

#    ifndef GL_COMPILE_STATUS
#        define GL_COMPILE_STATUS 0x8B81
#    endif
#    ifndef GL_LINK_STATUS
#        define GL_LINK_STATUS 0x8B82
#    endif
#    ifndef GL_INFO_LOG_LENGTH
#        define GL_INFO_LOG_LENGTH 0x8B84
#    endif
#    ifndef GL_VERTEX_SHADER
#        define GL_VERTEX_SHADER 0x8B31
#    endif
#    ifndef GL_FRAGMENT_SHADER
#        define GL_FRAGMENT_SHADER 0x8B30
#    endif
#    ifndef GL_ARRAY_BUFFER
#        define GL_ARRAY_BUFFER 0x8892
#    endif
#    ifndef GL_DYNAMIC_DRAW
#        define GL_DYNAMIC_DRAW 0x88E8
#    endif
#    ifndef GL_CLAMP_TO_EDGE
#        define GL_CLAMP_TO_EDGE 0x812F
#    endif

static void* gfx_wgl_get_proc(const char* name)
{
    void* p = (void*)wglGetProcAddress(name);
    if (!p && g_gl_lib) {
        p = (void*)GetProcAddress(g_gl_lib, name);
    }
    return p;
}

static bool gfx_load_gl_functions(void)
{
    // Load only what we actually use
#    define LOAD_GL(fn)                                                        \
        do {                                                                   \
            p_##fn = (void*)gfx_wgl_get_proc(#fn);                             \
            if (!p_##fn)                                                       \
                return false;                                                  \
        } while (0)
    LOAD_GL(glCreateShader);
    LOAD_GL(glShaderSource);
    LOAD_GL(glCompileShader);
    LOAD_GL(glGetShaderiv);
    LOAD_GL(glGetShaderInfoLog);
    LOAD_GL(glDeleteShader);
    LOAD_GL(glCreateProgram);
    LOAD_GL(glAttachShader);
    LOAD_GL(glLinkProgram);
    LOAD_GL(glGetProgramiv);
    LOAD_GL(glDeleteProgram);
    LOAD_GL(glGetUniformLocation);
    LOAD_GL(glUniform1i);
    LOAD_GL(glGetAttribLocation);
    LOAD_GL(glEnableVertexAttribArray);
    LOAD_GL(glVertexAttribPointer);
    LOAD_GL(glUseProgram);
    LOAD_GL(glGenBuffers);
    LOAD_GL(glBindBuffer);
    LOAD_GL(glBufferData);
    LOAD_GL(glDeleteBuffers);
    // VAO functions are optional
    p_glGenVertexArrays    = gfx_wgl_get_proc("glGenVertexArrays");
    p_glBindVertexArray    = gfx_wgl_get_proc("glBindVertexArray");
    p_glDeleteVertexArrays = gfx_wgl_get_proc("glDeleteVertexArrays");
#    undef LOAD_GL
    return true;
}

static bool gfx_create_dummy_context(void)
{
    if (wglGetCurrentContext()) {
        return true; // Already have context; do not own it
    }

    g_gl_lib = LoadLibraryA("opengl32.dll");
    if (!g_gl_lib) {
        return false;
    }

    WNDCLASSA wc     = {0};
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = GetModuleHandleA(NULL);
    wc.lpszClassName = "GfxDummyGL";
    RegisterClassA(&wc);

    g_hwnd = CreateWindowA("GfxDummyGL",
                           "gfx hidden",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT,
                           CW_USEDEFAULT,
                           32,
                           32,
                           NULL,
                           NULL,
                           wc.hInstance,
                           NULL);
    if (!g_hwnd) {
        return false;
    }

    g_hdc = GetDC(g_hwnd);
    if (!g_hdc) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        (WORD)(PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER),
        PFD_TYPE_RGBA,
        32,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0,
        0,
        0};
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf || !SetPixelFormat(g_hdc, pf, &pfd)) {
        ReleaseDC(g_hwnd, g_hdc);
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
        g_hdc  = NULL;
        return false;
    }

    g_hglrc = wglCreateContext(g_hdc);
    if (!g_hglrc || !wglMakeCurrent(g_hdc, g_hglrc)) {
        if (g_hglrc) {
            wglDeleteContext(g_hglrc);
        }
        ReleaseDC(g_hwnd, g_hdc);
        DestroyWindow(g_hwnd);
        g_hwnd  = NULL;
        g_hdc   = NULL;
        g_hglrc = NULL;
        return false;
    }

    g_owns_context = true;
    return true;
}

static void gfx_destroy_dummy_context(void)
{
    if (!g_owns_context) {
        return;
    }
    wglMakeCurrent(NULL, NULL);
    if (g_hglrc) {
        wglDeleteContext(g_hglrc);
        g_hglrc = NULL;
    }
    if (g_hdc && g_hwnd) {
        ReleaseDC(g_hwnd, g_hdc);
    }
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
    }
    g_hwnd         = NULL;
    g_hdc          = NULL;
    g_owns_context = false;
    if (g_gl_lib) {
        FreeLibrary(g_gl_lib);
        g_gl_lib = NULL;
    }
}
#endif // KORE_OS_WINDOWS

// ---------- Minimal Shader Setup ----------

static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
#ifdef KORE_DEBUG
        char    log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog(sh, (GLsizei)sizeof(log), &len, log);
#endif // KORE_DEBUG
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

#if KORE_OS_WINDOWS
    if (!gfx_create_dummy_context()) {
        // If a context already existed we proceed, else failure.
        if (!wglGetCurrentContext()) {
            return false;
        }
    }
    if (!gfx_load_gl_functions()) {
        gfx_destroy_dummy_context();
        return false;
    }
#endif // KORE_OS_WINDOWS

    g_program = create_program();
    if (!g_program) {
#if KORE_OS_WINDOWS
        gfx_destroy_dummy_context();
#endif // KORE_OS_WINDOWS
        return false;
    }

    g_attr_pos = glGetAttribLocation(g_program, "aPos");
    g_attr_uv  = glGetAttribLocation(g_program, "aUV");
    g_unif_tex = glGetUniformLocation(g_program, "uTex");

    if (glGenVertexArrays) {
        glGenVertexArrays(1, &g_vao);
        if (g_vao) {
            glBindVertexArray(g_vao);
        }
    }

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
        // Even if not inited, destroy dummy context if we own it (Windows)
#if KORE_OS_WINDOWS
        gfx_destroy_dummy_context();
#endif
        return;
    }

    if (g_vbo) {
        glDeleteBuffers(1, &g_vbo);
        g_vbo = 0;
    }
    if (g_vao) {
        if (glBindVertexArray) {
            glBindVertexArray(0);
        }
        if (glDeleteVertexArrays) {
            glDeleteVertexArrays(1, &g_vao);
        }
        g_vao = 0;
    }
    if (g_program) {
        glDeleteProgram(g_program);
        g_program = 0;
    }

#if KORE_OS_WINDOWS
    gfx_destroy_dummy_context();
#endif

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
#if defined(GL_CLAMP_TO_EDGE)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#endif
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
    L->pixels  = (uint32_t*)rgba_pixels; // Store reference to pixel buffer
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
    layer->w      = new_w;
    layer->h      = new_h;
    layer->pixels = (uint32_t*)rgba_pixels; // Update pixel buffer reference
    layer->tex    = create_texture(new_w, new_h, rgba_pixels);
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

    if (g_vao && glBindVertexArray) {
        glBindVertexArray(g_vao);
    }
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

    for (int i = 0; i < layer_count; ++i) {
        GfxLayer* L = layers[i];
        if (!L || !L->enabled) {
            continue;
        }

        // Update texture with current pixel data
        if (L->pixels) {
            gfx_layer_update_pixels(L, L->pixels);
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

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}
