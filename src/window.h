//------------------------------------------------------------------------------
// Window interface
//------------------------------------------------------------------------------

#pragma once

#include "core.h"

//------------------------------------------------------------------------------
// Window data structure
//------------------------------------------------------------------------------

typedef struct {
    const char* title;
    int         width;
    int         height;
    int         scale;
    uint32_t*   buf;

#if OS_WINDOWS
    HWND hwnd;
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

void win_open(Frame* w);
bool win_loop(Frame* w);
