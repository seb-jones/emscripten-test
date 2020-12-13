#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include "sdl.c"
#include "gl.c"

#define COLOUR_ATTRIBUTE_LOCATION 0
#define POSITION_ATTRIBUTE_LOCATION 1
#define POSITION_ATTRIBUTE_SIZE 4
#define COLOUR_ATTRIBUTE_SIZE 4
#define FLOATS_PER_PARTICLE (POSITION_ATTRIBUTE_SIZE * COLOUR_ATTRIBUTE_SIZE)
#define ONE_SECOND 1000.0

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

EM_BOOL main_loop(double time, void *user_data)
{
    Globals *globals = (Globals *)user_data;
    Timing *timing = &globals->timing;

    timing->previous_frame_start_time = timing->frame_start_time;
    timing->frame_start_time = time;

    double dt = timing->frame_start_time - timing->previous_frame_start_time;

    timing->fps_timer += dt;
    while (timing->fps_timer >= ONE_SECOND) {
        printf("FPS: %d\n", timing->fps);
        timing->fps_timer -= ONE_SECOND;
        timing->fps = 0;
    }

    SDL_PumpEvents();

    SDL *sdl = &globals->sdl;
    if (sdl->keyboard_state[SDL_SCANCODE_Q]) {
        cleanup_sdl(sdl);
        return EM_FALSE;
    }

    {
        Renderer *renderer = &globals->renderer;

        glClearColor(0.3, 0.3, 0.4, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        int buffer_bytes =
            (globals->particles_count * POSITION_ATTRIBUTE_SIZE *
             sizeof(float)) +
            (globals->particles_count * COLOUR_ATTRIBUTE_SIZE * sizeof(float));

        glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_bytes, globals->particles);

        SDL_GL_SwapWindow(sdl->window);

        glDrawArrays(GL_POINTS, 0, globals->particles_count);
    }

    ++timing->fps;

    return EM_TRUE;
}

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
    set_particle(particle, 0.0f, -1.0f, (float)rand() / (float)RAND_MAX,
                 (float)rand() / (float)RAND_MAX,
                 (float)rand() / (float)RAND_MAX, 1.0f);
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
        globals->particles_size = 100000;
        globals->particles =
            malloc(globals->particles_size * sizeof(*globals->particles) *
                   FLOATS_PER_PARTICLE);

        spawn_particle(globals->particles);

        globals->particles_count = 1;
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

        GLuint vertex_shader =
            load_shader(vertex_shader_code, GL_VERTEX_SHADER);

        const char fragment_shader_code[] =
            "precision mediump float;\n"
            "varying vec4 varying_colour;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = varying_colour;\n"
            "}";

        GLuint fragment_shader =
            load_shader(fragment_shader_code, GL_FRAGMENT_SHADER);

        renderer->program = glCreateProgram();

        // TODO proper error checking/reporting
        if (renderer->program == 0) return false;

        glAttachShader(renderer->program, vertex_shader);
        glAttachShader(renderer->program, fragment_shader);

        glBindAttribLocation(renderer->program, POSITION_ATTRIBUTE_LOCATION,
                             "position");
        glBindAttribLocation(renderer->program, COLOUR_ATTRIBUTE_LOCATION,
                             "colour");

        glLinkProgram(renderer->program);

        GLint linked;
        glGetProgramiv(renderer->program, GL_LINK_STATUS, &linked);

        if (!linked) {
            // TODO get error message from GL
            fprintf(stderr, "error linking GL program\n");
            return false;
        }

        glReleaseShaderCompiler();

        glUseProgram(renderer->program);

        // Set point_size uniform
        {
            GLint location =
                glGetUniformLocation(renderer->program, "point_size");

            GLfloat pointSizeRange[2];
            glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, pointSizeRange);

            GLfloat max_point_size = globals->window_width / 100;
            GLfloat pointSize = pointSizeRange[1] > max_point_size ?
                                    max_point_size :
                                    pointSizeRange[1];

            glUniform1f(location, pointSize);
        }

        // Set up Vertex Buffer Object
        {
            glGenBuffers(1, &renderer->buffer_object);
            glBindBuffer(GL_ARRAY_BUFFER, renderer->buffer_object);

            int buffer_bytes = (globals->particles_size *
                                POSITION_ATTRIBUTE_SIZE * sizeof(float)) +
                               (globals->particles_size *
                                COLOUR_ATTRIBUTE_SIZE * sizeof(float));

            glBufferData(GL_ARRAY_BUFFER, buffer_bytes, NULL, GL_DYNAMIC_DRAW);

            glEnableVertexAttribArray(POSITION_ATTRIBUTE_LOCATION);
            glEnableVertexAttribArray(COLOUR_ATTRIBUTE_LOCATION);

            glVertexAttribPointer(POSITION_ATTRIBUTE_LOCATION,
                                  POSITION_ATTRIBUTE_SIZE, GL_FLOAT, GL_FALSE,
                                  POSITION_ATTRIBUTE_SIZE * sizeof(float), 0);

            glVertexAttribPointer(
                COLOUR_ATTRIBUTE_LOCATION, COLOUR_ATTRIBUTE_SIZE, GL_FLOAT,
                GL_FALSE, COLOUR_ATTRIBUTE_SIZE * sizeof(float),
                (const void *)(POSITION_ATTRIBUTE_SIZE * sizeof(float)));
        }
    }

    globals->timing.frame_start_time = emscripten_performance_now();

    emscripten_request_animation_frame_loop(main_loop, globals);

    return 0;
}
