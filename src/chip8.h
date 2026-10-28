#ifndef CHIP8_H
#define CHIP8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    CHIP8_MEMORY_SIZE = 4096,
    CHIP8_ROM_START = 0x200,
    CHIP8_WIDTH = 64,
    CHIP8_HEIGHT = 32,
    CHIP8_FONT_START = 0x50
};

typedef struct {
    uint8_t memory[CHIP8_MEMORY_SIZE];
    uint8_t v[16];
    uint16_t i, pc;
    uint16_t stack[16];
    uint8_t sp, delay_timer, sound_timer;
    uint8_t pixels[CHIP8_HEIGHT][CHIP8_WIDTH];
    bool keys[16];
    bool waiting;
    uint8_t wait_register;
    int wait_key;
    uint32_t random_state;
    const char *error;
} Chip8;

void chip8_init(Chip8 *cpu);
bool chip8_load_rom(Chip8 *cpu, const char *path);
bool chip8_step(Chip8 *cpu);
void chip8_tick_timers(Chip8 *cpu);
void chip8_set_key(Chip8 *cpu, unsigned key, bool down);

#endif
