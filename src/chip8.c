#include "chip8.h"

#include <stdio.h>
#include <string.h>

static const uint8_t font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x90, 0xF0, 0x90, 0x90, 0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80
};

static bool fail(Chip8 *cpu, const char *message)
{
    cpu->error = message;
    return false;
}

static bool memory_range(Chip8 *cpu, size_t count)
{
    if (cpu->i >= CHIP8_MEMORY_SIZE || count > (size_t)(CHIP8_MEMORY_SIZE - cpu->i))
        return fail(cpu, "memory access outside 4096 bytes");
    return true;
}

void chip8_init(Chip8 *cpu)
{
    memset(cpu, 0, sizeof *cpu);
    memcpy(cpu->memory + CHIP8_FONT_START, font, sizeof font);
    cpu->pc = CHIP8_ROM_START;
    cpu->wait_key = -1;
    cpu->random_state = 0x6D2B79F5;
}

bool chip8_load_rom(Chip8 *cpu, const char *path)
{
    uint8_t rom[CHIP8_MEMORY_SIZE - CHIP8_ROM_START];
    FILE *file = fopen(path, "rb");
    if (!file)
        return fail(cpu, "could not open ROM");

    size_t size = fread(rom, 1, sizeof rom, file);
    int extra = fgetc(file);
    bool read_error = ferror(file) != 0;
    int close_result = fclose(file);
    if (read_error || close_result != 0)
        return fail(cpu, "could not read ROM");
    if (size == 0)
        return fail(cpu, "ROM is empty");
    if (extra != EOF)
        return fail(cpu, "ROM exceeds 3584 bytes");

    chip8_init(cpu);
    memcpy(cpu->memory + CHIP8_ROM_START, rom, size);
    return true;
}

void chip8_set_key(Chip8 *cpu, unsigned key, bool down)
{
    if (key >= 16 || cpu->keys[key] == down)
        return;
    cpu->keys[key] = down;
    if (!cpu->waiting)
        return;
    /* Fx0A waits for a fresh press and release. */
    if (down && cpu->wait_key < 0)
        cpu->wait_key = (int)key;
    else if (!down && cpu->wait_key == (int)key) {
        cpu->v[cpu->wait_register] = (uint8_t)key;
        cpu->waiting = false;
        cpu->wait_key = -1;
    }
}

void chip8_tick_timers(Chip8 *cpu)
{
    if (cpu->delay_timer)
        --cpu->delay_timer;
    if (cpu->sound_timer)
        --cpu->sound_timer;
}

bool chip8_step(Chip8 *cpu)
{
    if (cpu->error)
        return false;
    if (cpu->waiting)
        return true;
    if (cpu->pc > CHIP8_MEMORY_SIZE - 2)
        return fail(cpu, "instruction fetch outside memory");

    uint16_t opcode = (uint16_t)((cpu->memory[cpu->pc] << 8) |
                                cpu->memory[cpu->pc + 1]);
    unsigned x = (opcode >> 8) & 0xF;
    unsigned y = (opcode >> 4) & 0xF;
    unsigned n = opcode & 0xF;
    uint8_t byte = (uint8_t)opcode;
    uint16_t address = opcode & 0xFFF;
    uint8_t vx = cpu->v[x], vy = cpu->v[y];
    cpu->pc += 2;

    switch (opcode >> 12) {
    case 0x0:
        if (opcode == 0x00E0)
            memset(cpu->pixels, 0, sizeof cpu->pixels);
        else if (opcode == 0x00EE) {
            if (cpu->sp == 0)
                return fail(cpu, "stack underflow");
            cpu->pc = cpu->stack[--cpu->sp];
        } else
            return fail(cpu, "unsupported opcode (including 0nnn SYS)");
        break;
    case 0x1: cpu->pc = address; break;
    case 0x2:
        if (cpu->sp >= 16)
            return fail(cpu, "stack overflow");
        cpu->stack[cpu->sp++] = cpu->pc;
        cpu->pc = address;
        break;
    case 0x3: if (vx == byte) cpu->pc += 2; break;
    case 0x4: if (vx != byte) cpu->pc += 2; break;
    case 0x5:
        if (n != 0) return fail(cpu, "unsupported opcode");
        if (vx == vy) cpu->pc += 2;
        break;
    case 0x6: cpu->v[x] = byte; break;
    case 0x7: cpu->v[x] = (uint8_t)(vx + byte); break;
    case 0x8:
        /* Use the saved operands; either register may be VF. */
        switch (n) {
        case 0x0: cpu->v[x] = vy; break;
        case 0x1: cpu->v[x] = vx | vy; cpu->v[0xF] = 0; break;
        case 0x2: cpu->v[x] = vx & vy; cpu->v[0xF] = 0; break;
        case 0x3: cpu->v[x] = vx ^ vy; cpu->v[0xF] = 0; break;
        case 0x4:
            cpu->v[x] = (uint8_t)(vx + vy);
            cpu->v[0xF] = (unsigned)vx + vy > 255;
            break;
        case 0x5:
            cpu->v[x] = (uint8_t)(vx - vy);
            cpu->v[0xF] = vx >= vy;
            break;
        case 0x6: cpu->v[x] = vy >> 1; cpu->v[0xF] = vy & 1; break;
        case 0x7:
            cpu->v[x] = (uint8_t)(vy - vx);
            cpu->v[0xF] = vy >= vx;
            break;
        case 0xE: cpu->v[x] = (uint8_t)(vy << 1); cpu->v[0xF] = vy >> 7; break;
        default: return fail(cpu, "unsupported opcode");
        }
        break;
    case 0x9:
        if (n != 0) return fail(cpu, "unsupported opcode");
        if (vx != vy) cpu->pc += 2;
        break;
    case 0xA: cpu->i = address; break;
    case 0xB: cpu->pc = address + cpu->v[0]; break;
    case 0xC:
        cpu->random_state ^= cpu->random_state << 13;
        cpu->random_state ^= cpu->random_state >> 17;
        cpu->random_state ^= cpu->random_state << 5;
        cpu->v[x] = (uint8_t)cpu->random_state & byte;
        break;
    case 0xD: {
        if (n == 0) return fail(cpu, "unsupported 16x16 sprite opcode");
        if (!memory_range(cpu, n)) return false;
        unsigned start_x = vx % CHIP8_WIDTH, start_y = vy % CHIP8_HEIGHT;
        cpu->v[0xF] = 0;
        for (unsigned row = 0; row < n && start_y + row < CHIP8_HEIGHT; ++row) {
            uint8_t sprite = cpu->memory[cpu->i + row];
            for (unsigned bit = 0; bit < 8 && start_x + bit < CHIP8_WIDTH; ++bit) {
                if ((sprite & (0x80u >> bit)) != 0) {
                    uint8_t *pixel = &cpu->pixels[start_y + row][start_x + bit];
                    cpu->v[0xF] |= *pixel;
                    *pixel ^= 1;
                }
            }
        }
        break;
    }
    case 0xE:
        if (byte != 0x9E && byte != 0xA1)
            return fail(cpu, "unsupported opcode");
        if (vx >= 16) return fail(cpu, "key index outside keypad");
        if ((byte == 0x9E && cpu->keys[vx]) || (byte == 0xA1 && !cpu->keys[vx]))
            cpu->pc += 2;
        break;
    case 0xF:
        switch (byte) {
        case 0x07: cpu->v[x] = cpu->delay_timer; break;
        case 0x0A:
            cpu->waiting = true;
            cpu->wait_register = (uint8_t)x;
            cpu->wait_key = -1;
            break;
        case 0x15: cpu->delay_timer = vx; break;
        case 0x18: cpu->sound_timer = vx; break;
        case 0x1E: cpu->i = (uint16_t)(cpu->i + vx); break;
        case 0x29:
            if (vx > 0xF) return fail(cpu, "font digit outside hexadecimal range");
            cpu->i = CHIP8_FONT_START + 5 * vx;
            break;
        case 0x33:
            if (!memory_range(cpu, 3)) return false;
            cpu->memory[cpu->i] = vx / 100;
            cpu->memory[cpu->i + 1] = (vx / 10) % 10;
            cpu->memory[cpu->i + 2] = vx % 10;
            break;
        case 0x55:
        case 0x65:
            if (!memory_range(cpu, x + 1)) return false;
            if (byte == 0x55) memcpy(cpu->memory + cpu->i, cpu->v, x + 1);
            else memcpy(cpu->v, cpu->memory + cpu->i, x + 1);
            cpu->i += x + 1;
            break;
        default: return fail(cpu, "unsupported opcode");
        }
        break;
    }
    return true;
}
