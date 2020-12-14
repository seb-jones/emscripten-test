GLuint create_shader_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint program = glCreateProgram();
    if (!program) {
        return 0;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    return program;
}

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

    // Remove any non-ascii characters
    {
        uint8_t *write = (uint8_t *)code;

        while (*write != '\0' && *write <= 127) ++write;

        if (*write != '\0') *write = '\0';
    }

    GLuint shader = load_shader(code, type);

    if (!shader) {
        fprintf(stderr,
                "load_shader_from_file: error compiling shader file '%s'\n",
                filename);
        return 0;
    }

    return shader;
}

GLuint create_shader_program_from_code(const char *vertex_shader_code,
                                       const char *fragment_shader_code)
{
    assert(vertex_shader_code);
    assert(fragment_shader_code);

    GLuint vertex_shader = load_shader(vertex_shader_code, GL_VERTEX_SHADER);

    GLuint fragment_shader =
        load_shader(fragment_shader_code, GL_FRAGMENT_SHADER);

    return create_shader_program(vertex_shader, fragment_shader);
}

bool link_shader_program(GLuint program)
{
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked) {
        // TODO get proper error from GL?
        fprintf(stderr, "link_shader_program: error linking GL program\n");
        return false;
    }

    return true;
}
