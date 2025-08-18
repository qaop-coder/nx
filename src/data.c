//------------------------------------------------------------------------------
// File mapping implementation
//------------------------------------------------------------------------------

#include "data.h"

Data data_load(const char* filename)
{
    Data data = {0};

#if OS_WINDOWS
    data.file_handle = CreateFileA(filename,
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   NULL,
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL);
    if (data.file_handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return data;
    }

    data.size = GetFileSize(data.file_handle, NULL);
    if (data.size == INVALID_FILE_SIZE) {
        fprintf(stderr, "Error getting file size: %s\n", filename);
        CloseHandle(data.file_handle);
        return data;
    }

    data.mapping_handle =
        CreateFileMappingA(data.file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (data.mapping_handle == NULL) {
        fprintf(stderr, "Error creating file mapping: %s\n", filename);
        CloseHandle(data.file_handle);
        return data;
    }

    data.data =
        (u8*)MapViewOfFile(data.mapping_handle, FILE_MAP_READ, 0, 0, data.size);
    if (data.data == NULL) {
        fprintf(stderr, "Error mapping view of file: %s\n", filename);
        CloseHandle(data.mapping_handle);
        CloseHandle(data.file_handle);
        return data;
    }
#else
#    error "File mapping not implemented for this OS"
#endif

    return data;
}

bool data_loaded(const Data* data) { return data->data != NULL; }

void data_unload(Data* data)
{
#if OS_WINDOWS
    if (data->data) {
        UnmapViewOfFile(data->data);
    }
    if (data->mapping_handle) {
        CloseHandle(data->mapping_handle);
    }
    if (data->file_handle) {
        CloseHandle(data->file_handle);
    }
#else
#    error "File mapping not implemented for this OS"
#endif

    data->data = NULL;
    data->size = 0;
}
