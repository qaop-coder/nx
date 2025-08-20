# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a ZX Spectrum emulator/debugger/assembler/disassembler written in C23. It's a remake of an older Nx emulator, designed to help disassemble old ZX Spectrum games for conversion to the ZX Spectrum Next.

## Build System

The project uses a custom build system with `just` as the task runner:

- `just bootstrap` - Compile the build tool itself with clang
- `just build` - Build the main project executable 
- `just run` - Build and run the emulator
- `just clean` - Remove build artifacts

The build process:
1. First run `just bootstrap` to compile the `build` executable from `build.c`
2. The `build` executable compiles all C files in `src/` directory
3. Links platform-specific libraries (user32/gdi32/opengl32 on Windows, X11 on Linux)
4. Outputs to `_bin/nx` (or `_bin/nx.exe` on Windows)

The build system automatically detects file changes and rebuilds when needed.

## Architecture

### Core Components

- **Core System** (`src/core.h`) - Foundation with memory management, dynamic arrays, platform detection, and debug utilities
- **Memory Emulation** (`src/memory.h/.c`) - ZX Spectrum memory system emulation with peek/poke operations
- **Frame Management** (`src/frame.h`) - Cross-platform window management (Windows/Linux)
- **Graphics System** (`src/gfx.h/.c`) - OpenGL-based layered rendering with automatic scaling and letterboxing
- **Main Application** (`src/main.c`) - Entry point with basic rendering loop

### Memory Management

The codebase uses a custom memory tracking system:
- All allocations go through `ALLOC()`, `REALLOC()`, `FREE()` macros
- Debug builds track allocation locations and detect leaks
- Dynamic arrays implemented with `Array(type)` macro and `array_push()`, `array_free()`

### Graphics Architecture

Modern OpenGL-based rendering system:
- Multiple layers with RGBA pixel buffers
- Automatic aspect ratio preservation with letterboxing/pillarboxing
- Immediate-mode API for simple usage
- Cross-platform OpenGL context management

### Configuration

Display constants defined in `src/config.h`:
- Screen dimensions (256x192 pixels)
- TV dimensions (352x312 total with borders) 
- Window size (320x256 display area)
- Default scaling factor (3x)

## Platform Support

- **Windows** - Win32 API with OpenGL
- **Linux** - X11 with OpenGL
- **Build Requirements** - C23-compatible compiler (clang recommended)

## Development Workflow

1. Make changes to source files in `src/`
2. Run `just build` to compile
3. Run `just run` to test changes
4. The build system automatically rebuilds when source files change

## Test Resources

The `etc/` directory contains:
- ROM files for different ZX Spectrum models (48K, 128K, +2, +3)
- Test programs and screen files
- Assembly test code and expected outputs