//------------------------------------------------------------------------------
// Frame implementation for Linux (X11)
//------------------------------------------------------------------------------

#include "core.h"

#if OS_LINUX

#    include "window.h"

#    include <X11/Xatom.h>
#    include <stdlib.h>

static Atom g_wm_delete_window = None;

static void win_cleanup(Frame* f)
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

void win_open(Frame* f)
{
    f->display = XOpenDisplay(NULL);
    if (f->display == NULL) {
        fprintf(stderr, "Failed to open X display\n");
        exit(EXIT_FAILURE);
    }

    int screen_num = DefaultScreen(f->display);
    f->window      = XCreateSimpleWindow(f->display,
                                    RootWindow(f->display, screen_num),
                                    0,
                                    0,
                                    f->width,
                                    f->height,
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
    f->image = XCreateImage(f->display,
                            DefaultVisual(f->display, screen_num),
                            DefaultDepth(f->display, screen_num),
                            ZPixmap,
                            0,
                            (char*)f->buf,
                            f->width,
                            f->height,
                            32,
                            0);
    if (f->image == NULL) {
        fprintf(stderr, "Failed to create XImage\n");
        exit(EXIT_FAILURE);
    }

    XFlush(f->display);
}

bool win_loop(Frame* f)
{
    XEvent event;
    while (XPending(f->display)) {
        XNextEvent(f->display, &event);
        switch (event.type) {
        case Expose:
            XPutImage(f->display,
                      f->window,
                      f->gc,
                      f->image,
                      0,
                      0,
                      0,
                      0,
                      f->width,
                      f->height);
            break;

        case ClientMessage:
            if ((Atom)event.xclient.data.l[0] == g_wm_delete_window) {
                win_cleanup(f);
                return false; // Exit the loop
            }
            break;

        case DestroyNotify:
            win_cleanup(f);
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
