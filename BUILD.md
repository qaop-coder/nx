# Build System Manual

This document provides a comprehensive guide for writing `build.c` files using the custom C build system built on the Kore library.

## Overview

The build system allows you to define your project's build configuration entirely in C code using a `build.c` file. This provides type safety, IDE support, and eliminates the need for external build configuration files.

## Basic Structure

A typical `build.c` file has this structure:

```c
#include "build.h"
#include "build_watch.h"

CompileInfo build_create_compile_info(Arena* arena)
{
    // Platform-specific libraries
    KArray(const char*) libraries = 0;
    switch (build_platform()) {
    case Platform_Windows:
        array_add(libraries, "user32");
        array_add(libraries, "gdi32");
        break;
    case Platform_Linux:
        array_add(libraries, "X11");
        break;
    case Platform_MacOS:
        array_add(libraries, "Cocoa");
        break;
    default:
        $.eprn("Unsupported platform.");
        exit(EXIT_FAILURE);
    }

    // Configure build
    CompileInfo info = compile_info_init(arena, "my_program");
    compile_info_output_folder(&info, "_bin");
    compile_info_debug(&info);
    compile_info_add_folder(&info, "src", true);
    compile_info_add_include_path(&info, "3rd/some_lib");
    compile_info_add_libraries(&info, libraries);
    compile_info_add_flags(&info, "-std=c23 -Wall -Wextra");

    return info;
}

// Standard main function and build modes
i32 build() {
    Arena global_arena = arena_init();
    CompileInfo info = build_create_compile_info(&global_arena);
    
    if (compile(&info) != 0) {
        $.eprn("Compilation failed. Please check the output above.");
        return EXIT_FAILURE;
    }
    return 0;
}

int main(int argc, char** argv) {
    build_check(argc, argv);
    
    bool run = false;
    bool watch = false;
    if (argc > 1 && strcmp(argv[1], "run") == 0) {
        run = true;
    }
    if (argc > 1 && strcmp(argv[1], "watch") == 0) {
        watch = true;
    }

    if (run) {
        if (build() != 0) return EXIT_FAILURE;
        String exe_file = string_view("_bin/my_program");
        if (build_run(exe_file) != 0) {
            $.eprn("Failed to run the executable. Please check the output above.");
            return EXIT_FAILURE;
        }
    } else if (watch) {
        return build_watch("src", build_create_compile_info);
    } else {
        return build();
    }
    return 0;
}
```

## CompileInfo API Reference

### Initialization

```c
CompileInfo compile_info_init(Arena* arena, const char* output_file)
```
Creates a new CompileInfo structure. The output file name should not include the `.exe` extension on Windows - it will be added automatically.

### Basic Configuration

```c
void compile_info_debug(CompileInfo* info)
```
Enables debug mode (adds `-g -DDEBUG` flags).

```c
void compile_info_output_folder(CompileInfo* info, const char* folder)
```
Sets the output directory for the compiled executable (default: current directory).

```c
void compile_info_add_flags(CompileInfo* info, const char* flags)
```
Sets compiler flags (e.g., `-std=c23 -Wall -Wextra`).

### Source Files

```c
void compile_info_add_file(CompileInfo* info, const char* file)
```
Adds a single source file to compile.

```c
void compile_info_add_folder(CompileInfo* info, const char* folder, bool recursive)
```
Adds all `.c` files in a folder. If `recursive` is true, includes subdirectories.

### Dependencies

```c
void compile_info_add_include_path(CompileInfo* info, const char* path)
```
Adds an include directory (equivalent to `-I` flag).

```c
void compile_info_add_library(CompileInfo* info, const char* library)
```
Adds a library to link against (equivalent to `-l` flag).

```c
void compile_info_add_libraries(CompileInfo* info, KArray(const char*) libs)
```
Adds multiple libraries from an array.

### File Watching Configuration

These settings affect the `build watch` command:

```c
void compile_info_add_watch_extension(CompileInfo* info, const char* extension)
```
Adds a file extension to monitor for changes (e.g., `.c`, `.h`). If no extensions are specified, defaults to `.c`, `.h`, `.cpp`, `.hpp`.

```c
void compile_info_add_ignore_pattern(CompileInfo* info, const char* pattern)
```
Adds a pattern to ignore during file watching. Files containing this pattern will not trigger rebuilds. If no patterns are specified, no files are ignored.

## Platform Detection

Use the `build_platform()` function to configure platform-specific settings:

```c
switch (build_platform()) {
case Platform_Windows:
    // Windows-specific libraries and flags
    array_add(libraries, "user32");
    array_add(libraries, "gdi32");
    array_add(libraries, "opengl32");
    break;
case Platform_Linux:
    // Linux-specific libraries
    array_add(libraries, "X11");
    array_add(libraries, "GL");
    break;
case Platform_MacOS:
    // macOS-specific libraries
    array_add(libraries, "Cocoa");
    array_add(libraries, "OpenGL");
    break;
default:
    $.eprn("Unsupported platform.");
    exit(EXIT_FAILURE);
}
```

## Build Modes

The build system supports three modes:

### Normal Build
```bash
./build
```
Compiles the project once and exits.

### Run Mode
```bash
./build run
```
Compiles the project and runs the executable if compilation succeeds.

### Watch Mode
```bash
./build watch
```
Continuously monitors source files for changes and rebuilds automatically. Provides an interactive TUI with:
- Real-time build status with spinner animations
- Error/warning navigation with arrow keys
- File context display showing source code around issues
- Timestamped build events

## File Watching Examples

### Basic Configuration (Default Behavior)
```c
// No watch configuration needed - monitors .c/.h/.cpp/.hpp by default
CompileInfo info = compile_info_init(arena, "my_app");
// ... other configuration
```

### Custom Extensions
```c
CompileInfo info = compile_info_init(arena, "my_app");
// Only watch specific extensions
compile_info_add_watch_extension(&info, ".c");
compile_info_add_watch_extension(&info, ".h");
compile_info_add_watch_extension(&info, ".glsl"); // Shader files
```

### Ignore Patterns
```c
CompileInfo info = compile_info_init(arena, "my_app");
// Ignore temporary and backup files
compile_info_add_ignore_pattern(&info, ".tmp");
compile_info_add_ignore_pattern(&info, ".bak");
compile_info_add_ignore_pattern(&info, "~");        // Vim backup files
compile_info_add_ignore_pattern(&info, ".swp");     // Vim swap files
```

## Memory Management

The build system uses arena-based memory management:

```c
Arena global_arena = arena_init();
CompileInfo info = build_create_compile_info(&global_arena);
// Arena automatically cleaned up when function exits
```

All strings and arrays added to CompileInfo are automatically managed by the arena. You don't need to manually free memory.

## Error Handling

The build system provides detailed error reporting:

```c
if (compile(&info) != 0) {
    $.eprn("Compilation failed. Please check the output above.");
    return EXIT_FAILURE;
}
```

In watch mode, errors and warnings are displayed with:
- File names and line numbers (clickable in most terminals)
- Color-coded severity (red for errors, yellow for warnings)
- Interactive navigation with arrow keys
- Source code context showing the problematic lines

## Advanced Features

### Custom Build Logic
You can add custom build steps before or after compilation:

```c
i32 build() {
    Arena global_arena = arena_init();
    
    // Custom pre-build step
    $.prn("Generating version header...");
    // ... custom code generation
    
    CompileInfo info = build_create_compile_info(&global_arena);
    
    if (compile(&info) != 0) {
        $.eprn("Compilation failed.");
        return EXIT_FAILURE;
    }
    
    // Custom post-build step
    $.prn("Copying assets...");
    // ... asset copying logic
    
    return 0;
}
```

### Debug Information
Use `compile_info_dump()` to inspect the current configuration:

```c
CompileInfo info = build_create_compile_info(&global_arena);
compile_info_dump(&info);
```

This will print all files, libraries, and settings for debugging purposes.

## Best Practices

1. **Platform Independence**: Always use `build_platform()` for platform-specific configuration
2. **Consistent Naming**: Use consistent executable names across platforms
3. **Modular Configuration**: Break complex configurations into helper functions
4. **Error Messages**: Provide clear error messages for unsupported platforms or missing dependencies
5. **Watch Configuration**: Only specify custom watch extensions/ignore patterns if the defaults don't work
6. **Memory Safety**: Let the arena handle memory management - don't manually manage strings

## Troubleshooting

### "Build tool is out of date"
Run `just bootstrap` to rebuild the build tool after changes to `build.h` or `build_watch.h`.

### File watching not working
- Ensure your CompileInfo callback function is correctly passed to `build_watch()`
- Check that file extensions are correctly specified with the dot (`.c` not `c`)
- Verify ignore patterns aren't too broad

### Compilation errors
- Use `compile_info_dump()` to inspect the configuration
- Check that all include paths and libraries are correctly specified
- Ensure platform-specific libraries are properly configured

### TUI display issues
- Ensure your terminal supports Unicode (for spinner animations)
- Check that ANSI colors are supported in your terminal
- Use Git Bash or WSL on Windows for best compatibility