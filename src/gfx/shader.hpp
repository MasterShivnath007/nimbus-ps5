#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "gfx/gl.hpp"

namespace nimbus::gfx {

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;

    bool build(const char *name, const char *vertex_source, const char *fragment_source);
    bool build_from_files(const char *name, const std::string &vertex_path,
                          const std::string &fragment_path);
    void use() const;
    GLint uniform(const char *name);
    void set_int(const char *name, int value);
    void set_float(const char *name, float value);
    void set_vec2(const char *name, float x, float y);
    void set_vec3(const char *name, float x, float y, float z);
    void set_vec4(const char *name, float x, float y, float z, float w);
    void set_mat4(const char *name, const float *matrix);
    GLuint id() const { return program_; }
    bool valid() const { return program_ != 0; }

private:
    GLuint program_ = 0;
    std::string name_;
    std::unordered_map<std::string, GLint> uniforms_;
};

std::string read_text_file(const std::string &path);

}  // namespace nimbus::gfx
