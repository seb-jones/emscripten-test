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

#include "maths.c"
#include "sdl.c"
#include "gl.c"
#include "timing.c"

#define POSITION_ATTRIBUTE_LOCATION 0
#define TEXCOORD_ATTRIBUTE_LOCATION 1
#define POSITION_COMPONENTS 2
#define TEXCOORD_COMPONENTS 2
#define COMPONENT_BYTES (sizeof(float))
#define VERTEX_COMPONENTS (POSITION_COMPONENTS + TEXCOORD_COMPONENTS)
#define VERTEX_BYTES (VERTEX_COMPONENTS * COMPONENT_BYTES)
#define QUAD_VERTICES 6
#define QUAD_COMPONENTS                                                        \
    (QUAD_VERTICES * (POSITION_COMPONENTS + TEXCOORD_COMPONENTS))
#define QUAD_BYTES (QUAD_VERTICES * VERTEX_BYTES)
#define MAX_QUADS 20
#define MAX_QUAD_BYTES (MAX_QUADS * QUAD_BYTES)

typedef struct Renderer
{
    GLuint program;
    GLuint buffer_object;
    float *vertices;
    int quad_count;
} Renderer;

typedef struct Glyph
{
    int texture_x;
    int width;
    int height;
    int advance;
    int bearing_x;
    int bearing_y;
} Glyph;

typedef struct Font
{
    Glyph *glyphs;
    int glyphs_size;
    int texture_width;
    int texture_height;
    GLuint texture;
    float line_height;
} Font;

typedef struct Globals
{
    SDL sdl;
    Renderer renderer;
    Font font;
    Timing timing;
    char *visible_ascii;
    int visible_ascii_size;
    int window_width;
    int window_height;
} Globals;

void draw_quad(Renderer *renderer, float x, float y, float w, float h,
               float tex_x, float tex_y, float tex_w, float tex_h)
{
    // TODO unroll and simplify this function
    float r = x + w;
    float t = y + h;

    float positions[] = {
        r, t,

        x, t,

        x, y,

        r, t,

        x, y,

        r, y,
    };

    float tex_r = tex_x + tex_w;
    float tex_t = tex_y + tex_h;

    float texcoords[] = {
        tex_r, tex_t,

        tex_x, tex_t,

        tex_x, tex_y,

        tex_r, tex_t,

        tex_x, tex_y,

        tex_r, tex_y,
    };

    float *vertex =
        renderer->vertices + (renderer->quad_count * QUAD_COMPONENTS);
    float *position = positions;
    float *texcoord = texcoords;

    for (int i = 0; i < QUAD_VERTICES; ++i) {
        vertex[0] = position[0];
        vertex[1] = position[1];
        vertex[2] = texcoord[0];
        vertex[3] = texcoord[1];

        vertex += VERTEX_COMPONENTS;
        position += POSITION_COMPONENTS;
        texcoord += TEXCOORD_COMPONENTS;
    }

    ++renderer->quad_count;
}

void draw_glyph(Renderer *renderer, Font *font, Glyph *glyph, float x, float y)
{
    draw_quad(renderer, x, y, glyph->width, glyph->height, glyph->texture_x, 0,
              glyph->width, glyph->height);
}

void draw_string(Renderer *renderer, Font *font, const char *s, float x,
                 float y)
{
    Glyph *glyph;
    float px = x;
    float py = y;

    while (*s) {
        glyph = &font->glyphs[*s - ' '];

        draw_glyph(renderer, font, glyph, px + glyph->bearing_x,
                   py + font->line_height - glyph->bearing_y);

        px += glyph->advance;

        ++s;
    }
}

EM_BOOL main_loop(double time, void *user_data)
{
    Globals *globals = (Globals *)user_data;

    double dt = update_timing(&globals->timing, time);

    SDL_PumpEvents();

    SDL *sdl = &globals->sdl;
    if (sdl->keyboard_state[SDL_SCANCODE_Q]) {
        cleanup_sdl(sdl);
        return EM_FALSE;
    }

    // Render
    {
        Renderer *renderer = &globals->renderer;

        renderer->quad_count = 0;

        static char string[100];
        snprintf(string, 100, "FPS: %d", globals->timing.current_fps);

        draw_string(renderer, &globals->font, string, 0, 0);

        glClear(GL_COLOR_BUFFER_BIT);

        glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->quad_count * QUAD_BYTES,
                        renderer->vertices);

        glDrawArrays(GL_TRIANGLES, 0, renderer->quad_count * QUAD_VERTICES);

        SDL_GL_SwapWindow(sdl->window);
    }

    ++globals->timing.fps;

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
            "uniform mat4 projection;\n"
            "attribute vec2 position;\n"
            "attribute vec2 tex_coord;\n"
            "varying vec2 varying_tex_coord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(position.xy, 0.0, 1.0) * projection;\n"
            "    varying_tex_coord = tex_coord;\n"
            "}";

        const char fragment_shader_code[] =
            "precision lowp float;\n"
            "varying vec2 varying_tex_coord;\n"
            "uniform vec2 tex_dimensions;\n"
            "uniform sampler2D sampler;\n"
            "const float one = 1.0;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    vec2 tex_coord = vec2(varying_tex_coord.x / tex_dimensions.x, "
            "varying_tex_coord.y / tex_dimensions.y);\n"
            "    vec4 t = texture2D(sampler, tex_coord);\n"
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

        // Setup Font
        FT_Library freetype;
        FT_Face face;
        Font *font = &globals->font;
        {
            // TODO get proper error messages for freetype functions

            FT_Error error = FT_Init_FreeType(&freetype);
            if (error) printf("FT_Init_FreeType: %d\n ", error);

            error = FT_New_Face(freetype, "assets/fonts/NovaMono-Regular.ttf",
                                0, &face);
            if (error) printf("FT_New_Face: %d\n ", error);

            /* use 50pt at 100dpi */
            error = FT_Set_Char_Size(face, 30 * 64, 0, 100, 0);
            if (error) printf("FT_New_Face: %d\n ", error);

            font->line_height = face->size->metrics.height >> 6;

            globals->visible_ascii_size = '~' - ' ' + 1;
            globals->visible_ascii = malloc(globals->visible_ascii_size);

            font->glyphs_size = globals->visible_ascii_size;
            font->glyphs = malloc(font->glyphs_size * sizeof(*font->glyphs));

            Glyph *glyph = font->glyphs;

            int glyph_index;
            for (int i = 0; i < globals->visible_ascii_size; ++i, ++glyph) {
                globals->visible_ascii[i] = ' ' + i;

                glyph_index =
                    FT_Get_Char_Index(face, globals->visible_ascii[i]);

                error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
                if (error) printf("FT_Load_Glyph: %d\n ", error);

                glyph->width = face->glyph->metrics.width >> 6;
                glyph->height = face->glyph->metrics.height >> 6;
                glyph->advance = face->glyph->metrics.horiAdvance >> 6;
                glyph->bearing_x = face->glyph->metrics.horiBearingX >> 6;
                glyph->bearing_y = face->glyph->metrics.horiBearingY >> 6;

                font->texture_width += glyph->width;

                if (glyph->height > font->texture_height) {
                    font->texture_height = glyph->height;
                }
            }

            font->texture_width = round_up_to_power_of_two(font->texture_width);
            font->texture_height =
                round_up_to_power_of_two(font->texture_height);
        }

        // Setup Texture
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            glGenTextures(1, &font->texture);

            glActiveTexture(GL_TEXTURE0);

            glBindTexture(GL_TEXTURE_2D, font->texture);

            char *data = malloc(font->texture_width * font->texture_height);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, font->texture_width,
                         font->texture_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE,
                         data);

            free(data);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            Glyph *glyph = font->glyphs;
            int glyph_index;
            int x = 0;
            int error;
            for (int i = 0; i < globals->visible_ascii_size; ++i, ++glyph) {
                if (glyph->width == 0 || glyph->height == 0) continue;

                glyph_index =
                    FT_Get_Char_Index(face, globals->visible_ascii[i]);

                error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
                if (error) printf("FT_Load_Glyph: %d\n ", error);

                error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
                if (error) printf("FT_Render_Glyph: %d\n ", error);

                glTexSubImage2D(GL_TEXTURE_2D, 0, x, 0,
                                face->glyph->bitmap.width,
                                face->glyph->bitmap.rows, GL_ALPHA,
                                GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

                glyph->texture_x = x;

                x += face->glyph->bitmap.width;
            }
        }

        FT_Done_Face(face);
        FT_Done_FreeType(freetype);

        // Uniforms
        float l = 0.0f;
        float b = globals->window_height;
        float r = globals->window_width;
        float t = 0.0f;

        {
            GLint location = glGetUniformLocation(renderer->program, "sampler");

            glUniform1i(location, 0);

            location =
                glGetUniformLocation(renderer->program, "tex_dimensions");

            glUniform2f(location, font->texture_width, font->texture_height);

            location = glGetUniformLocation(renderer->program, "projection");

            float n = -1.0f;
            float f = 1.0f;

            // Orthographic projection matrix based on glOrtho:
            // https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glOrtho.xml
            float projection_matrix[] = {
                // row 1
                2.0f / (r - l),
                0.0f,
                0.0f,
                -((r + l) / (r - l)),

                // row 2
                0.0f,
                2.0f / (t - b),
                0.0f,
                -((t + b) / (t - b)),

                // row 3
                0.0f,
                0.0f,
                (-2.0f) / (f - n),
                -((f + n) / (f - n)),

                // row 4
                0.0f,
                0.0f,
                0.0f,
                1.0f,
            };

            glUniformMatrix4fv(location, 1, GL_FALSE, projection_matrix);
        }

        // Setup Vertices
        {
            renderer->vertices = malloc(MAX_QUAD_BYTES);
            renderer->quad_count = 0;
        }

        // Setup Vertex Buffer Object
        {
            glGenBuffers(1, &renderer->buffer_object);
            glBindBuffer(GL_ARRAY_BUFFER, renderer->buffer_object);
            glBufferData(GL_ARRAY_BUFFER, MAX_QUAD_BYTES, NULL,
                         GL_DYNAMIC_DRAW);

            glEnableVertexAttribArray(POSITION_ATTRIBUTE_LOCATION);
            glEnableVertexAttribArray(TEXCOORD_ATTRIBUTE_LOCATION);

            glVertexAttribPointer(POSITION_ATTRIBUTE_LOCATION,
                                  POSITION_COMPONENTS, GL_FLOAT, GL_FALSE,
                                  VERTEX_BYTES, 0);

            glVertexAttribPointer(
                TEXCOORD_ATTRIBUTE_LOCATION, TEXCOORD_COMPONENTS, GL_FLOAT,
                GL_FALSE, VERTEX_BYTES,
                (const void *)(POSITION_COMPONENTS * COMPONENT_BYTES));
        }
    }

    globals->timing.frame_start_time = emscripten_performance_now();

    emscripten_request_animation_frame_loop(main_loop, globals);

    return 0;
}
