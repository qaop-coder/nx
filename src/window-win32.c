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
    Frame* w = (Frame*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc   = BeginPaint(hwnd, &ps);
            HDC         memdc = CreateCompatibleDC(hdc);
            HBITMAP     hbmp = CreateCompatibleBitmap(hdc, w->width, w->height);
            HBITMAP     oldbmp = SelectObject(memdc, hbmp);

            BITMAPINFO bi      = {
                     .bmiHeader =
                    {
                             .biSize  = sizeof(BITMAPINFOHEADER),
                             .biWidth = w->width,
                             .biHeight =
                            -w->height, // Negative to indicate top-down bitmap
                             .biPlanes      = 1,
                             .biBitCount    = 32,
                             .biCompression = BI_RGB,
                    },
            };

            SetDIBitsToDevice(memdc,
                              0,
                              0,
                              w->width,
                              w->height,
                              0,
                              0,
                              0,
                              w->height,
                              w->buf,
                              &bi,
                              DIB_RGB_COLORS);
            BitBlt(hdc, 0, 0, w->width, w->height, memdc, 0, 0, SRCCOPY);
            SelectObject(memdc, oldbmp);
            DeleteObject(hbmp);
            DeleteDC(memdc);
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

void win_open(Frame* w)
{
    HINSTANCE  instance = GetModuleHandle(NULL);
    WNDCLASSEX wc       = {
              .cbSize        = sizeof(WNDCLASSEX),
              .style         = CS_HREDRAW | CS_VREDRAW,
              .lpfnWndProc   = WindowProc,
              .hInstance     = instance,
              .lpszClassName = w->title,
    };

    RegisterClassEx(&wc);

    w->hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,
                             w->title,
                             w->title,
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             w->width,
                             w->height,
                             NULL,
                             NULL,
                             instance,
                             NULL);

    if (w->hwnd == NULL) {
        fprintf(stderr, "Failed to create window: %ld\n", GetLastError());
        exit(EXIT_FAILURE);
    }

    SetWindowLongPtr(w->hwnd, GWLP_USERDATA, (LONG_PTR)w);
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
