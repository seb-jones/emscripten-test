#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include "maths.c"
#include "sdl.c"
#include "gl.c"

#define ONE_SECOND 1000.0

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 64
#define TEXTURE_BYTES_PER_PIXEL 4
#define TEXTURE_BYTES (TEXTURE_WIDTH * TEXTURE_HEIGHT * TEXTURE_BYTES_PER_PIXEL)

#define TRIANGLE_VERTICES 3
#define QUAD_VERTICES (TRIANGLE_VERTICES * 2)

#define POSITION_COMPONENTS 2
#define POSITION_BYTES (POSITION_COMPONENTS * sizeof(float))
#define WORLD_TRIANGLE_BYTES (POSITION_BYTES * TRIANGLE_VERTICES)

#define TEXCOORD_COMPONENTS 2
#define TEXCOORD_BYTES (TEXCOORD_COMPONENTS * sizeof(float))
#define SCREEN_QUAD_BYTES ((POSITION_BYTES + TEXCOORD_BYTES) * QUAD_VERTICES)

typedef struct Renderer
{
    GLuint world_program;
    GLuint screen_program;
    GLuint texture;
    GLuint fbo;
    GLuint world_vbo;
    GLuint screen_vbo;
    GLint screen_position_attribute;
    GLint screen_texcoord_attribute;
    GLint world_position_attribute;
} Renderer;

typedef struct Timing
{
    double frame_start_time;
    double previous_frame_start_time;
    double fps_timer;
    int fps;
} Timing;

typedef struct Globals
{
    SDL sdl;
    Renderer renderer;
    Timing timing;
    int window_width;
    int window_height;
    float *vertices;
} Globals;

EM_BOOL main_loop(double time, void *user_data)
{
    Globals *globals = (Globals *)user_data;
    Timing *timing = &globals->timing;

    timing->previous_frame_start_time = timing->frame_start_time;
    timing->frame_start_time = time;

    double dt = timing->frame_start_time - timing->previous_frame_start_time;

    timing->fps_timer += dt;
    while (timing->fps_timer >= ONE_SECOND) {
        printf("FPS: %d; DT: %f\n", timing->fps, dt);
        timing->fps_timer -= ONE_SECOND;
        timing->fps = 0;
    }

    SDL_PumpEvents();

    SDL *sdl = &globals->sdl;
    if (sdl->keyboard_state[SDL_SCANCODE_Q]) {
        cleanup_sdl(sdl);
        return EM_FALSE;
    }

    // Render
    {
        Renderer *renderer = &globals->renderer;

        // Render world to texture
        {
            glViewport(0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT);

            glUseProgram(renderer->world_program);

            glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo);

            glBindBuffer(GL_ARRAY_BUFFER, renderer->world_vbo);

            glDisableVertexAttribArray(renderer->screen_position_attribute);
            glDisableVertexAttribArray(renderer->screen_texcoord_attribute);
            glEnableVertexAttribArray(renderer->world_position_attribute);

            glVertexAttribPointer(renderer->world_position_attribute,
                                  POSITION_COMPONENTS, GL_FLOAT, GL_FALSE,
                                  POSITION_BYTES, 0);

            glClearColor(0.1, 0.3, 0.5, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);

            glDrawArrays(GL_TRIANGLES, 0, TRIANGLE_VERTICES);
        }

        // Render texture to screen
        {
            glViewport(0, 0, globals->window_width, globals->window_height);

            glUseProgram(renderer->screen_program);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glBindBuffer(GL_ARRAY_BUFFER, renderer->screen_vbo);

            glDisableVertexAttribArray(renderer->world_position_attribute);
            glEnableVertexAttribArray(renderer->screen_position_attribute);
            glEnableVertexAttribArray(renderer->screen_texcoord_attribute);

            glVertexAttribPointer(renderer->screen_position_attribute,
                                  POSITION_COMPONENTS, GL_FLOAT, GL_FALSE,
                                  POSITION_BYTES + TEXCOORD_BYTES, 0);

            glVertexAttribPointer(renderer->screen_texcoord_attribute,
                                  TEXCOORD_COMPONENTS, GL_FLOAT, GL_FALSE,
                                  POSITION_BYTES + TEXCOORD_BYTES,
                                  (void *)POSITION_BYTES);

            glClearColor(0.0, 0.0, 0.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);

            glDrawArrays(GL_TRIANGLES, 0, QUAD_VERTICES);
        }

        SDL_GL_SwapWindow(sdl->window);

        ++timing->fps;
    }

    return EM_TRUE;
}

int main(int argc, char *argv[])
{
    Globals *globals = malloc(sizeof(*globals));
    memset(globals, 0, sizeof(*globals));
    globals->window_width = 800;
    globals->window_height = 600;

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

        glClearColor(0.1, 0.3, 0.5, 1.0);

        // Setup Screen Shader Program
        {
            const char vertex_shader_code[] =
                "attribute vec2 position;\n"
                "attribute vec2 texcoord;\n"
                "varying vec2 varying_texcoord;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    gl_Position = vec4(position, 0.0, 1.0);\n"
                "    varying_texcoord = texcoord;\n"
                "}";

            const char fragment_shader_code[] =
                "precision lowp float;\n"
                "varying vec2 varying_texcoord;\n"
                "uniform sampler2D sampler;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    gl_FragColor = texture2D(sampler, varying_texcoord);\n"
                "}";

            renderer->screen_program = create_shader_program_from_code(
                vertex_shader_code, fragment_shader_code);

            if (renderer->screen_program == 0) return false;

            if (!link_shader_program(renderer->screen_program)) {
                return false;
            }

            glUseProgram(renderer->screen_program);

            renderer->screen_position_attribute =
                glGetAttribLocation(renderer->screen_program, "position");
            renderer->screen_texcoord_attribute =
                glGetAttribLocation(renderer->screen_program, "texcoord");

            GLint location =
                glGetUniformLocation(renderer->screen_program, "sampler");

            glUniform1i(location, 0);
        }

        // Setup World Shader Program
        {
            const char vertex_shader_code[] =
                "attribute vec2 position;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    gl_Position = vec4(position, 0.0, 1.0);\n"
                "}";

            const char fragment_shader_code[] =
                "precision lowp float;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    gl_FragColor = vec4(0.8, 0.2, 0.8, 1.0);\n"
                "}";

            renderer->world_program = create_shader_program_from_code(
                vertex_shader_code, fragment_shader_code);

            if (renderer->world_program == 0) return false;

            if (!link_shader_program(renderer->world_program)) {
                return false;
            }

            renderer->world_position_attribute =
                glGetAttribLocation(renderer->world_program, "position");
        }

        glReleaseShaderCompiler();

        // Setup Texture
        {
            glGenTextures(1, &renderer->texture);

            glActiveTexture(GL_TEXTURE0);

            glBindTexture(GL_TEXTURE_2D, renderer->texture);

            // Generate blank pixels
            {
                uint8_t *pixels = malloc(TEXTURE_BYTES);
                memset(pixels, UINT8_MAX, TEXTURE_BYTES);

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_WIDTH,
                             TEXTURE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             pixels);

                free(pixels);
            }

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        // Setup Framebuffer Object
        {
            glGenFramebuffers(1, &renderer->fbo);

            glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, renderer->texture, 0);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

            if (status != GL_FRAMEBUFFER_COMPLETE) {
                fprintf(stderr, "Incomplete FBO: %d\n", status);
            }
        }

        // Setup Screen VBO
        {
            float positions[] = {
                1.0f,  1.0f,

                -1.0f, 1.0f,

                -1.0f, -1.0f,

                1.0f,  1.0f,

                -1.0f, -1.0f,

                1.0f,  -1.0f,
            };

            float texcoords[] = {
                1.0f, 1.0f,

                0.0f, 1.0f,

                0.0f, 0.0f,

                1.0f, 1.0f,

                0.0f, 0.0f,

                1.0f, 0.0f,
            };

            float *vertices = malloc(SCREEN_QUAD_BYTES);

            float *vertex = vertices;
            float *position = positions;
            float *texcoord = texcoords;

            for (int i = 0; i < QUAD_VERTICES; ++i) {
                vertex[0] = position[0];
                vertex[1] = position[1];
                vertex[2] = texcoord[0];
                vertex[3] = texcoord[1];

                vertex += POSITION_COMPONENTS + TEXCOORD_COMPONENTS;
                position += POSITION_COMPONENTS;
                texcoord += TEXCOORD_COMPONENTS;
            }

            glUseProgram(renderer->screen_program);

            glGenBuffers(1, &renderer->screen_vbo);
            glBindBuffer(GL_ARRAY_BUFFER, renderer->screen_vbo);
            glBufferData(GL_ARRAY_BUFFER, SCREEN_QUAD_BYTES, vertices,
                         GL_STATIC_DRAW);

            free(vertices);
        }

        // Setup World VBO
        {
            float positions[] = {
                0.5f, 1.0f,

                0.0f, 0.0f,

                1.0f, 0.0f,
            };

            glUseProgram(renderer->world_program);

            glGenBuffers(1, &renderer->world_vbo);
            glBindBuffer(GL_ARRAY_BUFFER, renderer->world_vbo);
            glBufferData(GL_ARRAY_BUFFER, WORLD_TRIANGLE_BYTES, positions,
                         GL_STATIC_DRAW);
        }
    }

    globals->timing.frame_start_time = emscripten_performance_now();

    emscripten_request_animation_frame_loop(main_loop, globals);

    return 0;
}
