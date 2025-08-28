//------------------------------------------------------------------------------
// Frame implementation for Linux (X11)
//------------------------------------------------------------------------------

#include "kore.h"

#if KORE_OS_LINUX

#    include "window.h"

#    include <X11/Xatom.h>
#    include <stdlib.h>

static Atom g_wm_delete_window = None;

static void frame_cleanup(Frame* f)
{
    if (!f) {
        return;
    }
    if (f->display) {
        if (f->image) {
            f->image->data = NULL; // Prevent double free
            XDestroyImage(f->image);
            f->image = nullptr;
        }
        if (f->gc) {
            XFreeGC(f->display, f->gc);
            f->gc = nullptr;
        }
        if (f->window) {
            XDestroyWindow(f->display, f->window);
            f->window = 0;
        }
        XCloseDisplay(f->display);
        f->display = nullptr;
    }
}

void frame_open(Frame* f)
{
    f->display = XOpenDisplay(NULL);
    if (f->display == NULL) {
        fprintf(stderr, "Failed to open X display\n");
        exit(EXIT_FAILURE);
    }

    int screen_num = DefaultScreen(f->display);
    int win_w      = f->width * f->scale;
    int win_h      = f->height * f->scale;
    f->window      = XCreateSimpleWindow(f->display,
                                    RootWindow(f->display, screen_num),
                                    0,
                                    0,
                                    win_w,
                                    win_h,
                                    1,
                                    BlackPixel(f->display, screen_num),
                                    WhitePixel(f->display, screen_num));

    XStoreName(f->display, f->window, f->title);
    XSelectInput(f->display,
                 f->window,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    g_wm_delete_window = XInternAtom(f->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(f->display, f->window, &g_wm_delete_window, 1);

    XMapWindow(f->display, f->window);

    f->gc = XCreateGC(f->display, f->window, 0, NULL);
    XSync(f->display, f->window);

    int   img_w  = win_w;
    int   img_h  = win_h;
    char* pixels = (char*)malloc((size_t)img_w * img_h * 4);
    if (!pixels) {
        fprintf(stderr, "Failed to allocate memory for image pixels\n");
        exit(EXIT_FAILURE);
    }
    f->image = XCreateImage(f->display,
                            DefaultVisual(f->display, screen_num),
                            DefaultDepth(f->display, screen_num),
                            ZPixmap,
                            0,
                            pixels,
                            img_w,
                            img_h,
                            32,
                            0);
    if (f->image == NULL) {
        fprintf(stderr, "Failed to create XImage\n");
        free(pixels);
        exit(EXIT_FAILURE);
    }

    XFlush(f->display);
}

static void win_draw(Frame* f)
{
    if (!f->image) {
        return;
    }

    const int scale = (f->scale <= 0) ? 1 : f->scale;
    const int src_w = f->width;
    const int src_h = f->height;
    const int dst_w = src_w * scale;
    // const int dst_h = src_h * scale;

    const u32* src  = (const u32*)f->buf;
    u32*       dst  = (u32*)f->image->data;

    // Nearest-neighbor scale (pixel replication)
    for (int y = 0; y < src_h; ++y) {
        const u32* src_row = src + y * src_w;
        for (int sy = 0; sy < scale; ++sy) {
            u32* dst_row = dst + (y * scale + sy) * dst_w;
            for (int x = 0; x < src_w; ++x) {
                u32 px = src_row[x];
                for (int sx = 0; sx < scale; ++sx) {
                    dst_row[x * scale + sx] = px;
                }
            }
        }
    }

    XPutImage(f->display,
              f->window,
              f->gc,
              f->image,
              0,
              0,
              0,
              0,
              f->image->width,
              f->image->height);
    XFlush(f->display);
}

bool frame_loop(Frame* f)
{
    XEvent event;
    win_draw(f);
    while (XPending(f->display)) {
        XNextEvent(f->display, &event);
        switch (event.type) {
        case Expose:
            win_draw(f);
            break;

        case ClientMessage:
            if ((Atom)event.xclient.data.l[0] == g_wm_delete_window) {
                frame_cleanup(f);
                return false; // Exit the loop
            }
            break;

        case DestroyNotify:
            frame_cleanup(f);
            return false; // Exit the loop

        case KeyPress:
        case KeyRelease:
            // Handle key events if needed
            break;

        case ButtonPress:
            // Handle mouse button press if needed
            break;

        default:
            break;
        }
    }

    return true; // Continue the loop
}

#endif // OS_LINUX
