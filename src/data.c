//------------------------------------------------------------------------------
// File mapping implementation
//------------------------------------------------------------------------------

#include "data.h"

#if OS_LINUX
#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif // OS_LINUX

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
#elif OS_LINUX
    data.fd = open(filename, O_RDONLY);
    if (data.fd < 0) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return data;
    }
    struct stat st;
    if (fstat(data.fd, &st) < 0) {
        fprintf(stderr, "Error getting file size: %s\n", filename);
        close(data.fd);
        return data;
    }
    data.size = st.st_size;
    data.data = (u8*)mmap(NULL, data.size, PROT_READ, MAP_PRIVATE, data.fd, 0);
    if (data.data == MAP_FAILED) {
        fprintf(stderr, "Error mapping file: %s\n", filename);
        close(data.fd);
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
#elif OS_LINUX
    if (data->data) {
        munmap(data->data, data->size);
    }
    if (data->fd >= 0) {
        close(data->fd);
    }
#else
#    error "File mapping not implemented for this OS"
#endif

    data->data = NULL;
    data->size = 0;
}
