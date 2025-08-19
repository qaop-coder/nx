//------------------------------------------------------------------------------
// Window interface
//------------------------------------------------------------------------------

#pragma once

#include "core.h"
#include "gfx.h"

//------------------------------------------------------------------------------
// Window data structure
//------------------------------------------------------------------------------

typedef struct {
    const char* title;
    int         width;
    int         height;
    Array(GfxLayer*) layers;

#if OS_WINDOWS
    HWND  hwnd;
    HDC   hdc;
    HGLRC hglrc;
#elif OS_LINUX
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
