
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <sys/stat.h>
#    include <windows.h>
#else
#    include <dirent.h>
#    include <sys/stat.h>
#endif

//
// Basic types
//

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float  f32;
typedef double f64;

typedef intptr_t  isize;
typedef uintptr_t usize;

typedef struct {
    const char* data;
    size_t      length;
} String;

#define STRINGV(s) (int)(s).length, (s).data

//
// ANSI Colour sequences
//

#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_UNDERLINE "\033[4m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN "\033[36m"
#define ANSI_WHITE "\033[37m"

//
// Dynamic memory allocation
//

typedef struct {
    size_t total_size;
} MemoryInfo;

void* memory_alloc(size_t size)
{
    void* ptr = malloc(size + sizeof(MemoryInfo));
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    MemoryInfo* info = (MemoryInfo*)ptr;
    info->total_size = size;
    return info + 1;
}

void* memory_realloc(void* ptr, size_t new_size)
{
    if (!ptr) {
        return memory_alloc(new_size);
    }

    MemoryInfo* info    = (MemoryInfo*)ptr - 1;
    void*       new_ptr = realloc(info, new_size + sizeof(MemoryInfo));
    if (!new_ptr) {
        fprintf(stderr, "Memory reallocation failed\n");
        exit(EXIT_FAILURE);
    }

    info             = (MemoryInfo*)new_ptr;
    info->total_size = new_size;
    return info + 1;
}

void* memory_free(void* ptr)
{
    if (!ptr) {
        return nullptr;
    }

    MemoryInfo* info = (MemoryInfo*)ptr - 1;
    free(info);
    return nullptr;
}

size_t memory_size(void* ptr)
{
    if (!ptr) {
        return 0;
    }

    MemoryInfo* info = (MemoryInfo*)ptr - 1;
    return info->total_size;
}

//
// Arena memory allocation
//

typedef struct {
    void* buffer;
    usize cursor;
} Arena;

#define ARENA_PAGE_SIZE 4096

Arena arena_init()
{
    void* buffer = malloc(ARENA_PAGE_SIZE);
    Arena arena  = {.buffer = buffer, .cursor = 0};
    return arena;
}

void arena_free(Arena* arena)
{
    if (arena->buffer) {
        free(arena->buffer);
        arena->buffer = nullptr;
    }
    arena->cursor = 0;
}

u64 arena_store(Arena* arena) { return arena->cursor; }

void arena_restore(Arena* arena, u64 cursor)
{
    if (cursor > arena->cursor) {
        fprintf(stderr, "Invalid cursor restore in arena\n");
        exit(EXIT_FAILURE);
    }
    arena->cursor = cursor;
}

void* arena_alloc(Arena* arena, size_t size)
{
    while (memory_size(arena->buffer) < arena->cursor + size) {
        size_t new_size = memory_size(arena->buffer) + ARENA_PAGE_SIZE;
        arena->buffer   = memory_realloc(arena->buffer, new_size);
    }

    void* ptr = (char*)arena->buffer + arena->cursor;
    arena->cursor += size;
    return ptr;
}

void* arena_mark(Arena* arena)
{
    void* mark = (char*)arena->buffer + arena->cursor;
    return mark;
}

void arena_reset(Arena* arena) { arena->cursor = 0; }

//
// Dynamic array implementation
//

typedef struct {
    usize length;
} ArrayInfo;

#define __array_info(arr) ((ArrayInfo*)(arr) - 1)
#define __array_capacity(arr) (memory_size(__array_info(arr)) / sizeof(*(arr)))
#define __array_length(arr) __array_info(arr)->length

void* __array_maybe_grow(void*  arr,
                         size_t element_size,
                         size_t additional_elements)
{
    if (!arr) {
        ArrayInfo* new_arr = memory_alloc(sizeof(ArrayInfo) +
                                          element_size * additional_elements);
        new_arr->length    = 0;
        return new_arr + 1;
    }

    ArrayInfo* info              = __array_info(arr);
    size_t     current_capacity  = memory_size(info) / element_size;
    size_t     required_capacity = info->length + additional_elements;

    if (required_capacity <= current_capacity) {
        return arr;
    }

    size_t new_capacity =
        current_capacity ? current_capacity * 2 : required_capacity;
    ArrayInfo* new_arr =
        memory_realloc(info, sizeof(ArrayInfo) + element_size * new_capacity);

    return new_arr + 1;
}

#define Array(type) type*

#define array_length(arr) ((arr) ? __array_length(arr) : 0)
#define array_push(arr, value)                                                 \
    arr = (typeof(arr))__array_maybe_grow(arr, sizeof(*(arr)), 1),             \
    (arr)[__array_length(arr)] = (value), __array_length(arr) += 1
#define array_free(arr) (arr) = memory_free(__array_info(arr))

//
// String routines
//

String string_view(const char* str)
{
    String s;
    s.data   = str;
    s.length = str ? strlen(str) : 0;
    return s;
}

bool string_ends_with_zstring(String str, const char* suffix)
{
    size_t suffix_length = strlen(suffix);
    if (str.length < suffix_length) {
        return false;
    }
    return strncmp(str.data + str.length - suffix_length,
                   suffix,
                   suffix_length) == 0;
}

//
// String builder
//

typedef struct {
    Arena* arena;
    String str;
} StringBuilder;

StringBuilder string_builder_init(Arena* arena)
{
    StringBuilder sb;
    sb.arena      = arena;
    sb.str.data   = nullptr;
    sb.str.length = 0;
    return sb;
}

void* string_builder_alloc(StringBuilder* sb, size_t size)
{
    if (sb->str.data == nullptr) {
        sb->str.data   = (char*)arena_mark(sb->arena);
        sb->str.length = 0;
    }

    void* new_buf = arena_alloc(sb->arena, size);
    sb->str.length += size;

    return new_buf;
}

void string_builder_append_zstring(StringBuilder* sb, const char* str)
{
    size_t len = strlen(str);
    if (len == 0) {
        return;
    }

    char* new_str = string_builder_alloc(sb, len);
    memcpy(new_str, str, len);
}

void string_builder_append_string(StringBuilder* sb, String str)
{
    if (str.length == 0) {
        return;
    }

    char* new_str = string_builder_alloc(sb, str.length);
    memcpy(new_str, str.data, str.length);
}

void string_builder_null_terminate(StringBuilder* sb)
{
    char* null_place = string_builder_alloc(sb, 1);
    *null_place      = '\0';
}

//
// File Management
//

u64 file_time(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return (u64)st.st_mtime;
}

i32 file_time_compare(u64 time1, u64 time2)
{
    if (time1 < time2) {
        return -1;
    } else if (time1 > time2) {
        return 1;
    }
    return 0;
}

void file_delete(const char* path)
{
#ifdef _WIN32
    if (DeleteFileA(path) == 0) {
        fprintf(stderr, "Failed to delete file: %s\n", path);
    }
#else
    if (remove(path) != 0) {
        fprintf(stderr, "Failed to delete file: %s\n", path);
    }
#endif
}

void file_rename(const char* old_path, const char* new_path)
{
    // Attempt to rename the file
#ifdef _WIN32
    if (MoveFileA(old_path, new_path) == 0) {
        fprintf(stderr,
                "Failed to rename file from '%s' to '%s'\n",
                old_path,
                new_path);
    }
#else
    if (rename(old_path, new_path) != 0) {
        fprintf(stderr,
                "Failed to rename file from '%s' to '%s'\n",
                old_path,
                new_path);
    }
#endif
}

static void files_list_recursive(Arena* arena,
                                 Array(String) * files,
                                 const char* directory,
                                 bool        recursive)
{
#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    HANDLE           find_handle;

    StringBuilder sb = string_builder_init(arena);
    string_builder_append_zstring(&sb, directory);
    string_builder_append_zstring(&sb, "\\*");
    string_builder_null_terminate(&sb);

    find_handle = FindFirstFileA(sb.str.data, &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(find_data.cFileName, ".") != 0 &&
                strcmp(find_data.cFileName, "..") != 0) {

                StringBuilder file_sb = string_builder_init(arena);
                string_builder_append_zstring(&file_sb, directory);
                string_builder_append_zstring(&file_sb, "\\");
                string_builder_append_zstring(&file_sb, find_data.cFileName);

                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (recursive) {
                        // If it's a directory and recursive, list files inside
                        // it
                        files_list_recursive(
                            arena, files, file_sb.str.data, recursive);
                    }
                } else {
                    // Only add files, not directories
                    array_push(*files, file_sb.str);
                }
            }
        } while (FindNextFileA(find_handle, &find_data));
        FindClose(find_handle);
    }
#elif defined(__linux__) || defined(__APPLE__)
    DIR* dir = opendir(directory);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 &&
                strcmp(entry->d_name, "..") != 0) {
                StringBuilder file_sb = string_builder_init(arena);
                string_builder_append_zstring(&file_sb, directory);
                string_builder_append_zstring(&file_sb, "/");
                string_builder_append_zstring(&file_sb, entry->d_name);

                // Check if it's a directory
                struct stat st;
                if (stat(file_sb.str.data, &st) == 0) {
                    if (S_ISDIR(st.st_mode)) {
                        // If it's a directory and recursive, list files inside
                        // it
                        if (recursive) {
                            files_list_recursive(
                                arena, files, file_sb.str.data, recursive);
                        }
                    } else if (S_ISREG(st.st_mode)) {
                        // Only add regular files, not directories
                        array_push(*files, file_sb.str);
                    }
                }
            }
        }
        closedir(dir);
    } else {
        fprintf(stderr, "Failed to open directory: %s\n", directory);
        exit(EXIT_FAILURE);
    }
#else
#    error "File listing not implemented on this platform"
#endif // _WIN32
}

Array(String) files_list(Arena* arena, const char* directory, bool recursive)
{
    Array(String) files = nullptr;
    files_list_recursive(arena, &files, directory, recursive);
    return files;
}

//
// Build system
//

i32 build_run(String command)
{
    printf("Running command: %.*s\n", STRINGV(command));

    char* command_buffer = (char*)memory_alloc(command.length + 1);
    memcpy(command_buffer, command.data, command.length);
    command_buffer[command.length] = '\0';

#ifdef _WIN32
    STARTUPINFOA        si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    BOOL success  = CreateProcessA(NULL,
                                  (LPSTR)command_buffer,
                                  NULL,
                                  NULL,
                                  FALSE,
                                  0,
                                  NULL,
                                  NULL,
                                  &si,
                                  &pi);

    i32 exit_code = 1;
    if (success) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, (LPDWORD)&exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        fprintf(stderr, "Failed to run command: %s\n", command.data);
    }

#else
    i32 exit_code = system(command.data);
#endif
    return exit_code;
}

//
// C compilation support
//

typedef struct {
    Arena* arena;
    Array(String) files;
    Array(String) libraries;
    bool   debug;
    String output_file;
    String output_folder;
} CompileInfo;

CompileInfo compile_info_init(Arena* arena, const char* output_file)
{
    CompileInfo info;
    info.arena       = arena;
    info.files       = nullptr;
    info.libraries   = nullptr;
    info.debug       = false;

    StringBuilder sb = string_builder_init(arena);
    string_builder_append_zstring(&sb, output_file);
#ifdef _WIN32
    string_builder_append_zstring(&sb, ".exe");
#endif
    info.output_file = sb.str;

    return info;
}

void compile_info_debug(CompileInfo* info) { info->debug = true; }

void compile_info_add_file(CompileInfo* info, const char* file)
{
    StringBuilder sb = string_builder_init(info->arena);
    string_builder_append_zstring(&sb, file);
    array_push(info->files, sb.str);
}

void compile_info_add_library(CompileInfo* info, const char* library)
{
    StringBuilder sb = string_builder_init(info->arena);
    string_builder_append_zstring(&sb, library);
    array_push(info->libraries, sb.str);
}

void compile_info_add_folder(CompileInfo* info,
                             const char*  folder,
                             bool         recursive)
{
    Array(String) files = files_list(info->arena, folder, recursive);
    for (usize i = 0; i < array_length(files); ++i) {
        if (string_ends_with_zstring(files[i], ".c")) {
            // Only add C/C++ source files
            array_push(info->files, files[i]);
        }
    }
    array_free(files);
}

void compile_info_dump(CompileInfo* info)
{
    printf("CompileInfo:\n");
    printf("  Output file: %.*s\n",
           (int)info->output_file.length,
           info->output_file.data);
    printf("  Debug mode: %s\n", info->debug ? "enabled" : "disabled");
    printf("  Files to compile:\n");
    for (usize i = 0; i < array_length(info->files); ++i) {
        printf("    %.*s\n", (int)info->files[i].length, info->files[i].data);
    }
}

void compile_info_output_folder(CompileInfo* info, const char* folder)
{
    StringBuilder sb = string_builder_init(info->arena);
    string_builder_append_zstring(&sb, folder);
    info->output_folder = sb.str;
}

i32 compile(CompileInfo* info)
{
    printf("Compiling project...\n");
    StringBuilder sb = string_builder_init(info->arena);
    string_builder_append_zstring(&sb, "mkdir -p ");
    string_builder_append_string(&sb, info->output_folder);
    string_builder_null_terminate(&sb);
    build_run(sb.str);

    sb = string_builder_init(info->arena);
    string_builder_append_zstring(&sb, "clang --std=c23 -o ");
    string_builder_append_string(&sb, info->output_folder);
    string_builder_append_zstring(&sb, "/");
    string_builder_append_string(&sb, info->output_file);

    if (info->debug) {
        string_builder_append_zstring(&sb, " -g");
    }

    for (usize i = 0; i < array_length(info->libraries); ++i) {
        string_builder_append_zstring(&sb, " -l");
        string_builder_append_string(&sb, info->libraries[i]);
    }

    for (usize i = 0; i < array_length(info->files); ++i) {
        string_builder_append_zstring(&sb, " ");
        string_builder_append_string(&sb, info->files[i]);
    }

    string_builder_null_terminate(&sb);
    return build_run(sb.str);
}

i32 compile_project(const char*  exe_name,
                    const char*  source_folder,
                    const char** libraries,
                    const char*  output_folder)
{
    Arena       temp_arena = arena_init();
    CompileInfo info       = compile_info_init(&temp_arena, exe_name);
    compile_info_add_folder(&info, source_folder, true);
    compile_info_output_folder(&info, output_folder);
    compile_info_debug(&info);
    if (libraries) {
        while (*libraries) {
            compile_info_add_library(&info, *libraries);
            libraries++;
        }
    }
    return compile(&info);
}

//
// Build checker
//

void build_check(int argc, char** argv)
{
    const char* exe_file      = argv[0];
    Arena       temp_arena    = arena_init();

    // Delete the old executable file
    StringBuilder sb_old_name = string_builder_init(&temp_arena);
    string_builder_append_zstring(&sb_old_name, exe_file);
    string_builder_append_zstring(&sb_old_name, ".old");
    string_builder_null_terminate(&sb_old_name);
    if (file_time(sb_old_name.str.data) > 0) {
        file_delete(sb_old_name.str.data);
    }

#ifdef _WIN32
    // Delete the old .ilk file if it exists
    StringBuilder sb_ilk_name = string_builder_init(&temp_arena);
    string_builder_append_zstring(&sb_ilk_name, exe_file);
    string_builder_append_zstring(&sb_ilk_name, ".ilk");
    string_builder_null_terminate(&sb_ilk_name);
    if (file_time(sb_ilk_name.str.data) > 0) {
        file_delete(sb_ilk_name.str.data);
    }

    // Delete the old .pdb file if it exists
    StringBuilder sb_pdb_name = string_builder_init(&temp_arena);
    string_builder_append_zstring(&sb_pdb_name, exe_file);
    string_builder_append_zstring(&sb_pdb_name, ".pdb");
    string_builder_null_terminate(&sb_pdb_name);
    if (file_time(sb_pdb_name.str.data) > 0) {
        file_delete(sb_pdb_name.str.data);
    }
#endif

    u64 time_build_h   = file_time("build.h");
    u64 time_build_c   = file_time("build.c");
    u64 time_build_exe = file_time(exe_file);

    if (file_time_compare(time_build_h, time_build_exe) > 0 ||
        file_time_compare(time_build_c, time_build_exe) > 0) {
        printf(ANSI_GREEN
               "Rebuilding '%s' due to changes in build files.\n" ANSI_RESET,
               exe_file);

        // Create the new executable file name: <old-exe-file>.new
        StringBuilder sb_name = string_builder_init(&temp_arena);
        string_builder_append_zstring(&sb_name, exe_file);
        string_builder_append_zstring(&sb_name, ".new");
        string_builder_null_terminate(&sb_name);
        const char* new_exe_file = sb_name.str.data;

        // Generate the build process for the new executable
        StringBuilder sb         = string_builder_init(&temp_arena);
        string_builder_append_zstring(&sb, "clang --std=c23 -g -o ");
        string_builder_append_zstring(&sb, new_exe_file);
        string_builder_append_zstring(&sb, " build.c");
        string_builder_null_terminate(&sb);

        // Run the build command
        if (build_run(sb.str) != 0) {
            fprintf(stderr, "Build failed. Please check the output above.\n");
            arena_free(&temp_arena);
            exit(EXIT_FAILURE);
        }

        // Move the current exe to .old (this works even while running)
        file_rename(exe_file, sb_old_name.str.data);

        // Move new exe to final location
        file_rename(new_exe_file, exe_file);

        // Rerun the build process again.
        StringBuilder sb_rerun = string_builder_init(&temp_arena);
        string_builder_append_zstring(&sb_rerun, exe_file);
        for (int i = 1; i < argc; ++i) {
            string_builder_append_zstring(&sb_rerun, " ");
            string_builder_append_zstring(&sb_rerun, argv[i]);
        }
        string_builder_null_terminate(&sb_rerun);
        if (build_run(sb_rerun.str) != 0) {
            fprintf(stderr,
                    "Rerun build failed. Please check the output above.\n");
            exit(EXIT_FAILURE);
        } else {
            exit(EXIT_SUCCESS);
        }

        arena_free(&temp_arena);
    }
}

typedef enum {
    Platform_Windows,
    Platform_Linux,
    Platform_MacOS,
    Platform_Unknown
} Platform;

Platform build_platform()
{
#ifdef _WIN32
    return Platform_Windows;
#elif defined(__linux__)
    return Platform_Linux;
#elif defined(__APPLE__)
    return Platform_MacOS;
#else
    return Platform_Unknown;
#endif
}
