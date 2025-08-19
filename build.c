#include "build.h"

int main(int argc, char** argv)
{
    build_check(argc, argv);

    bool run = false;
    if (argc > 1 && strcmp(argv[1], "run") == 0) {
        run = true;
    }

    const char** libraries;

    switch (build_platform()) {
    case Platform_Windows:
        libraries = (const char*[]){"user32", "gdi32", 0};
        break;
    case Platform_Linux:
        libraries = (const char*[]){"X11", 0};
        break;
    case Platform_MacOS:
        libraries = (const char*[]){"Cocoa", 0};
        break;
    default:
        fprintf(stderr, "Unsupported platform.\n");
        return EXIT_FAILURE;
    }
    if (compile_project("nx", "src", libraries, "_bin")) {
        fprintf(stderr, "Compilation failed. Please check the output above.\n");
        return EXIT_FAILURE;
    }

    if (run) {
        String exe_file = string_view("_bin/nx");
        if (build_run(exe_file) != 0) {
            fprintf(stderr,
                    "Failed to run the executable. Please check the output "
                    "above.\n");
            return EXIT_FAILURE;
        }
    }

    return 0;
}
