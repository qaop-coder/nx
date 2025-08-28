//------------------------------------------------------------------------------
// Memory emulation
//------------------------------------------------------------------------------

#pragma once

#include "kore.h"

typedef struct {
    u8* ram;
} Memory;

void mem_init(Memory* memory);
void mem_done(Memory* memory);

void mem_poke(Memory* memory, u16 addr, u8 value);
u8   mem_peek(Memory* memory, u16 addr);

void mem_poke16(Memory* memory, u16 addr, u16 value);
u16  mem_peek16(Memory* memory, u16 addr);

void mem_load(Memory* memory, u16 addr, const u8* data, u16 size);
void mem_load_file(Memory* memory, u16 addr, const char* filename);
