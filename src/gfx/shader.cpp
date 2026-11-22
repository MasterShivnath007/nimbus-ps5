#include "gfx/shader.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace nimbus::gfx {

static GLuint compile(GLenum type, const char *source, const char *name) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length) + 1, 0);
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        std::fprintf(stderr, "[shader] %s %s compile failed:\n%s\n", name,
                     type == GL_VERTEX_SHADER ? "vertex" : "fragment", log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

bool Shader::build(const char *name, const char *vertex_source, const char *fragment_source) {
    name_ = name;
    const GLuint vs = compile(GL_VERTEX_SHADER, vertex_source, name);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_source, name);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length) + 1, 0);
        glGetProgramInfoLog(program, length, nullptr, log.data());
        std::fprintf(stderr, "[shader] %s link failed:\n%s\n", name, log.data());
        glDeleteProgram(program);
        return false;
    }
    if (program_) glDeleteProgram(program_);
    program_ = program;
    uniforms_.clear();
    return true;
}

bool Shader::build_from_files(const char *name, const std::string &vertex_path,
                              const std::string &fragment_path) {
    const std::string vertex = read_text_file(vertex_path);
    const std::string fragment = read_text_file(fragment_path);
    if (vertex.empty() || fragment.empty()) {
        std::fprintf(stderr, "[shader] cannot read %s / %s\n", vertex_path.c_str(),
                     fragment_path.c_str());
        return false;
    }
    return build(name, vertex.c_str(), fragment.c_str());
}

void Shader::use() const { glUseProgram(program_); }

GLint Shader::uniform(const char *name) {
    const auto it = uniforms_.find(name);
    if (it != uniforms_.end()) return it->second;
    const GLint location = glGetUniformLocation(program_, name);
    uniforms_.emplace(name, location);
    return location;
}

void Shader::set_int(const char *name, int value) { glUniform1i(uniform(name), value); }
void Shader::set_float(const char *name, float value) { glUniform1f(uniform(name), value); }
void Shader::set_vec2(const char *name, float x, float y) { glUniform2f(uniform(name), x, y); }
void Shader::set_vec3(const char *name, float x, float y, float z) {
    glUniform3f(uniform(name), x, y, z);
}
void Shader::set_vec4(const char *name, float x, float y, float z, float w) {
    glUniform4f(uniform(name), x, y, z, w);
}
void Shader::set_mat4(const char *name, const float *matrix) {
    glUniformMatrix4fv(uniform(name), 1, GL_FALSE, matrix);
}

std::string read_text_file(const std::string &path) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return {};
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    std::string out;
    if (length > 0) {
        out.resize(static_cast<size_t>(length));
        const size_t read = std::fread(out.data(), 1, out.size(), file);
        out.resize(read);
    }
    std::fclose(file);
    return out;
}

}  // namespace nimbus::gfx
