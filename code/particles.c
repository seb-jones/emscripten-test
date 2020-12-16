#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include "sdl.c"
#include "gl.c"

#define POSITION_ATTRIBUTE_LOCATION 0
#define COLOUR_ATTRIBUTE_LOCATION 1
#define POSITION_ATTRIBUTE_SIZE 4
#define COLOUR_ATTRIBUTE_SIZE 4
#define PARTICLE_FLOATS (POSITION_ATTRIBUTE_SIZE + COLOUR_ATTRIBUTE_SIZE)
#define PARTICLE_BYTES ((PARTICLE_FLOATS) * sizeof(float))
#define ONE_SECOND 1000.0
#define PARTICLE_SPEED 0.002f
#define NEW_PARTICLES_PER_FRAME 1000

typedef struct Renderer
{
    GLuint program;
    GLuint buffer_object;
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
    float *particles;
    int window_width;
    int window_height;
    double frame_start_time;
    double previous_frame_start_time;
    int particles_count;
    int particles_size;
} Globals;

void set_particle(float *particle, float x, float y, float r, float g, float b,
                  float a)
{
    particle[0] = x;
    particle[1] = y;
    particle[2] = 0; // z
    particle[3] = 1; // w

    particle[4] = r;
    particle[5] = g;
    particle[6] = b;
    particle[7] = a;
}

void spawn_particle(float *particle)
{
    float x = ((float)rand() / (float)(RAND_MAX / 2)) - 1.0f;
    float y = -1.1f;

    set_particle(particle, x, y, (float)rand() / (float)(RAND_MAX),
                 (float)rand() / (float)(RAND_MAX),
                 (float)rand() / (float)(RAND_MAX), 1.0f);
}

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

    // Update
    {
        float *particle = globals->particles;
        int i = 0;
        while (i < globals->particles_count) {
            particle[1] += PARTICLE_SPEED * dt;

            if (particle[1] > 1.0f) {
                --globals->particles_count;

                float *source = globals->particles +
                                (globals->particles_count * PARTICLE_FLOATS);

                memcpy(particle, source, PARTICLE_BYTES);
            } else {
                ++i;
                particle += PARTICLE_FLOATS;
            }
        }

        for (i = 0; i < NEW_PARTICLES_PER_FRAME; ++i) {
            if (globals->particles_count == globals->particles_size) break;

            spawn_particle(particle);
            particle += PARTICLE_FLOATS;
            ++globals->particles_count;
        }
    }

    // Render
    {
        Renderer *renderer = &globals->renderer;

        glClear(GL_COLOR_BUFFER_BIT);

        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        globals->particles_count * PARTICLE_BYTES,
                        globals->particles);

        glDrawArrays(GL_POINTS, 0, globals->particles_count);

        SDL_GL_SwapWindow(sdl->window);
    }

    ++timing->fps;

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

    // Setup Particles
    {
        globals->particles_size = 200000;
        globals->particles = malloc(globals->particles_size * PARTICLE_BYTES);
    }

    // Setup Renderer
    {
        Renderer *renderer = &globals->renderer;

        glEnable(GL_BLEND);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const char vertex_shader_code[] = "uniform float point_size;\n"
                                          "attribute vec4 position;\n"
                                          "attribute vec4 colour;\n"
                                          "varying vec4 varying_colour;\n"
                                          "\n"
                                          "void main()\n"
                                          "{\n"
                                          "    gl_Position = position;\n"
                                          "    gl_PointSize = point_size;\n"
                                          "    varying_colour = colour;\n"
                                          "}";

        const char fragment_shader_code[] =
            "precision mediump float;\n"
            "varying vec4 varying_colour;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = varying_colour;\n"
            "}";

        renderer->program = create_shader_program_from_code(
            vertex_shader_code, fragment_shader_code);

        if (renderer->program == 0) return false;

        glBindAttribLocation(renderer->program, POSITION_ATTRIBUTE_LOCATION,
                             "position");
        glBindAttribLocation(renderer->program, COLOUR_ATTRIBUTE_LOCATION,
                             "colour");

        if (!link_shader_program(renderer->program)) {
            return false;
        }

        glReleaseShaderCompiler();

        glUseProgram(renderer->program);

        glClearColor(0.0, 0.0, 0.0, 1.0);

        // Set point_size uniform
        {
            GLint location =
                glGetUniformLocation(renderer->program, "point_size");

            GLfloat pointSizeRange[2];
            glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, pointSizeRange);

            GLfloat max_point_size = globals->window_width / 50;
            GLfloat pointSize = pointSizeRange[1] > max_point_size ?
                                    max_point_size :
                                    pointSizeRange[1];

            glUniform1f(location, pointSize);
        }

        // Set up Vertex Buffer Object
        {
            glGenBuffers(1, &renderer->buffer_object);
            glBindBuffer(GL_ARRAY_BUFFER, renderer->buffer_object);

            glBufferData(GL_ARRAY_BUFFER,
                         globals->particles_size * PARTICLE_BYTES, NULL,
                         GL_DYNAMIC_DRAW);

            glEnableVertexAttribArray(POSITION_ATTRIBUTE_LOCATION);
            glEnableVertexAttribArray(COLOUR_ATTRIBUTE_LOCATION);

            glVertexAttribPointer(POSITION_ATTRIBUTE_LOCATION,
                                  POSITION_ATTRIBUTE_SIZE, GL_FLOAT, GL_FALSE,
                                  PARTICLE_BYTES, 0);

            glVertexAttribPointer(
                COLOUR_ATTRIBUTE_LOCATION, COLOUR_ATTRIBUTE_SIZE, GL_FLOAT,
                GL_FALSE, PARTICLE_BYTES,
                (const void *)(POSITION_ATTRIBUTE_SIZE * sizeof(float)));
        }
    }

    globals->timing.frame_start_time = emscripten_performance_now();

    emscripten_request_animation_frame_loop(main_loop, globals);

    return 0;
}
