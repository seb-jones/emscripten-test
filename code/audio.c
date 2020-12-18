#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include "sdl.c"

typedef struct Globals
{
    SDL sdl;
    int window_width;
    int window_height;
    Mix_Music *music;
} Globals;

EM_BOOL main_loop(double time, void *user_data)
{
    Globals *globals = (Globals *)user_data;

    SDL_PumpEvents();

    SDL *sdl = &globals->sdl;
    if (sdl->keyboard_state[SDL_SCANCODE_Q]) {
        cleanup_sdl(sdl);
        return EM_FALSE;
    }

    if (Mix_PlayingMusic()) {
        printf("Playing Music.\n");
    }

    return EM_TRUE;
}

int main(int argc, char *argv[])
{
    Globals *globals = malloc(sizeof(*globals));
    memset(globals, 0, sizeof(*globals));
    globals->window_width = 640;
    globals->window_height = 480;

    if (!setup_sdl(&globals->sdl, globals->window_width,
                   globals->window_height)) {
        cleanup_sdl(&globals->sdl);
        return 1;
    }

    int frequency = frequency = EM_ASM_INT_V({
        var context;
        try {
            context = new AudioContext();
        } catch (e) {
            context = new webkitAudioContext(); // safari only
        }
        return context.sampleRate;
    });

    if (!setup_sdl_mixer(&globals->sdl, frequency)) {
        cleanup_sdl(&globals->sdl);
        return 1;
    }

    globals->music = Mix_LoadMUS("./assets/audio/music.ogg");
    if (globals->music == NULL) {
        fprintf(stderr, "Mix_LoadMUS: %s\n", Mix_GetError());
        cleanup_sdl(&globals->sdl);
        return 1;
    }

    if (Mix_PlayMusic(globals->music, -1) < 0) {
        fprintf(stderr, "Mix_PlayMusic: %s\n", Mix_GetError());
        return 1;
    }

    emscripten_request_animation_frame_loop(main_loop, globals);

    return 0;
}
