#include "display.h"

bool display_init(Display *display)
{
    *display = (Display){0};
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        return false;
    if (!SDL_CreateWindowAndRenderer("CHIP-8", 768, 384, SDL_WINDOW_RESIZABLE,
                                     &display->window, &display->renderer))
        return false;
    if (!SDL_SetRenderLogicalPresentation(display->renderer, CHIP8_WIDTH,
                                          CHIP8_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX))
        return false;

    SDL_AudioSpec spec = {.format = SDL_AUDIO_F32, .channels = 1, .freq = 48000};
    display->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                               &spec, NULL, NULL);
    return display->audio && SDL_ResumeAudioStreamDevice(display->audio);
}

void display_close(Display *display)
{
    SDL_DestroyAudioStream(display->audio);
    SDL_DestroyRenderer(display->renderer);
    SDL_DestroyWindow(display->window);
    SDL_Quit();
}

bool display_events(Chip8 *cpu)
{
    static const SDL_Scancode mapping[16] = {
        SDL_SCANCODE_X, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_A,
        SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_Z, SDL_SCANCODE_C,
        SDL_SCANCODE_4, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_V
    };
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            return false;
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            /* Cancel the press without completing Fx0A. */
            for (unsigned key = 0; key < 16; ++key)
                cpu->keys[key] = false;
            cpu->wait_key = -1;
        }
        if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP)
            continue;
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
            return false;
        if (event.key.repeat)
            continue;
        for (unsigned key = 0; key < 16; ++key)
            if (event.key.scancode == mapping[key])
                chip8_set_key(cpu, key, event.type == SDL_EVENT_KEY_DOWN);
    }
    return true;
}

bool display_draw(Display *display, const Chip8 *cpu)
{
    if (!SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255) ||
        !SDL_RenderClear(display->renderer) ||
        !SDL_SetRenderDrawColor(display->renderer, 255, 255, 255, 255))
        return false;
    for (unsigned y = 0; y < CHIP8_HEIGHT; ++y)
        for (unsigned x = 0; x < CHIP8_WIDTH; ++x)
            if (cpu->pixels[y][x]) {
                SDL_FRect pixel = {(float)x, (float)y, 1.0f, 1.0f};
                if (!SDL_RenderFillRect(display->renderer, &pixel))
                    return false;
            }
    return SDL_RenderPresent(display->renderer);
}

bool display_sound(Display *display, bool enabled)
{
    if (!enabled) {
        if (display->beeping && !SDL_ClearAudioStream(display->audio))
            return false;
        display->beeping = false;
        display->audio_phase = 0;
        return true;
    }
    display->beeping = true;
    int queued = SDL_GetAudioStreamQueued(display->audio);
    if (queued < 0)
        return false;
    /* Keep the audio queue short so the beep stops promptly. */
    while (queued < (int)(480 * sizeof(float))) {
        float samples[480];
        for (unsigned n = 0; n < 480; ++n) {
            samples[n] = display->audio_phase < 60 ? 0.12f : -0.12f;
            display->audio_phase = (display->audio_phase + 1) % 120;
        }
        if (!SDL_PutAudioStreamData(display->audio, samples, sizeof samples))
            return false;
        queued += sizeof samples;
    }
    return true;
}
