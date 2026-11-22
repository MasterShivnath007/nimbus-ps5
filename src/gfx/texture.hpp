#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gfx/gl.hpp"

namespace nimbus::gfx {

struct Texture2D {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    GLenum format = GL_RGBA;

    Texture2D() = default;
    ~Texture2D();
    Texture2D(const Texture2D &) = delete;
    Texture2D &operator=(const Texture2D &) = delete;
    Texture2D(Texture2D &&other) noexcept;
    Texture2D &operator=(Texture2D &&other) noexcept;

    bool create_rgba(int w, int h, const void *pixels, bool mipmaps = true);
    bool create_r8(int w, int h, const void *pixels, bool mipmaps = false);
    bool create_rg16(int w, int h, const void *pixels, bool mipmaps = false);
    void update_rgba(int w, int h, const void *pixels);
    void update_r8(int x, int y, int w, int h, const void *pixels);
    void set_filter(bool linear, bool mipmaps);
    void bind(int unit) const;
    void release();
};

struct Framebuffer {
    GLuint fbo = 0;
    Texture2D color;
    GLuint depth = 0;
    int width = 0;
    int height = 0;
    bool hdr = false;

    Framebuffer() = default;
    ~Framebuffer();
    Framebuffer(const Framebuffer &) = delete;
    Framebuffer &operator=(const Framebuffer &) = delete;

    bool create(int w, int h, bool hdr_color);
    void bind() const;
    void unbind(int viewport_w, int viewport_h) const;
    void release();
};

Texture2D load_texture(const std::string &path, bool mipmaps, bool srgb = false);
bool write_png(const std::string &path, int width, int height, const void *rgba);

}  // namespace nimbus::gfx
