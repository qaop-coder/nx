#define CORE_IMPLEMENTATION
#include "core.h"

#include "config.h"
#include "frame.h"
#include "memory.h"

#define WINDOW_SCALE 3

int main(int argc, char** argv)
{
    Memory memory = {0};
    mem_init(&memory);

    Frame main_window = frame_open(WINDOW_WIDTH * WINDOW_SCALE,
                                   WINDOW_HEIGHT * WINDOW_SCALE,
                                   "Nx (Dev.9)");

    u32* screen = frame_add_layer(&main_window, WINDOW_WIDTH, WINDOW_HEIGHT);

    mem_load_file(&memory, 0x0000, "etc/roms/48.rom");
    mem_load_file(&memory, 0x4000, "etc/screens/AticAtac.scr");

    while (frame_loop(&main_window)) {
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
