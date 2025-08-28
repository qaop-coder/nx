//------------------------------------------------------------------------------
// Memory emulation
//------------------------------------------------------------------------------

#include "memory.h"

void mem_init(Memory* memory)
{
    memory->ram = KORE_ARRAY_ALLOC(u8, 65536); // Allocate 64KB of RAM

    for (u32 i = 0; i < 65536; i++) {
        memory->ram[i] = 0xff;
    }
}

void mem_done(Memory* memory)
{
    KORE_ARRAY_FREE(memory->ram);
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
        memcpy(memory->ram + addr, data, size);
    }
}

void mem_load_file(Memory* memory, u16 addr, const char* filename)
{
    KData data = $.data_load(filename);
    if ($.is_data_loaded(&data)) {
        mem_load(memory, addr, data.data, (u16)data.size);
        $.data_unload(&data);
    } else {
        $.eprn("Failed to load file: %s", filename);
        exit(EXIT_FAILURE);
    }
}
