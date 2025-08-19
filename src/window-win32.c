//------------------------------------------------------------------------------
// Frame implementation for Win32
//------------------------------------------------------------------------------

#include "core.h"

#if OS_WINDOWS

#    include "window.h"

#    include <stdlib.h>

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

            if (f && f->buf) {
                if (f->scale <= 0) {
                    f->scale = 1; // Default scale
                }
                int win_w     = f->width * f->scale;
                int win_h     = f->height * f->scale;

                BITMAPINFO bi = {
                    .bmiHeader =
                        {
                            .biSize        = sizeof(BITMAPINFOHEADER),
                            .biWidth       = f->width,
                            .biHeight      = -f->height, // Negative to indicate
                                                         // top-down bitmap
                            .biPlanes      = 1,
                            .biBitCount    = 32,
                            .biCompression = BI_RGB,
                        },
                };

                SetStretchBltMode(hdc, COLORONCOLOR);
                StretchDIBits(hdc,
                              0,
                              0,
                              win_w,
                              win_h,
                              0,
                              0,
                              f->width,
                              f->height,
                              f->buf,
                              &bi,
                              DIB_RGB_COLORS,
                              SRCCOPY);

                EndPaint(hwnd, &ps);
            }
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

void win_open(Frame* f)
{
    if (f->scale <= 0) {
        f->scale = 1; // Default scale
    }
    int win_w           = f->width * f->scale;
    int win_h           = f->height * f->scale;

    HINSTANCE  instance = GetModuleHandle(NULL);
    WNDCLASSEX wc       = {
              .cbSize        = sizeof(WNDCLASSEX),
              .style         = CS_HREDRAW | CS_VREDRAW,
              .lpfnWndProc   = WindowProc,
              .hInstance     = instance,
              .lpszClassName = f->title,
    };

    RegisterClassEx(&wc);

    RECT rc = {0, 0, win_w, win_h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    f->hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,
                             f->title,
                             f->title,
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             rc.right - rc.left,
                             rc.bottom - rc.top,
                             NULL,
                             NULL,
                             instance,
                             NULL);

    if (f->hwnd == NULL) {
        fprintf(stderr, "Failed to create window: %ld\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    SetWindowLongPtr(f->hwnd, GWLP_USERDATA, (LONG_PTR)f);
}

bool win_loop(Frame* w)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    InvalidateRect(w->hwnd, NULL, TRUE);
    return true;
}

#endif // OS_WINDOWS
