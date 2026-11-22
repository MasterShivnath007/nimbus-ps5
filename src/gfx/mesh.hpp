#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gfx/gl.hpp"

namespace nimbus::gfx {

struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    int vertex_count = 0;
    int mode = GL_TRIANGLES;

    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
    Mesh(Mesh &&other) noexcept;
    Mesh &operator=(Mesh &&other) noexcept;

    void upload(const void *data, size_t bytes, int vertices, int draw_mode,
                const std::vector<int> &layout /* components per attribute */);
    void draw() const;
    void release();
};

Mesh make_sphere(float radius, int segments, int rings);
Mesh make_grid(int subdivisions);
Mesh make_quad();

// Dynamic stream of colored line segments (x,y,z,r,g,b,a per vertex).
class LineBatch {
public:
    LineBatch() = default;
    ~LineBatch();
    void begin();
    void add(const float a[3], const float b[3], const float color[4]);
    void add_colored(const float a[3], const float color_a[4], const float b[3],
                     const float color_b[4]);
    void flush();

private:
    std::vector<float> data_;
    Mesh mesh_;
    bool uploaded_ = false;
};

}  // namespace nimbus::gfx
