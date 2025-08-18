//------------------------------------------------------------------------------
// Configuration constants
//------------------------------------------------------------------------------

#pragma once

// The width and height of the actual pixel area of the screen
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

// The width and height of the TV
//
// The image comprises of 64 lines of border, 192 lines of pixel data, and 56
// lines of border.  Each line comprises of 48 pixels of border, 256 pixels of
// pixel data, followed by another 48 pixels of border.
//
// Timing of a line is 24T for each border, 128T for the pixel data, and 48T for
// the horizontal retrace (224 t-states).
#define TV_WIDTH 352
#define TV_HEIGHT 312

// The width and height of the window that displays the emulated image (can be
// smaller than the TV size).
#define WINDOW_WIDTH 320
#define WINDOW_HEIGHT 256

// Size of the borders
#define BORDER_WIDTH ((WINDOW_WIDTH - SCREEN_WIDTH) / 2
#define BORDER_HEIGHT ((WINDOW_HEIGHT - SCREEN_HEIGHT) / 2

// The actual size of the UI window in pixels.
#define UI_WIDTH (WINDOW_WIDTH * 2)
#define UI_HEIGHT (WINDOW_HEIGHT * 2)

#define DEFAULT_SCALE 3
