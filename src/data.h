//------------------------------------------------------------------------------
// File mapping interface
//------------------------------------------------------------------------------

#pragma once

#include "core.h"

typedef struct {
    u8* data;
    u64 size;

#if OS_WINDOWS
    HANDLE file_handle;
    HANDLE mapping_handle;
#elif OS_LINUX
    int fd;
#else
#    error "File mapping not implemented for this OS"
#endif
} Data;

Data data_load(const char* filename);
bool data_loaded(const Data* data);
void data_unload(Data* data);
