#include "gfx/mesh.hpp"

#include <cmath>
#include <utility>

#include "gfx/math.hpp"

namespace nimbus::gfx {

Mesh::~Mesh() { release(); }

Mesh::Mesh(Mesh &&other) noexcept
    : vao(other.vao), vbo(other.vbo), vertex_count(other.vertex_count), mode(other.mode) {
    other.vao = other.vbo = 0;
    other.vertex_count = 0;
}

Mesh &Mesh::operator=(Mesh &&other) noexcept {
    if (this != &other) {
        release();
        vao = other.vao;
        vbo = other.vbo;
        vertex_count = other.vertex_count;
        mode = other.mode;
        other.vao = other.vbo = 0;
        other.vertex_count = 0;
    }
    return *this;
}

void Mesh::release() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vbo = vao = 0;
    vertex_count = 0;
}

void Mesh::upload(const void *data, size_t bytes, int vertices, int draw_mode,
                  const std::vector<int> &layout) {
    release();
    mode = draw_mode;
    vertex_count = vertices;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
    int stride = 0;
    for (int components : layout) stride += components * static_cast<int>(sizeof(float));
    int offset = 0;
    for (size_t i = 0; i < layout.size(); ++i) {
        glEnableVertexAttribArray(static_cast<GLuint>(i));
        glVertexAttribPointer(static_cast<GLuint>(i), layout[i], GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)));
        offset += layout[i] * static_cast<int>(sizeof(float));
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::draw() const {
    if (!vao || vertex_count == 0) return;
    glBindVertexArray(vao);
    glDrawArrays(static_cast<GLenum>(mode), 0, vertex_count);
    glBindVertexArray(0);
}

Mesh make_sphere(float radius, int segments, int rings) {
    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(segments * rings * 6 * 8));
    auto push = [&](double lat, double lon) {
        const double lat_rad = lat * kDeg;
        const double lon_rad = lon * kDeg;
        const double c = std::cos(lat_rad);
        const float x = static_cast<float>(c * std::cos(lon_rad));
        const float y = static_cast<float>(c * std::sin(lon_rad));
        const float z = static_cast<float>(std::sin(lat_rad));
        vertices.insert(vertices.end(), {x, y, z, x, y, z,
                                         static_cast<float>(lon / 360.0 + 0.5),
                                         static_cast<float>(0.5 - lat / 180.0)});
    };
    for (int ring = 0; ring < rings; ++ring) {
        const double lat0 = 90.0 - 180.0 * ring / rings;
        const double lat1 = 90.0 - 180.0 * (ring + 1) / rings;
        for (int segment = 0; segment < segments; ++segment) {
            const double lon0 = -180.0 + 360.0 * segment / segments;
            const double lon1 = -180.0 + 360.0 * (segment + 1) / segments;
            push(lat0, lon0);
            push(lat1, lon0);
            push(lat1, lon1);
            push(lat0, lon0);
            push(lat1, lon1);
            push(lat0, lon1);
        }
    }
    // Scale positions and normals by radius (normals must stay unit).
    for (size_t i = 0; i < vertices.size(); i += 8) {
        vertices[i] *= radius;
        vertices[i + 1] *= radius;
        vertices[i + 2] *= radius;
    }
    Mesh mesh;
    mesh.upload(vertices.data(), vertices.size() * sizeof(float),
                static_cast<int>(vertices.size() / 8), GL_TRIANGLES, {3, 3, 2});
    return mesh;
}

Mesh make_grid(int subdivisions) {
    std::vector<float> vertices;
    const int n = subdivisions < 1 ? 1 : subdivisions;
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            const float u0 = static_cast<float>(i) / n;
            const float u1 = static_cast<float>(i + 1) / n;
            const float v0 = static_cast<float>(j) / n;
            const float v1 = static_cast<float>(j + 1) / n;
            vertices.insert(vertices.end(), {u0, v0, u1, v0, u1, v1,
                                             u0, v0, u1, v1, u0, v1});
        }
    }
    Mesh mesh;
    mesh.upload(vertices.data(), vertices.size() * sizeof(float),
                static_cast<int>(vertices.size() / 2), GL_TRIANGLES, {2});
    return mesh;
}

Mesh make_quad() {
    const float vertices[] = {
        -1, -1, 1, -1, 1, 1,
        -1, -1, 1, 1, -1, 1,
    };
    Mesh mesh;
    mesh.upload(vertices, sizeof(vertices), 6, GL_TRIANGLES, {2});
    return mesh;
}

LineBatch::~LineBatch() = default;

void LineBatch::begin() {
    data_.clear();
    uploaded_ = false;
}

void LineBatch::add(const float a[3], const float b[3], const float color[4]) {
    data_.insert(data_.end(), {a[0], a[1], a[2], color[0], color[1], color[2], color[3]});
    data_.insert(data_.end(), {b[0], b[1], b[2], color[0], color[1], color[2], color[3]});
}

void LineBatch::add_colored(const float a[3], const float color_a[4], const float b[3],
                            const float color_b[4]) {
    data_.insert(data_.end(), {a[0], a[1], a[2], color_a[0], color_a[1], color_a[2], color_a[3]});
    data_.insert(data_.end(), {b[0], b[1], b[2], color_b[0], color_b[1], color_b[2], color_b[3]});
}

void LineBatch::flush() {
    if (data_.empty()) return;
    mesh_.upload(data_.data(), data_.size() * sizeof(float),
                 static_cast<int>(data_.size() / 7), GL_LINES, {3, 4});
    uploaded_ = true;
    mesh_.draw();
}

}  // namespace nimbus::gfx
