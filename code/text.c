#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include "sdl.c"
#include "gl.c"

typedef struct Renderer
{
    GLuint program;
    GLuint buffer_object;
} Renderer;

typedef struct Globals
{
    SDL sdl;
    Renderer renderer;
    int window_width;
    int window_height;
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

    // Render
    {
        Renderer *renderer = &globals->renderer;

        glClear(GL_COLOR_BUFFER_BIT);
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

    // Setup Renderer
    {
        Renderer *renderer = &globals->renderer;

        glEnable(GL_BLEND);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const char vertex_shader_code[] =
            "attribute vec2 position;\n"
            "attribute vec2 tex_coord;\n"
            "varying vec2 varying_tex_coord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(position.x, position.y, 0.0, 1.0);\n"
            "    varying_tex_coord = tex_coord;\n"
            "}";

        const char fragment_shader_code[] =
            "precision mediump float;\n"
            "varying vec2 varying_tex_coord;\n"
            "uniform sampler2D sampler;\n"
            "const float one = 1.0;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    vec4 t = texture2D(sampler, varying_tex_coord);\n"
            "    gl_FragColor = vec4(one, one, one, t.a);\n"
            "}";

        renderer->program = create_shader_program_from_code(
            vertex_shader_code, fragment_shader_code);

        if (renderer->program == 0) return false;

        if (!link_shader_program(renderer->program)) {
            return false;
        }

        glReleaseShaderCompiler();

        glUseProgram(renderer->program);

        glClearColor(0.1, 0.3, 0.5, 1.0);
    }

    emscripten_request_animation_frame_loop(main_loop, globals);

    return 0;
}
