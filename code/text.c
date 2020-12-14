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

#define POSITION_ATTRIBUTE_LOCATION 0
#define TEXCOORD_ATTRIBUTE_LOCATION 1
#define POSITION_COMPONENTS 2
#define TEXCOORD_COMPONENTS 2
#define COMPONENT_BYTES (sizeof(float))
#define VERTEX_COMPONENTS (POSITION_COMPONENTS + TEXCOORD_COMPONENTS)
#define VERTEX_BYTES (VERTEX_COMPONENTS * COMPONENT_BYTES)
#define QUAD_VERTICES 6
#define QUAD_BYTES (QUAD_VERTICES * VERTEX_BYTES)

typedef struct Renderer
{
    GLuint program;
    GLuint texture;
    GLuint buffer_object;
} Renderer;

typedef struct Globals
{
    SDL sdl;
    Renderer renderer;
    int window_width;
    int window_height;
    float *vertices;
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

        glBufferSubData(GL_ARRAY_BUFFER, 0, QUAD_BYTES, globals->vertices);

        glDrawArrays(GL_TRIANGLES, 0, QUAD_VERTICES);

        SDL_GL_SwapWindow(sdl->window);
    }

    return EM_FALSE;
    /* return EM_TRUE; */
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

        // Uniforms
        float l = 0.0f;
        float b = globals->window_height;
        float r = globals->window_width;
        float t = 0.0f;

        {
            GLint location = glGetUniformLocation(renderer->program, "sampler");

            glUniform1i(location, 0);

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

        // Setup Font
        FT_Library freetype;
        FT_Face face;
        {
            // TODO get proper error messages for freetype functions

            FT_Error error = FT_Init_FreeType(&freetype);
            if (error) printf("FT_Init_FreeType: %d\n ", error);

            error = FT_New_Face(freetype, "assets/fonts/NovaMono-Regular.ttf",
                                0, &face);
            if (error) printf("FT_New_Face: %d\n ", error);

            /* use 50pt at 100dpi */
            error = FT_Set_Char_Size(face, 100 * 64, 0, 100, 0);
            if (error) printf("FT_New_Face: %d\n ", error);

            int glyph_index = FT_Get_Char_Index(face, 'Q');

            error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
            if (error) printf("FT_Load_Glyph: %d\n ", error);

            error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
            if (error) printf("FT_Render_Glyph: %d\n ", error);
        }

        // Setup Texture
        int glyph_width;
        int glyph_height;
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

            glGenTextures(1, &renderer->texture);

            glActiveTexture(GL_TEXTURE0);

            glBindTexture(GL_TEXTURE_2D, renderer->texture);

            glyph_width = face->glyph->bitmap.width;
            glyph_height = face->glyph->bitmap.rows;

            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, glyph_width, glyph_height,
                         0, GL_ALPHA, GL_UNSIGNED_BYTE,
                         face->glyph->bitmap.buffer);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        FT_Done_Face(face);
        FT_Done_FreeType(freetype);

        // Setup Vertices
        {
            float positions[] = {
                glyph_width, glyph_height,

                0.0f,        glyph_height,

                0.0f,        0.0f,

                glyph_width, glyph_height,

                0.0f,        0.0f,

                glyph_width, 0.0f,
            };

            float texcoords[] = {
                1.0f, 1.0f,

                0.0f, 1.0f,

                0.0f, 0.0f,

                1.0f, 1.0f,

                0.0f, 0.0f,

                1.0f, 0.0f,
            };

            globals->vertices = malloc(VERTEX_BYTES * QUAD_VERTICES);

            float *vertex = globals->vertices;
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
        }

        // Setup Vertex Buffer Object
        {
            glGenBuffers(1, &renderer->buffer_object);
            glBindBuffer(GL_ARRAY_BUFFER, renderer->buffer_object);
            glBufferData(GL_ARRAY_BUFFER, QUAD_BYTES, NULL, GL_DYNAMIC_DRAW);

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

    emscripten_request_animation_frame_loop(main_loop, globals);

    return 0;
}
