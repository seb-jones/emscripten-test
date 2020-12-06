GLuint load_shader(const char *shader_code, GLenum type)
{
    GLuint shader = glCreateShader(type);

    if (shader == 0) return 0;

    glShaderSource(shader, 1, &shader_code, NULL);

    glCompileShader(shader);

    // Get compilation status
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLint info_length = 0;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_length);

        if (info_length > 1) {
            char *info_log = malloc(sizeof(*info_log) * info_length);

            glGetShaderInfoLog(shader, info_length, NULL, info_log);

            fprintf(stderr, "load_shader: error compiling: %s\n", info_log);

            free(info_log);
        }

        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

GLuint load_shader_from_file(const char *filename, GLenum type)
{
    FILE *file = fopen(filename, "r");
    assert(file);

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rewind(file);

    char *code = (char *)malloc(sizeof(char) * size);
    assert(code);

    size_t read = fread(code, 1, size, file);
    assert(read == size);

    if (!code) return 0;

    return load_shader(code, type);
}
