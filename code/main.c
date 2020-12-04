#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <emscripten.h>
#include <emscripten/html5.h>

SDL_Window *window;
SDL_GLContext glcontext;
const Uint8 *keyboard_state;
int keyboard_state_size;
double frame_start_time;
double previous_frame_start_time;
double fps_timer = 0;
int fps = 0;
int current_fps = 0;

void quit_sdl()
{
    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

EM_BOOL main_loop(double time, void *user_data)
{
    previous_frame_start_time = frame_start_time;
    frame_start_time = time;

    double dt = frame_start_time - previous_frame_start_time;

    fps_timer += dt;
    while (fps_timer >= 1000.0) {
        current_fps = fps;
        fps = 0;
        fps_timer -= 1000.0;
    }

    SDL_PumpEvents();

    if (keyboard_state[SDL_SCANCODE_Q]) {
        quit_sdl();
        return EM_FALSE;
    }

    glClearColor(0.9, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(window);

    ++fps;

    return EM_TRUE;
}

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    int window_width = 1280;
    int window_height = 720;

    window = SDL_CreateWindow("EMCC Test", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, window_width,
                              window_height, SDL_WINDOW_OPENGL);

    glcontext = SDL_GL_CreateContext(window);

    frame_start_time = emscripten_performance_now();

    keyboard_state = SDL_GetKeyboardState(&keyboard_state_size);

    emscripten_request_animation_frame_loop(main_loop, NULL);

    return 0;
}
