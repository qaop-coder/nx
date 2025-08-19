//------------------------------------------------------------------------------
// Frame implementation for Win32
//------------------------------------------------------------------------------

#include "core.h"

#if OS_WINDOWS

#    include "frame.h"

#    include <stdlib.h>

static void frame_cleanup(Frame* f)
{
    // Destroy layers
    for (u64 i = 0; i < array_length(f->layers); ++i) {
        gfx_layer_destroy(f->layers[i]);
    }
    array_free(f->layers);
    gfx_shutdown();

    if (f->hglrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(f->hglrc);
        f->hglrc = 0;
    }
    if (f->hdc && f->hwnd) {
        ReleaseDC(f->hwnd, f->hdc);
        f->hdc = 0;
    }
    if (f->hwnd) {
        DestroyWindow(f->hwnd);
        f->hwnd = 0;
    }
}

static LRESULT CALLBACK WindowProc(HWND   hwnd,
                                   UINT   msg,
                                   WPARAM wParam,
                                   LPARAM lParam)
{
    Frame* f = (Frame*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

Frame frame_open(int width, int height, const char* title)
{
    Frame f = {
        .title  = title,
        .width  = width,
        .height = height,
    };

    HINSTANCE  instance = GetModuleHandle(NULL);
    WNDCLASSEX wc       = {
              .cbSize        = sizeof(WNDCLASSEX),
              .style         = CS_HREDRAW | CS_VREDRAW,
              .lpfnWndProc   = WindowProc,
              .hInstance     = instance,
              .lpszClassName = f.title,
    };

    RegisterClassEx(&wc);

    RECT rc = {0, 0, f.width, f.height};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    f.hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,
                            f.title,
                            f.title,
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            rc.right - rc.left,
                            rc.bottom - rc.top,
                            NULL,
                            NULL,
                            instance,
                            NULL);

    if (f.hwnd == NULL) {
        fprintf(stderr, "Failed to create window: %ld\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    SetWindowLongPtr(f.hwnd, GWLP_USERDATA, (LONG_PTR)&f);

    f.hdc                     = GetDC(f.hwnd);

    PIXELFORMATDESCRIPTOR pfd = {
        .nSize    = sizeof(PIXELFORMATDESCRIPTOR),
        .nVersion = 1,
        .dwFlags  = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType   = PFD_TYPE_RGBA,
        .cColorBits   = 32,
        .cDepthBits   = 24,
        .cStencilBits = 8,
        .iLayerType   = PFD_MAIN_PLANE,
    };
    int pf = ChoosePixelFormat(f.hdc, &pfd);
    SetPixelFormat(f.hdc, pf, &pfd);

    f.hglrc = wglCreateContext(f.hdc);
    if (!f.hglrc) {
        fprintf(
            stderr, "Failed to create OpenGL context: %ld\n", GetLastError());
        frame_cleanup(&f);
        exit(EXIT_FAILURE);
    }

    if (!gfx_init()) {
        fprintf(stderr, "Failed to initialize graphics system\n");
        frame_cleanup(&f);
        exit(EXIT_FAILURE);
    }

    return f;
}

bool frame_loop(Frame* f)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (!f->hwnd) {
        frame_cleanup(f);
        return false;
    }

    RECT cr;
    GetClientRect(f->hwnd, &cr);
    int win_w = cr.right - cr.left;
    int win_h = cr.bottom - cr.top;

    gfx_render(f->layers, array_length(f->layers), win_w, win_h);
    SwapBuffers(f->hdc);
    return true;
}

u32* frame_add_layer(Frame* f, int width, int height)
{
    u32*      pixels = ALLOC_ARRAY(u32, width * height);
    GfxLayer* layer  = gfx_layer_create(width, height, pixels);
    if (!layer) {
        fprintf(stderr, "Failed to create graphics layer\n");
        free(pixels);
        return NULL;
    }
    array_push(f->layers, layer);
    return pixels;
}

#endif // OS_WINDOWS
