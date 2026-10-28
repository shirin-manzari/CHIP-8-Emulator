#ifndef DISPLAY_H
#define DISPLAY_H

#include "chip8.h"
#include <SDL3/SDL.h>

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioStream *audio;
    unsigned audio_phase;
    bool beeping;
} Display;

bool display_init(Display *display);
void display_close(Display *display);
bool display_events(Chip8 *cpu);
bool display_draw(Display *display, const Chip8 *cpu);
bool display_sound(Display *display, bool enabled);

#endif
