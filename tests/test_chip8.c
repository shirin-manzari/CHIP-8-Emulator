#include "chip8.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void execute(Chip8 *cpu, uint16_t opcode)
{
    cpu->pc = CHIP8_ROM_START;
    cpu->memory[CHIP8_ROM_START] = (uint8_t)(opcode >> 8);
    cpu->memory[CHIP8_ROM_START + 1] = (uint8_t)opcode;
    assert(chip8_step(cpu));
}

static void test_init(void)
{
    Chip8 cpu;
    memset(&cpu, 0xFF, sizeof cpu);
    chip8_init(&cpu);
    assert(cpu.pc == 0x200 && cpu.i == 0 && cpu.sp == 0);
    assert(!cpu.error && !cpu.waiting && cpu.wait_key == -1);
    assert(cpu.delay_timer == 0 && cpu.sound_timer == 0);
    assert(cpu.random_state != 0);
    for (unsigned j = 0; j < 16; ++j)
        assert(cpu.v[j] == 0 && !cpu.keys[j] && cpu.stack[j] == 0);
    for (unsigned y = 0; y < CHIP8_HEIGHT; ++y)
        for (unsigned x = 0; x < CHIP8_WIDTH; ++x)
            assert(cpu.pixels[y][x] == 0);
    for (unsigned j = 0; j < CHIP8_MEMORY_SIZE; ++j)
        if (j < CHIP8_FONT_START || j >= CHIP8_FONT_START + 80)
            assert(cpu.memory[j] == 0);
    assert(cpu.memory[CHIP8_FONT_START] == 0xF0);
    assert(cpu.memory[CHIP8_FONT_START + 79] == 0x80);
}

static void test_rom(void)
{
    const char *path = "tests/test_rom.tmp";
    const uint8_t rom[] = {0x61, 0xAB, 0x71, 0x01};
    Chip8 cpu;
    chip8_init(&cpu);
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(rom, 1, sizeof rom, file) == sizeof rom);
    assert(fclose(file) == 0);
    cpu.v[2] = 99;
    assert(chip8_load_rom(&cpu, path));
    assert(cpu.pc == 0x200 && cpu.v[2] == 0);
    assert(memcmp(cpu.memory + 0x200, rom, sizeof rom) == 0);
    assert(chip8_step(&cpu) && cpu.v[1] == 0xAB);
    assert(chip8_step(&cpu) && cpu.v[1] == 0xAC && cpu.pc == 0x204);

    file = fopen(path, "wb");
    assert(file && fclose(file) == 0);
    assert(!chip8_load_rom(&cpu, path));
    assert(cpu.v[1] == 0xAC);
    file = fopen(path, "wb");
    assert(file);
    for (unsigned j = 0; j < CHIP8_MEMORY_SIZE - CHIP8_ROM_START; ++j)
        assert(fputc(0xAB, file) != EOF);
    assert(fclose(file) == 0);
    assert(chip8_load_rom(&cpu, path));
    assert(cpu.memory[4095] == 0xAB);
    file = fopen(path, "ab");
    assert(file && fputc(0, file) != EOF && fclose(file) == 0);
    assert(!chip8_load_rom(&cpu, path));
    assert(cpu.memory[4095] == 0xAB);
    assert(remove(path) == 0);
    assert(!chip8_load_rom(&cpu, path));
}

static void test_registers(void)
{
    Chip8 cpu;
    chip8_init(&cpu);
    execute(&cpu, 0x61FF);
    execute(&cpu, 0x7102);
    assert(cpu.v[1] == 1 && cpu.v[15] == 0);
    cpu.v[2] = 0xAA;
    execute(&cpu, 0x8120);
    assert(cpu.v[1] == 0xAA);
    const uint16_t logic[] = {0x8121, 0x8122, 0x8123};
    const uint8_t expected[] = {0xAF, 0x0A, 0xA5};
    for (unsigned j = 0; j < 3; ++j) {
        cpu.v[1] = 0x0F; cpu.v[2] = 0xAA; cpu.v[15] = 1;
        execute(&cpu, logic[j]);
        assert(cpu.v[1] == expected[j] && cpu.v[15] == 0);
    }

    for (unsigned a = 0; a < 256; ++a)
        for (unsigned b = 0; b < 256; ++b) {
            cpu.v[1] = (uint8_t)a; cpu.v[2] = (uint8_t)b;
            execute(&cpu, 0x8124);
            assert(cpu.v[1] == (uint8_t)(a + b) && cpu.v[15] == (a + b > 255));
            cpu.v[1] = (uint8_t)a;
            execute(&cpu, 0x8125);
            assert(cpu.v[1] == (uint8_t)(a - b) && cpu.v[15] == (a >= b));
            cpu.v[1] = (uint8_t)a;
            execute(&cpu, 0x8127);
            assert(cpu.v[1] == (uint8_t)(b - a) && cpu.v[15] == (b >= a));
        }
    cpu.v[1] = 0xFF; cpu.v[2] = 0x82;
    execute(&cpu, 0x8126);
    assert(cpu.v[1] == 0x41 && cpu.v[15] == 0 && cpu.v[2] == 0x82);
    execute(&cpu, 0x812E);
    assert(cpu.v[1] == 4 && cpu.v[15] == 1);
    cpu.v[1] = 255; cpu.v[15] = 1;
    execute(&cpu, 0x81F4);
    assert(cpu.v[1] == 0 && cpu.v[15] == 1);
    cpu.v[15] = 250; cpu.v[2] = 10;
    execute(&cpu, 0x8F24);
    assert(cpu.v[15] == 1);
    cpu.v[15] = 0x81;
    execute(&cpu, 0x8FF6);
    assert(cpu.v[15] == 1);
}

static void test_branches(void)
{
    Chip8 cpu;
    chip8_init(&cpu);
    cpu.v[1] = 7; cpu.v[2] = 7;
    execute(&cpu, 0x3107); assert(cpu.pc == 0x204);
    execute(&cpu, 0x3108); assert(cpu.pc == 0x202);
    execute(&cpu, 0x4108); assert(cpu.pc == 0x204);
    execute(&cpu, 0x4107); assert(cpu.pc == 0x202);
    execute(&cpu, 0x5120); assert(cpu.pc == 0x204);
    execute(&cpu, 0x9120); assert(cpu.pc == 0x202);
    cpu.v[2] = 8;
    execute(&cpu, 0x5120); assert(cpu.pc == 0x202);
    execute(&cpu, 0x9120); assert(cpu.pc == 0x204);
    execute(&cpu, 0x1345); assert(cpu.pc == 0x345);
    cpu.v[0] = 10;
    execute(&cpu, 0xB345); assert(cpu.pc == 0x34F);
    cpu.pc = 0x200;
    cpu.memory[0x200] = 0x22; cpu.memory[0x201] = 0x10;
    cpu.memory[0x210] = 0x22; cpu.memory[0x211] = 0x20;
    cpu.memory[0x220] = 0x00; cpu.memory[0x221] = 0xEE;
    cpu.memory[0x212] = 0x00; cpu.memory[0x213] = 0xEE;
    assert(chip8_step(&cpu) && cpu.pc == 0x210 && cpu.sp == 1);
    assert(chip8_step(&cpu) && cpu.pc == 0x220 && cpu.sp == 2);
    assert(chip8_step(&cpu) && cpu.pc == 0x212 && cpu.sp == 1);
    assert(chip8_step(&cpu) && cpu.pc == 0x202 && cpu.sp == 0);
    for (unsigned j = 0; j < 16; ++j) {
        execute(&cpu, 0x2340);
        assert(cpu.pc == 0x340 && cpu.sp == j + 1);
    }
    for (unsigned j = 16; j > 0; --j) {
        execute(&cpu, 0x00EE);
        assert(cpu.pc == 0x202 && cpu.sp == j - 1);
    }
}

static void test_memory(void)
{
    Chip8 cpu;
    chip8_init(&cpu);
    execute(&cpu, 0xA300);
    cpu.v[1] = 250; cpu.v[15] = 1;
    execute(&cpu, 0xF11E);
    assert(cpu.i == 0x3FA && cpu.v[15] == 1);
    cpu.i = 0xFFF;
    execute(&cpu, 0xF11E);
    assert(cpu.i == 0x10F9 && cpu.v[15] == 1);
    for (unsigned digit = 0; digit < 16; ++digit) {
        cpu.v[1] = (uint8_t)digit;
        execute(&cpu, 0xF129);
        assert(cpu.i == CHIP8_FONT_START + 5 * digit);
    }
    for (unsigned value = 0; value < 256; ++value) {
        cpu.i = 4093; cpu.v[1] = (uint8_t)value;
        execute(&cpu, 0xF133);
        assert(cpu.memory[4093] == value / 100);
        assert(cpu.memory[4094] == value / 10 % 10);
        assert(cpu.memory[4095] == value % 10 && cpu.i == 4093);
    }
    for (unsigned j = 0; j < 16; ++j) cpu.v[j] = (uint8_t)(j * 7);
    cpu.i = 4080;
    execute(&cpu, 0xFF55);
    assert(cpu.i == 4096);
    memset(cpu.v, 0, sizeof cpu.v);
    cpu.i = 4080;
    execute(&cpu, 0xFF65);
    assert(cpu.i == 4096);
    for (unsigned j = 0; j < 16; ++j) assert(cpu.v[j] == j * 7);
    cpu.i = 4095; cpu.v[0] = 123;
    execute(&cpu, 0xF055);
    assert(cpu.i == 4096 && cpu.memory[4095] == 123);
    cpu.i = 4095; cpu.v[0] = 0;
    execute(&cpu, 0xF065);
    assert(cpu.i == 4096 && cpu.v[0] == 123);
}

static void test_drawing(void)
{
    Chip8 cpu;
    chip8_init(&cpu);
    cpu.i = 0x300;
    cpu.memory[0x300] = 0xC0; cpu.memory[0x301] = 0x80;
    cpu.v[1] = 4; cpu.v[2] = 5;
    execute(&cpu, 0xD122);
    assert(cpu.pixels[5][4] == 1 && cpu.pixels[5][5] == 1);
    assert(cpu.pixels[6][4] == 1 && cpu.v[15] == 0);
    execute(&cpu, 0xD122);
    assert(cpu.pixels[5][4] == 0 && cpu.pixels[6][4] == 0 && cpu.v[15] == 1);
    cpu.v[1] = 127; cpu.v[2] = 63;
    execute(&cpu, 0xD122);
    assert(cpu.pixels[31][63] == 1 && cpu.pixels[31][0] == 0 && cpu.pixels[0][63] == 0);
    execute(&cpu, 0x00E0);
    for (unsigned y = 0; y < 32; ++y)
        for (unsigned x = 0; x < 64; ++x) assert(cpu.pixels[y][x] == 0);
    cpu.v[15] = 63; cpu.v[2] = 0;
    execute(&cpu, 0xDF21);
    assert(cpu.pixels[0][63] == 1 && cpu.v[15] == 0);
    cpu.i = 4095; cpu.memory[4095] = 0x80;
    execute(&cpu, 0xD121);
    assert(cpu.pixels[31][63] == 0);
    assert(cpu.pixels[0][63] == 0 && cpu.v[15] == 1);
}

static void test_keys_timers_random(void)
{
    Chip8 cpu;
    chip8_init(&cpu);
    cpu.v[1] = 3;
    execute(&cpu, 0xF115); execute(&cpu, 0xF118);
    chip8_tick_timers(&cpu);
    execute(&cpu, 0xF207);
    assert(cpu.v[2] == 2 && cpu.sound_timer == 2);
    chip8_set_key(&cpu, 5, true);
    cpu.v[1] = 5;
    execute(&cpu, 0xE19E); assert(cpu.pc == 0x204);
    execute(&cpu, 0xE1A1); assert(cpu.pc == 0x202);
    execute(&cpu, 0xF30A);
    assert(cpu.waiting && cpu.pc == 0x202);
    assert(chip8_step(&cpu) && cpu.pc == 0x202);
    chip8_set_key(&cpu, 5, false);
    assert(cpu.waiting);
    chip8_set_key(&cpu, 0, true);
    chip8_set_key(&cpu, 0, true);
    assert(cpu.waiting);
    chip8_set_key(&cpu, 2, true);
    chip8_set_key(&cpu, 2, false);
    assert(cpu.waiting);
    chip8_tick_timers(&cpu); chip8_tick_timers(&cpu); chip8_tick_timers(&cpu);
    assert(cpu.delay_timer == 0 && cpu.sound_timer == 0);
    chip8_set_key(&cpu, 0, false);
    assert(!cpu.waiting && cpu.v[3] == 0);
    execute(&cpu, 0xE19E); assert(cpu.pc == 0x202);
    execute(&cpu, 0xE1A1); assert(cpu.pc == 0x204);
    chip8_set_key(&cpu, 99, true);
    unsigned seen = 0;
    for (unsigned j = 0; j < 256; ++j) {
        execute(&cpu, 0xC10F);
        assert(cpu.v[1] <= 15);
        seen |= 1u << cpu.v[1];
    }
    assert(seen == 0xFFFF);
    execute(&cpu, 0xC100); assert(cpu.v[1] == 0);
}

static void test_errors(void)
{
    const uint16_t invalid[] = {0x0123, 0x00FD, 0x5121, 0x9121, 0x8128,
                                 0xE100, 0xF1FF, 0xD120};
    Chip8 cpu;
    for (unsigned j = 0; j < sizeof invalid / sizeof invalid[0]; ++j) {
        chip8_init(&cpu);
        cpu.memory[0x200] = (uint8_t)(invalid[j] >> 8);
        cpu.memory[0x201] = (uint8_t)invalid[j];
        assert(!chip8_step(&cpu) && cpu.error);
        uint16_t pc = cpu.pc;
        assert(!chip8_step(&cpu) && cpu.pc == pc);
    }
    chip8_init(&cpu); cpu.memory[0x200] = 0; cpu.memory[0x201] = 0xEE;
    assert(!chip8_step(&cpu) && cpu.sp == 0);
    chip8_init(&cpu);
    for (unsigned j = 0; j < 16; ++j) execute(&cpu, 0x2200);
    assert(!chip8_step(&cpu) && cpu.sp == 16);
    const uint16_t accesses[] = {0xF133, 0xF155, 0xF165, 0xD122};
    for (unsigned j = 0; j < sizeof accesses / sizeof accesses[0]; ++j) {
        chip8_init(&cpu); cpu.i = 4095; cpu.memory[4095] = 0xA5;
        cpu.memory[0x200] = (uint8_t)(accesses[j] >> 8);
        cpu.memory[0x201] = (uint8_t)accesses[j];
        assert(!chip8_step(&cpu) && cpu.memory[4095] == 0xA5);
    }
    chip8_init(&cpu); cpu.pc = 4095;
    assert(!chip8_step(&cpu));
    chip8_init(&cpu); cpu.pc = 4094;
    cpu.memory[4094] = 0x61; cpu.memory[4095] = 0xAB;
    assert(chip8_step(&cpu) && cpu.v[1] == 0xAB && cpu.pc == 4096);
    assert(!chip8_step(&cpu));
    chip8_init(&cpu); cpu.pc = 65535;
    assert(!chip8_step(&cpu));
    chip8_init(&cpu); cpu.i = 65535;
    cpu.memory[0x200] = 0xF0; cpu.memory[0x201] = 0x65;
    assert(!chip8_step(&cpu));
    chip8_init(&cpu); cpu.v[1] = 16;
    cpu.memory[0x200] = 0xE1; cpu.memory[0x201] = 0x9E;
    assert(!chip8_step(&cpu));
    chip8_init(&cpu); cpu.v[1] = 16;
    cpu.memory[0x200] = 0xF1; cpu.memory[0x201] = 0x29;
    assert(!chip8_step(&cpu));
    chip8_init(&cpu); cpu.v[0] = 255;
    execute(&cpu, 0xBFFF);
    assert(!chip8_step(&cpu));
}

int main(void)
{
    test_init();
    test_rom();
    test_registers();
    test_branches();
    test_memory();
    test_drawing();
    test_keys_timers_random();
    test_errors();
    puts("All CHIP-8 CPU tests passed.");
    return 0;
}
