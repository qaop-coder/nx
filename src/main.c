#include "memory.h"

int main(int argc, char** argv)
{
    Memory memory = {0};
    mem_init(&memory);

    mem_done(&memory);
    return 0;
}
