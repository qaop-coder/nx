//------------------------------------------------------------------------------
// Memory emulation
//------------------------------------------------------------------------------

#include "memory.h"

#include <stdlib.h>

void mem_init(Memory* memory)
{
    memory->ram = (u8*)malloc(65536); // Allocate 64KB of RAM
    CHECK_MEMORY(memory->ram);
}

void mem_done(Memory* memory)
{
    free(memory->ram);
    memory->ram = NULL; // Set pointer to NULL after freeing
}

void mem_poke(Memory* memory, u16 addr, u8 value)
{
    if (addr >= 16384) {
        memory->ram[addr] = value;
    }
}

u8 mem_peek(Memory* memory, u16 addr) { return memory->ram[addr]; }

void mem_poke16(Memory* memory, u16 addr, u16 value)
{
    mem_poke(memory, addr, (u8)(value & 0xFF));            // Lower byte
    mem_poke(memory, addr + 1, (u8)((value >> 8) & 0xFF)); // Upper byte
}

u16 mem_peek16(Memory* memory, u16 addr)
{
    u16 value = 0;
    value |= (u16)mem_peek(memory, addr);          // Lower byte
    value |= (u16)mem_peek(memory, addr + 1) << 8; // Upper byte
    return value;
}

void mem_load(Memory* memory, u16 addr, const u8* data, u16 size)
{
    if (addr + size <= 65536 && data != NULL) {
        for (u16 i = 0; i < size; i++) {
            mem_poke(memory, addr + i, data[i]);
        }
    }
}
