//------------------------------------------------------------------------------
// Core definitions
//------------------------------------------------------------------------------

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

//------------------------------------------------------------------------------
// OS determination
//------------------------------------------------------------------------------

#define YES (1)
#define NO (0)

#define OS_WINDOWS NO
#define OS_LINUX NO
#define OS_MACOS NO

#if defined(_WIN32) || defined(_WIN64)
#    undef OS_WINDOWS
#    define OS_WINDOWS YES
#elif defined(__linux__)
#    undef OS_LINUX
#    define OS_LINUX YES
#elif defined(__APPLE__) || defined(__MACH__)
#    undef OS_MACOS
#    define OS_MACOS YES
#else
#    error "Unsupported operating system"
#endif

#if OS_WINDOWS
// #    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#elif OS_LINUX
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>
#endif

//------------------------------------------------------------------------------
// Basic types
//------------------------------------------------------------------------------

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float  f32;
typedef double f64;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define CHECK_MEMORY(ptr)                                                      \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            fprintf(stderr,                                                    \
                    "Memory allocation failed at %s:%d\n",                     \
                    __FILE__,                                                  \
                    __LINE__);                                                 \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

#if OS_WINDOWS
#    define BREAK()                                                            \
        if (IsDebuggerPresent())                                               \
            __debugbreak();
#elif OS_LINUX
#    define BREAK()                                                            \
        if is_debbuger_present ()                                              \
            __builtin_trap();
#else
#    define BREAK() ((void)0)
#endif

//------------------------------------------------------------------------------
// Locking primitives for thread safety
//------------------------------------------------------------------------------

#if OS_WINDOWS
typedef CRITICAL_SECTION Lock;
#elif OS_LINUX
typedef pthread_mutex_t Lock;
#endif

void lock_init(Lock* lock);
void lock_done(Lock* lock);
void lock_acquire(Lock* lock);
void lock_release(Lock* lock);

//------------------------------------------------------------------------------
// Dynamic memory allocation macros and functions
//------------------------------------------------------------------------------

void* memory_alloc(u64 size, const char* file, int line);
void* memory_realloc(void* ptr, size_t new_size, const char* file, int line);
void* memory_free(void* ptr, const char* file, int line);
void  memory_leak(void* ptr);
void  memory_dump();

void memory_break_on(u64 index);

u64 memory_size(void* ptr);

#define ALLOC(size) memory_alloc(size, __FILE__, __LINE__)
#define REALLOC(ptr, new_size) memory_realloc(ptr, new_size, __FILE__, __LINE__)
#define FREE(ptr) memory_free(ptr, __FILE__, __LINE__)

#define ALLOC_ARRAY(type, count)                                               \
    (type*)memory_alloc(sizeof(type) * (count), __FILE__, __LINE__)
#define REALLOC_ARRAY(ptr, type, new_count)                                    \
    (type*)memory_realloc(ptr, sizeof(type) * (new_count), __FILE__, __LINE__)
#define FREE_ARRAY(ptr) memory_free(ptr, __FILE__, __LINE__)

//------------------------------------------------------------------------------
// Dynamic arrays
//------------------------------------------------------------------------------

typedef struct ArrayInfo_t {
    u64 count;
} ArrayInfo;

#define Array(T) T*

#define __array_info(arr) ((ArrayInfo*)(arr) - 1)
#define __array_capacity(arr) (memory_size(__array_info(arr)) / sizeof(*(arr)))
#define __array_length(arr) (__array_info(arr)->count)

void* __array_maybe_grow(void* arr, u64 element_size, u64 new_capacity);

#define array_length(arr) ((arr) ? __array_length(arr) : 0)
#define array_push(arr, value)                                                 \
    arr = (typeof(arr))__array_maybe_grow(arr, sizeof(*(arr)), 1),             \
    (arr)[__array_length(arr)] = (value), __array_length(arr) += 1
#define array_free(arr)                                                        \
    (arr) = memory_free(__array_info(arr), __FILE__, __LINE__)

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// I M P L E M E N T A T I O N
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#ifdef CORE_IMPLEMENTATION

bool is_debugger_present()
{
#    if OS_WINDOWS
    return IsDebuggerPresent() != 0;
#    elif OS_LINUX
    // Check if a debugger is attached (Linux)
    FILE* status_file = fopen("/proc/self/status", "r");
    if (!status_file) {
        return false;
    }

    char line[256];
    while (fgets(line, sizeof(line), status_file)) {
        if (strncmp(line, "TracerPid:", 10) == 0) {
            fclose(status_file);
            return atoi(line + 10) != 0;
        }
    }
    fclose(status_file);
    return false;
#    else
#        error "Implement is_debugger_present for your OS"
#    endif // OS_LINUX
}

void* __array_maybe_grow(void* arr, u64 element_size, u64 new_capacity)
{
    if (!arr) {
        ArrayInfo* new_arr = (ArrayInfo*)memory_alloc(
            sizeof(ArrayInfo) + element_size * new_capacity,
            __FILE__,
            __LINE__);
        new_arr->count = 0;
        return new_arr + 1;
    }

    u64 current_capacity = __array_capacity(arr);
    if (new_capacity <= current_capacity) {
        return arr; // No need to grow
    }

    ArrayInfo* info    = __array_info(arr);
    ArrayInfo* new_arr = (ArrayInfo*)memory_realloc(
        info,
        sizeof(ArrayInfo) + element_size * new_capacity,
        __FILE__,
        __LINE__);
    new_arr->count = info->count;

    return new_arr + 1;
}
//------------------------------------------------------------------------------
// Locking primitives implementation
//------------------------------------------------------------------------------

#    if OS_WINDOWS

void lock_init(Lock* lock) { InitializeCriticalSection(lock); }

void lock_done(Lock* lock) { DeleteCriticalSection(lock); }

void lock_acquire(Lock* lock) { EnterCriticalSection(lock); }

void lock_release(Lock* lock) { LeaveCriticalSection(lock); }

#    elif OS_LINUX

void lock_init(Lock* lock) { pthread_mutex_init(lock, NULL); }

void lock_done(Lock* lock) { pthread_mutex_destroy(lock); }

void lock_acquire(Lock* lock) { pthread_mutex_lock(lock); }

void lock_release(Lock* lock) { pthread_mutex_unlock(lock); }

#    else
#        error "Unsupported OS for locking primitives"
#    endif

//------------------------------------------------------------------------------
// Implementation of dynamic memory allocation functions
//------------------------------------------------------------------------------

typedef struct MemoryInfo_t {
    size_t size;

#    if DEBUG
    const char*          file;
    int                  line;
    u64                  index;
    struct MemoryInfo_t* next;
    struct MemoryInfo_t* prev;
#    endif // DEBUG
} MemoryInfo;

#    if DEBUG

Lock        g_memory_lock;
bool        g_memory_initialised = false;
MemoryInfo* g_memory_head        = nullptr;
u64         g_memory_index       = 1;
u64         g_memory_break_index = 0;

static void _memory_lock()
{
    if (!g_memory_initialised) {
        lock_init(&g_memory_lock);
        g_memory_initialised = true;
    }
    lock_acquire(&g_memory_lock);
}

static void _memory_unlock() { lock_release(&g_memory_lock); }

#    endif // DEBUG

void* memory_alloc(u64 size, const char* file, int line)
{
#    if DEBUG
    if (g_memory_break_index > 0 && g_memory_index == g_memory_break_index) {
        BREAK();
    }
#    endif // DEBUG

    if (size == 0) {
        return nullptr;
    }

    MemoryInfo* info = (MemoryInfo*)malloc(sizeof(MemoryInfo) + size);
    if (!info) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    info->size = size;

#    if DEBUG
    _memory_lock();
    info->file  = file;
    info->line  = line;
    info->index = g_memory_index++;
    info->next  = g_memory_head;
    info->prev  = nullptr;

    if (g_memory_head) {
        g_memory_head->prev = info;
    }

    g_memory_head = info;
    _memory_unlock();

#    endif // DEBUG

    return (void*)(info + 1);
}

void* memory_realloc(void* ptr, size_t new_size, const char* file, int line)
{
#    if _DEBUG
    if (g_memory_break_index > 0 && g_memory_index == g_memory_break_index) {
        BREAK();
    }
#    endif // _DEBUG

    if (new_size == 0) {
        free(ptr);
        return nullptr;
    }

    if (!ptr) {
        return memory_alloc(new_size, file, line);
    }

    MemoryInfo* info = (MemoryInfo*)ptr - 1;

    // Remove from linked list if debug mode
#    if DEBUG
    _memory_lock();
    if (info->prev) {
        info->prev->next = info->next;
    } else {
        g_memory_head = info->next;
    }
    if (info->next) {
        info->next->prev = info->prev;
    }
    _memory_unlock();
#    endif // DEBUG

    info = (MemoryInfo*)realloc(info, sizeof(MemoryInfo) + new_size);
    if (!info) {
        fprintf(stderr, "Memory reallocation failed\n");
        exit(EXIT_FAILURE);
    }

    info->size = new_size;

#    if DEBUG
    _memory_lock();
    info->file  = file;
    info->line  = line;
    info->index = g_memory_index++;
    info->next  = g_memory_head;
    info->prev  = nullptr;
    if (g_memory_head) {
        g_memory_head->prev = info;
    }
    g_memory_head = info;
    _memory_unlock();
#    endif // DEBUG

    return (void*)(info + 1);
}

void* memory_free(void* ptr, const char* file, int line)
{
    if (!ptr) {
        return nullptr;
    }

    MemoryInfo* info = (MemoryInfo*)ptr - 1;

#    if DEBUG
    _memory_lock();
    if (info->prev) {
        info->prev->next = info->next;
    } else {
        g_memory_head = info->next;
    }
    if (info->next) {
        info->next->prev = info->prev;
    }
    _memory_unlock();
#    endif // DEBUG

    free(info);
    return nullptr;
}

void memory_leak(void* ptr)
{
    if (!ptr) {
        return;
    }

    MemoryInfo* info = (MemoryInfo*)ptr - 1;
#    if DEBUG
    _memory_lock();
    if (info->prev) {
        info->prev->next = info->next;
    } else {
        g_memory_head = info->next;
    }
    if (info->next) {
        info->next->prev = info->prev;
    }
    _memory_unlock();
#    endif // DEBUG
}

void memory_dump()
{
    printf("\nMemory Dump:\n");
    printf(
        "-----------------------------------------------------------------\n");
#    if DEBUG
    _memory_lock();
    MemoryInfo* current = g_memory_head;
    if (!current) {
        printf("No memory allocations found.\n");
        _memory_unlock();
        return;
    }

    printf("Index           Address              Size     File:Line\n");
    printf(
        "-----------------------------------------------------------------\n");
    while (current) {
        printf("%-15zu %-20p %-8zu %s:%d\n",
               current->index,
               (void*)(current + 1),
               current->size,
               current->file,
               current->line);
        current = current->next;
    }
    _memory_unlock();
#    endif // DEBUG
}

#    if DEBUG
void memory_break_on(u64 index) { g_memory_break_index = index; }
#    endif

u64 memory_size(void* ptr)
{
    if (!ptr) {
        return 0;
    }

    MemoryInfo* info = (MemoryInfo*)ptr - 1;
    return info->size;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#endif // CORE_IMPLEMENTATION
