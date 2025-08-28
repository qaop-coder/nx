#define KORE_IMPLEMENTATION
#include "kore.h"

#include "config.h"
#include "frame.h"
#include "memory.h"

#include <math.h>

#define WINDOW_SCALE 3

int main(int argc, char** argv)
{
    $.init();
    $.memory_break_on(5);

    Memory memory = {0};
    mem_init(&memory);

    Frame main_window = frame_open(WINDOW_WIDTH * WINDOW_SCALE,
                                   WINDOW_HEIGHT * WINDOW_SCALE,
                                   "Nx (Dev.9)");

    u32* screen  = frame_add_layer(&main_window, WINDOW_WIDTH, WINDOW_HEIGHT);
    u32* overlay = frame_add_layer(&main_window, WINDOW_WIDTH, WINDOW_HEIGHT);

    mem_load_file(&memory, 0x0000, "etc/roms/48.rom");
    mem_load_file(&memory, 0x4000, "etc/screens/AticAtac.scr");

    while (frame_loop(&main_window)) {
        static unsigned frame = 0;

        // Background layer: animated color pattern (fully opaque)
        for (int y = 0; y < WINDOW_HEIGHT; ++y) {
            u32* row = &screen[y * WINDOW_WIDTH];
            for (int x = 0; x < WINDOW_WIDTH; ++x) {
                unsigned r = (x + frame) & 0xFF;
                unsigned g = (y + frame) & 0xFF;
                unsigned b = ((x ^ y) + frame) & 0xFF;
                row[x]     = 0xFF000000u | (r << 16) | (g << 8) |
                         b; // ARGB - fully opaque
            }
        }

        // Overlay layer: test alpha blending with animated circles
        for (int y = 0; y < WINDOW_HEIGHT; ++y) {
            u32* row = &overlay[y * WINDOW_WIDTH];
            for (int x = 0; x < WINDOW_WIDTH; ++x) {
                // Create animated circles with varying alpha
                int cx1 = WINDOW_WIDTH / 3 + (int)(30.0 * sin(frame * 0.05));
                int cy1 = WINDOW_HEIGHT / 3 + (int)(20.0 * cos(frame * 0.03));
                int cx2 =
                    2 * WINDOW_WIDTH / 3 + (int)(25.0 * cos(frame * 0.04));
                int cy2 =
                    2 * WINDOW_HEIGHT / 3 + (int)(15.0 * sin(frame * 0.06));

                int dx1 = x - cx1, dy1 = y - cy1;
                int dx2 = x - cx2, dy2 = y - cy2;
                int dist1 = dx1 * dx1 + dy1 * dy1;
                int dist2 = dx2 * dx2 + dy2 * dy2;

                u32 alpha = 0x00000000; // Fully transparent by default

                // Red circle with 50% alpha
                if (dist1 < 40 * 40) {
                    alpha = 0x80FF0000; // 50% alpha red
                }
                // Blue circle with 75% alpha
                else if (dist2 < 30 * 30) {
                    alpha = 0xC0000000 | (0x0000FF); // 75% alpha blue
                }

                row[x] = alpha;
            }
        }

        frame++;

        // Calculate FPS every frame, but display every 60 frames
        f64 fps = frame_fps(&main_window);
        if (frame % 60 == 0) {
            printf("FPS: %.1f\n", fps);
        }
    }

    printf("Exiting...\n");

    frame_free_pixels(screen);
    frame_free_pixels(overlay);

    mem_done(&memory);
    $.done();
    return 0;
}
