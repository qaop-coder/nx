#include "build.h"

int main(int argc, char** argv)
{
    build_check(argc, argv);

    bool run = false;
    if (argc > 1 && strcmp(argv[1], "run") == 0) {
        run = true;
    }

    const char* libraries[] = {"gdi32", "user32", 0};
    compile_project("nx", "src", libraries, "_bin");

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
