#include "chip8.h"
#include "display.h"
#include <SDL3/SDL_main.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { CPU_HZ = 700 };

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ROM.ch8\n", argv[0]);
        return EXIT_FAILURE;
    }
    Chip8 cpu;
    chip8_init(&cpu);
    if (!chip8_load_rom(&cpu, argv[1])) {
        fprintf(stderr, "%s: %s\n", argv[1], cpu.error);
        return EXIT_FAILURE;
    }
    cpu.random_state ^= (uint32_t)time(NULL);
    if (!cpu.random_state)
        cpu.random_state = 1;

    Display display;
    if (!display_init(&display)) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        display_close(&display);
        return EXIT_FAILURE;
    }

    int status = EXIT_SUCCESS;
    const double cpu_period = 1.0 / CPU_HZ, timer_period = 1.0 / 60;
    double now = (double)SDL_GetTicksNS() / 1e9;
    double next_cpu = now, next_timer = now + timer_period;
    double previous = now;
    bool redraw = true;
    while (display_events(&cpu)) {
        now = (double)SDL_GetTicksNS() / 1e9;
        /* Skip catch-up after a long pause. */
        if (now - previous > 0.25) {
            next_cpu = now;
            next_timer = now + timer_period;
        }
        previous = now;
        while (next_cpu <= now || next_timer <= now) {
            if (next_timer <= next_cpu) {
                chip8_tick_timers(&cpu);
                next_timer += timer_period;
                redraw = true;
            } else {
                uint16_t pc = cpu.pc;
                if (!chip8_step(&cpu)) {
                    fprintf(stderr, "CHIP-8 at 0x%03X: %s\n", (unsigned)pc, cpu.error);
                    status = EXIT_FAILURE;
                    goto cleanup;
                }
                next_cpu += cpu_period;
            }
        }
        if ((redraw && !display_draw(&display, &cpu)) ||
            !display_sound(&display, cpu.sound_timer != 0)) {
            fprintf(stderr, "SDL: %s\n", SDL_GetError());
            status = EXIT_FAILURE;
            break;
        }
        redraw = false;
        double deadline = next_cpu < next_timer ? next_cpu : next_timer;
        double remaining = deadline - (double)SDL_GetTicksNS() / 1e9;
        if (remaining > 0)
            SDL_DelayNS((Uint64)(remaining * 1e9));
    }

cleanup:
    display_close(&display);
    return status;
}
