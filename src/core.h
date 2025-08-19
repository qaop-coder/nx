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
