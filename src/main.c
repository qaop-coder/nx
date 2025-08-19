#include "config.h"
#include "memory.h"
#include "window.h"

int main(int argc, char** argv)
{
    Memory memory = {0};
    mem_init(&memory);

    u32   screen[WINDOW_WIDTH * WINDOW_HEIGHT] = {0};
    Frame main_window                          = {
                                 .title  = "Nx (Dev.9)",
                                 .width  = WINDOW_WIDTH,
                                 .height = WINDOW_HEIGHT,
                                 .buf    = screen,
                                 .scale  = 3,
    };

    win_open(&main_window);

    mem_load_file(&memory, 0x0000, "etc/roms/48.rom");
    mem_load_file(&memory, 0x4000, "etc/screens/AticAtac.scr");

    while (win_loop(&main_window)) {
        static unsigned frame = 0;
        for (int y = 0; y < WINDOW_HEIGHT; ++y) {
            u32* row = &screen[y * WINDOW_WIDTH];
            for (int x = 0; x < WINDOW_WIDTH; ++x) {
                unsigned r = (x + frame) & 0xFF;
                unsigned g = (y + frame) & 0xFF;
                unsigned b = ((x ^ y) + frame) & 0xFF;
                row[x]     = 0xFF000000u | (r << 16) | (g << 8) | b; // ARGB
            }
        }
        frame++;
    }

    printf("Exiting...\n");

    mem_done(&memory);
    return 0;
}
