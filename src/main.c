#include "config.h"
#include "memory.h"
#include "window.h"

int main(int argc, char** argv)
{
    Memory memory = {0};
    mem_init(&memory);

    u32    screen[WINDOW_WIDTH * WINDOW_HEIGHT] = {0};
    Window main_window                          = {
                                 .title  = "Nx (Dev.9)",
                                 .width  = WINDOW_WIDTH,
                                 .height = WINDOW_HEIGHT,
                                 .buf    = screen,
    };

    win_open(&main_window);

    mem_load_file(&memory, 0x0000, "etc/roms/48.rom");
    mem_load_file(&memory, 0x4000, "etc/screens/AticAtac.scr");

    while (win_loop(&main_window)) {
    }

    mem_done(&memory);
    return 0;
}
