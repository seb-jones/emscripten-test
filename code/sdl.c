#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <GLES2/gl2.h>

typedef struct SDL
{
    SDL_Window *window;
    SDL_GLContext *glcontext;
    const Uint8 *keyboard_state;
    int keyboard_state_size;
    bool mixer_initialised;
} SDL;

bool setup_sdl_mixer(SDL *sdl, int frequency)
{
    if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        fprintf(stderr, "setup_sdl_mixer: Mix_OpenAudio: %s\n", Mix_GetError());
        return false;
    }

    Mix_Init(MIX_INIT_OGG);
    if (!(Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG)) {
        fprintf(stderr, "setup_sdl_mixer: Mix_Init: %s\n", Mix_GetError());
        return false;
    }

    sdl->mixer_initialised = true;

    return true;
}

void cleanup_sdl_mixer()
{
    Mix_Quit();
    Mix_CloseAudio();
}

bool setup_sdl(SDL *sdl, int window_width, int window_height)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "setup_sdl: SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow("EMCC Test", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, window_width,
                                   window_height, SDL_WINDOW_OPENGL);

    if (!sdl->window) {
        fprintf(stderr, "setup_sdl: SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    sdl->glcontext = SDL_GL_CreateContext(sdl->window);
    if (!sdl->glcontext) {
        fprintf(stderr, "setup_sdl: SDL_GL_CreateContext: %s\n",
                SDL_GetError());
        return false;
    }

    sdl->keyboard_state = SDL_GetKeyboardState(&sdl->keyboard_state_size);

    glViewport(0, 0, window_width, window_height);

    return true;
}

void cleanup_sdl(const SDL *sdl)
{
    if (sdl->mixer_initialised) {
        cleanup_sdl_mixer();
    }

    SDL_GL_DeleteContext(sdl->glcontext);
    SDL_DestroyWindow(sdl->window);
    SDL_Quit();
}
