#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <GLES2/gl2.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include "gl.c"

SDL_Window *window;
SDL_GLContext glcontext;
const Uint8 *keyboard_state;
int keyboard_state_size;
double frame_start_time;
double previous_frame_start_time;
double fps_timer = 0;
int fps = 0;
int current_fps = 0;
int window_width = 640;
int window_height = 480;
float camera_x = 0;
float camera_y = 0;

GLuint texture = 0;
GLuint program_object = 0;
GLuint vertex_pos_buffer, texcoord_buffer;

Mix_Music *music = NULL;
Mix_Chunk *wave = NULL;
int channel = -1;

FT_Library library;
GLuint font = 0;

// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
uint32_t round_up_to_power_of_two(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

bool setup_sdl()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "setup_sdl: SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("EMCC Test", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, window_width,
                              window_height, SDL_WINDOW_OPENGL);

    glcontext = SDL_GL_CreateContext(window);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Try adaptive vsync
    if (SDL_GL_SetSwapInterval(-1) == -1) {
        fprintf(stderr,
                "main: SDL_GL_SetSwapInterval: Error setting adaptive "
                "vsync: '%s'. Trying regular vsync.\n",
                SDL_GetError());
        // Try regular vsync
        if (SDL_GL_SetSwapInterval(1) == -1) {
            // vsync not supported
            fprintf(stderr, "main: SDL_GL_SetSwapInterval: '%s'\n",
                    SDL_GetError());
        }
    }

    keyboard_state = SDL_GetKeyboardState(&keyboard_state_size);

    assert(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG);

    const char *image_filename = "assets/images/player.png";

    SDL_Surface *surface = IMG_Load(image_filename);

    if (!surface) {
        fprintf(stderr, "setup_sdl: IMG_Load '%s': %s\n", image_filename,
                IMG_GetError());
        return false;
    }

    if (surface->format->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Surface *converted_surface =
            SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        assert(converted_surface);

        SDL_FreeSurface(surface);

        surface = converted_surface;
    }

    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, surface->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    SDL_FreeSurface(surface);

    // SHADERS
    GLuint vertex_shader =
        load_shader_from_file("assets/shaders/viewport.vert", GL_VERTEX_SHADER);

    GLuint fragment_shader = load_shader_from_file(
        "assets/shaders/viewport.frag", GL_FRAGMENT_SHADER);

    program_object = glCreateProgram();

    if (program_object == 0) return false;

    glAttachShader(program_object, vertex_shader);
    glAttachShader(program_object, fragment_shader);

    glBindAttribLocation(program_object, 0, "position");
    glBindAttribLocation(program_object, 1, "tex_coord");

    glLinkProgram(program_object);

    // Check link status
    GLint linked;
    glGetProgramiv(program_object, GL_LINK_STATUS, &linked);

    if (!linked) {
        fprintf(stderr, "setup_sdl: error linking GL program\n");
        return false;
    }

    glReleaseShaderCompiler();

    // VBOs
    GLfloat vertices[] = {
        0.5f,  0.5f,  0.0f,

        -0.5f, 0.5f,  0.0f,

        -0.5f, -0.5f, 0.0f,

        0.5f,  0.5f,  0.0f,

        -0.5f, -0.5f, 0.0f,

        0.5f,  -0.5f, 0.0f,
    };

    glGenBuffers(1, &vertex_pos_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_pos_buffer);
    glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(*vertices), vertices,
                 GL_STATIC_DRAW);

    GLfloat tex_coords[] = {
        1.0f, 0.0f,

        0.0f, 0.0f,

        0.0f, 1.0f,

        1.0f, 0.0f,

        0.0f, 1.0f,

        1.0f, 1.0f,
    };

    glGenBuffers(1, &texcoord_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, texcoord_buffer);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(*tex_coords), tex_coords,
                 GL_STATIC_DRAW);

    // Font
    {
        // TODO get proper error messages for freetype functions

        FT_Error error = FT_Init_FreeType(&library);
        if (error) printf("setup_sdl: FT_Init_FreeType: %d\n ", error);

        FT_Face face;

        error =
            FT_New_Face(library, "assets/fonts/NovaMono-Regular.ttf", 0, &face);
        if (error) printf("setup_sdl: FT_New_Face: %d\n ", error);

        /* use 50pt at 100dpi */
        // TODO get DPI of screen
        error = FT_Set_Char_Size(face, 50 * 64, 0, 100, 0);
        if (error) printf("setup_sdl: FT_New_Face: %d\n ", error);

        int visible_ascii_size = ('~' - ' ' + 1);
        char *visible_ascii = malloc(visible_ascii_size + 1);

        char c = ' ';
        for (int i = 0; i < visible_ascii_size; ++i, ++c) {
            visible_ascii[i] = c;
        }

        visible_ascii[visible_ascii_size] = '\0';

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        int max_advance = (face->size->metrics.max_advance >> 6);
        int max_height = face->size->metrics.height >> 6;

        int font_texture_width =
            round_up_to_power_of_two(max_advance * visible_ascii_size);

        int font_texture_height = round_up_to_power_of_two(max_height);

        glGenTextures(1, &font);

        glBindTexture(GL_TEXTURE_2D, font);

        // TODO This suppresses a warning emitted by WebGL, see if we can
        // replace or work around it
        uint8_t *buffer = malloc(font_texture_width * font_texture_height);
        memset(buffer, 0, font_texture_width * font_texture_height);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, font_texture_width,
                     font_texture_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE,
                     buffer);

        free(buffer);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        const char *glyphs = visible_ascii;
        int glyph_index;

        for (int i = 0; i < visible_ascii_size; ++i, ++glyphs) {
            glyph_index = FT_Get_Char_Index(face, *glyphs);

            error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
            if (error) printf("setup_sdl: FT_Load_Glyph: %d\n ", error);

            error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
            if (error) printf("setup_sdl: FT_Render_Glyph: %d\n ", error);

            if (!face->glyph->bitmap.buffer) continue;

            glTexSubImage2D(GL_TEXTURE_2D, 0, i * max_advance, 0, max_advance,
                            max_height, GL_ALPHA, GL_UNSIGNED_BYTE,
                            face->glyph->bitmap.buffer);
        }
    }

    // Audio
    int frequency = frequency = EM_ASM_INT_V({
        var context;
        try {
            context = new AudioContext();
        } catch (e) {
            context = new webkitAudioContext(); // safari only
        }
        return context.sampleRate;
    });

    if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        fprintf(stderr, "setup_sdl: Mix_OpenAudio: %s\n", Mix_GetError());
        return false;
    }

    Mix_Init(MIX_INIT_OGG);
    if (!(Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG)) {
        fprintf(stderr, "setup_sdl: Mix_Init: %s\n", Mix_GetError());
        return false;
    }

    music = Mix_LoadMUS("./assets/audio/music.ogg");
    if (music == NULL) {
        fprintf(stderr, "setup_sdl: Mix_LoadMUS: %s\n", Mix_GetError());
        return false;
    }

    wave = Mix_LoadWAV("./assets/audio/sound.wav");
    if (wave == NULL) {
        fprintf(stderr, "setup_sdl: Mix_LoadWAV: %s\n", Mix_GetError());
        return false;
    }

    if (Mix_PlayMusic(music, -1) < 0) {
        fprintf(stderr, "setup_sdl: Mix_PlayMusic: %s\n", Mix_GetError());
        return -1;
    }

    return true;
}

void cleanup_sdl()
{
    Mix_Quit();
    Mix_CloseAudio();
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
        cleanup_sdl();
        return EM_FALSE;
    }

    if (keyboard_state[SDL_SCANCODE_SPACE] &&
        (channel == -1 || !Mix_Playing(channel)))
    {
        printf("meep\n");
        channel = Mix_PlayChannel(-1, wave, 0);
    }

    if (keyboard_state[SDL_SCANCODE_LEFT]) {
        camera_x -= 0.05f;
    } else if (keyboard_state[SDL_SCANCODE_RIGHT]) {
        camera_x += 0.05f;
    }

    if (keyboard_state[SDL_SCANCODE_UP]) {
        camera_y += 0.05f;
    } else if (keyboard_state[SDL_SCANCODE_DOWN]) {
        camera_y -= 0.05f;
    }

    // Draw Triangle
    {
        // Set viewport and clear screen
        glViewport(0, 0, window_width, window_height);

        glClearColor(0.3, 0.3, 0.4, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program_object);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        // Uniforms
        {
            GLint location =
                glGetUniformLocation(program_object, "translation");

            glUniform4f(location, camera_x, camera_y, 0, 0);

            location = glGetUniformLocation(program_object, "sampler");

            glUniform1i(location, 0);
        }

        // Pass positions to shader
        glBindBuffer(GL_ARRAY_BUFFER, vertex_pos_buffer);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(0);

        // Pass tex_coords to shader
        glBindBuffer(GL_ARRAY_BUFFER, texcoord_buffer);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(1);

        // Draw
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    SDL_GL_SwapWindow(window);

    ++fps;

    return EM_TRUE;
}

int main(int argc, char *argv[])
{
    if (!setup_sdl()) {
        cleanup_sdl();
        return 1;
    }

    frame_start_time = emscripten_performance_now();

    emscripten_request_animation_frame_loop(main_loop, NULL);

    return 0;
}
