//------------------------------------------------------------------------------
// Window interface
//------------------------------------------------------------------------------

#pragma once

#include "gfx.h"

//------------------------------------------------------------------------------
// Window data structure
//------------------------------------------------------------------------------

typedef struct {
    const char* title;
    int         width;
    int         height;
    KArray(GfxLayer*) layers;

    // FPS tracking
    KTimePoint last_time;
    u64        frame_count;
    f64        fps;

#if KORE_OS_WINDOWS
    HWND  hwnd;
    HDC   hdc;
    HGLRC hglrc;
#elif KORE_OS_LINUX
    Display* display;
    Window   window;
    GC       gc;
    XImage*  image;
#else
#    error "Unsupported OS"
#endif
} Frame;

//------------------------------------------------------------------------------
// Window API
//------------------------------------------------------------------------------

Frame frame_open(int width, int height, const char* title);
bool  frame_loop(Frame* w);
u32*  frame_add_layer(Frame* w, int width, int height);
f64   frame_fps(Frame* w);

void frame_free_pixels(u32* pixels);
