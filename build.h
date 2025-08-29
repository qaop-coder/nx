#pragma once

#define _CRT_SECURE_NO_WARNINGS

#define KORE_IMPLEMENTATION
#include "3rd/kore/kore.h"

#include <stdbool.h>
#include <string.h>

#if KORE_OS_WINDOWS
#    include <sys/stat.h>
#else
#    include <dirent.h>
#    include <sys/stat.h>
#endif

//
// Basic types
//

typedef struct {
    const char* data;
    size_t      length;
} String;

#define STRINGV(s) (int)(s).length, (s).data

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
    void* buffer = KORE_ALLOC(ARENA_PAGE_SIZE);
    Arena arena  = {.buffer = buffer, .cursor = 0};
    return arena;
}

void arena_free(Arena* arena)
{
    if (arena->buffer) {
        KORE_FREE(arena->buffer);
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
    while ($.memory_size(arena->buffer) < arena->cursor + size) {
        size_t new_size = $.memory_size(arena->buffer) + ARENA_PAGE_SIZE;
        arena->buffer   = KORE_REALLOC(arena->buffer, new_size);
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
#if KORE_OS_WINDOWS
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
#if KORE_OS_WINDOWS
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
                                 KArray(String) * files,
                                 const char* directory,
                                 bool        recursive)
{
#if KORE_OS_WINDOWS
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
                    array_add(*files, file_sb.str);
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
                        array_add(*files, file_sb.str);
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
#endif
}

KArray(String) files_list(Arena* arena, const char* directory, bool recursive)
{
    KArray(String) files = nullptr;
    files_list_recursive(arena, &files, directory, recursive);
    return files;
}

//
// Build system
//

i32 build_run(String command)
{
    printf("Running command: %.*s\n", STRINGV(command));

    char* command_buffer = KORE_ARRAY_ALLOC(char, command.length + 1);
    memcpy(command_buffer, command.data, command.length);
    command_buffer[command.length] = '\0';

#if KORE_OS_WINDOWS
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

i32 build_run_capture(String command,
                      KArray(String) * output_lines,
                      Arena* arena)
{
    char* command_buffer = KORE_ARRAY_ALLOC(char, command.length + 1);
    memcpy(command_buffer, command.data, command.length);
    command_buffer[command.length] = '\0';

    i32 exit_code = 1;

#if KORE_OS_WINDOWS
    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdout_read, stdout_write;
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        fprintf(stderr, "Failed to create pipe for command output\n");
        goto cleanup;
    }

    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA        si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb         = sizeof(si);
    si.hStdError  = stdout_write;
    si.hStdOutput = stdout_write;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    BOOL success = CreateProcessA(
        NULL, (LPSTR)command_buffer, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    CloseHandle(stdout_write);

    if (success) {
        // Read output line by line
        char          buffer[4096];
        DWORD         bytes_read;
        StringBuilder current_line = string_builder_init(arena);

        while (
            ReadFile(
                stdout_read, buffer, sizeof(buffer) - 1, &bytes_read, NULL) &&
            bytes_read > 0) {
            buffer[bytes_read] = '\0';

            for (DWORD i = 0; i < bytes_read; i++) {
                if (buffer[i] == '\n') {
                    string_builder_null_terminate(&current_line);
                    array_add(*output_lines, current_line.str);
                    current_line = string_builder_init(arena);
                } else if (buffer[i] != '\r') {
                    char* ch = string_builder_alloc(&current_line, 1);
                    *ch      = buffer[i];
                }
            }
        }

        // Add final line if not empty
        if (current_line.str.length > 0) {
            string_builder_null_terminate(&current_line);
            array_add(*output_lines, current_line.str);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, (LPDWORD)&exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    CloseHandle(stdout_read);

#else
    FILE* pipe = popen(command_buffer, "r");
    if (!pipe) {
        fprintf(stderr, "Failed to run command: %.*s\n", STRINGV(command));
        goto cleanup;
    }

    char line_buffer[4096];
    while (fgets(line_buffer, sizeof(line_buffer), pipe)) {
        // Remove trailing newline
        size_t len = strlen(line_buffer);
        if (len > 0 && line_buffer[len - 1] == '\n') {
            line_buffer[len - 1] = '\0';
        }

        // Copy line to arena and add to array
        StringBuilder sb = string_builder_init(arena);
        string_builder_append_zstring(&sb, line_buffer);
        string_builder_null_terminate(&sb);
        array_add(*output_lines, sb.str);
    }

    exit_code = pclose(pipe);
#endif

cleanup:
    KORE_ARRAY_FREE(command_buffer);
    return exit_code;
}

//
// C compilation support
//

typedef struct {
    Arena* arena;
    KArray(String) files;
    KArray(String) libraries;
    KArray(String) include_paths;
    bool   debug;
    String output_file;
    String output_folder;
} CompileInfo;

CompileInfo compile_info_init(Arena* arena, const char* output_file)
{
    CompileInfo info;
    info.arena         = arena;
    info.files         = nullptr;
    info.libraries     = nullptr;
    info.include_paths = nullptr;
    info.debug         = false;

    StringBuilder sb   = string_builder_init(arena);
    string_builder_append_zstring(&sb, output_file);
#if KORE_OS_WINDOWS
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
    array_add(info->files, sb.str);
}

void compile_info_add_library(CompileInfo* info, const char* library)
{
    StringBuilder sb = string_builder_init(info->arena);
    string_builder_append_zstring(&sb, library);
    array_add(info->libraries, sb.str);
}

void compile_info_add_libraries(CompileInfo* info, KArray(const char*) libs)
{
    usize num_libs = array_length(libs);
    for (usize i = 0; i < num_libs; ++i) {
        compile_info_add_library(info, libs[i]);
    }
}

void compile_info_add_include_path(CompileInfo* info, const char* path)
{
    StringBuilder sb = string_builder_init(info->arena);
    string_builder_append_zstring(&sb, path);
    array_add(info->include_paths, sb.str);
}

void compile_info_add_folder(CompileInfo* info,
                             const char*  folder,
                             bool         recursive)
{
    KArray(String) files = files_list(info->arena, folder, recursive);
    for (usize i = 0; i < array_length(files); ++i) {
        if (string_ends_with_zstring(files[i], ".c")) {
            // Only add C/C++ source files
            array_add(info->files, files[i]);
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
        string_builder_append_zstring(&sb, " -g -DDEBUG");
    }

    for (usize i = 0; i < array_length(info->include_paths); ++i) {
        string_builder_append_zstring(&sb, " -I");
        string_builder_append_string(&sb, info->include_paths[i]);
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

//
// Build checker
//

void build_check(int argc, char** argv)
{
    $.init();

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

#if KORE_OS_WINDOWS
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
        $.prn(KORE_ANSI_GREEN
              "Rebuilding '%s' due to changes in build files." KORE_ANSI_RESET,
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
#if KORE_OS_WINDOWS
    return Platform_Windows;
#elif KORE_OS_LINUX
    return Platform_Linux;
#elif KORE_OS_MACOS
    return Platform_MacOS;
#else
    return Platform_Unknown;
#endif
}