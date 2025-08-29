#include "build.h"
#include "build_watch.h"

i32 build()
{
    KArray(const char*) libraries = 0;

    switch (build_platform()) {
    case Platform_Windows:
        array_add(libraries, "user32");
        array_add(libraries, "gdi32");
        array_add(libraries, "opengl32");
        break;
    case Platform_Linux:
        array_add(libraries, "X11");
        break;
    case Platform_MacOS:
        array_add(libraries, "Cocoa");
        break;
    default:
        $.eprn("Unsupported platform.");
        return EXIT_FAILURE;
    }

    Arena global_arena = arena_init();

    CompileInfo info   = compile_info_init(&global_arena, "nx");
    compile_info_output_folder(&info, "_bin");
    compile_info_debug(&info);
    compile_info_add_folder(&info, "src", true);
    compile_info_add_include_path(&info, "3rd/kore");
    compile_info_add_libraries(&info, libraries);

    if (compile(&info) != 0) {
        $.eprn("Compilation failed. Please check the output above.");
        return EXIT_FAILURE;
    }

    return 0;
}

int main(int argc, char** argv)
{
    build_check(argc, argv);

    bool run   = false;
    bool watch = false;
    if (argc > 1 && strcmp(argv[1], "run") == 0) {
        run = true;
    }
    if (argc > 1 && strcmp(argv[1], "watch") == 0) {
        watch = true;
    }

    if (run) {
        if (build() != 0) {
            return EXIT_FAILURE;
        }

        String exe_file = string_view("_bin/nx");
        if (build_run(exe_file) != 0) {
            $.eprn(
                "Failed to run the executable. Please check the output above.");
            return EXIT_FAILURE;
        }
    } else if (watch) {
        return build_watch("src", build);
    } else {
        return build();
    }

    return 0;
}
