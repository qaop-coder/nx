#include "memory.h"

int main(int argc, char** argv)
{
    Memory memory = {0};
    printf("Starting memory emulation...\n");
    mem_init(&memory);

    mem_load_file(&memory, 0x0000, "etc/roms/48.rom");
    mem_load_file(&memory, 0x4000, "etc/screens/AticAtac.scr");

    mem_done(&memory);
    return 0;
}
