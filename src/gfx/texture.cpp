#include "gfx/texture.hpp"

#include <cstdio>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace nimbus::gfx {

Texture2D::~Texture2D() { release(); }

Texture2D::Texture2D(Texture2D &&other) noexcept
    : id(other.id), width(other.width), height(other.height), format(other.format) {
    other.id = 0;
    other.width = other.height = 0;
}

Texture2D &Texture2D::operator=(Texture2D &&other) noexcept {
    if (this != &other) {
        release();
        id = other.id;
        width = other.width;
        height = other.height;
        format = other.format;
        other.id = 0;
        other.width = other.height = 0;
    }
    return *this;
}

void Texture2D::release() {
    if (id) glDeleteTextures(1, &id);
    id = 0;
    width = height = 0;
}

bool Texture2D::create_rgba(int w, int h, const void *pixels, bool mipmaps) {
    release();
    width = w;
    height = h;
    format = GL_RGBA;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return id != 0;
}

bool Texture2D::create_r8(int w, int h, const void *pixels, bool mipmaps) {
    release();
    width = w;
    height = h;
    format = GL_RED;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return id != 0;
}

bool Texture2D::create_rg16(int w, int h, const void *pixels, bool mipmaps) {
    release();
    width = w;
    height = h;
    format = GL_RG;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16, w, h, 0, GL_RG, GL_UNSIGNED_SHORT, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return id != 0;
}

void Texture2D::update_rgba(int w, int h, const void *pixels) {
    if (!id || width != w || height != h) {
        create_rgba(w, h, pixels, false);
        return;
    }
    glBindTexture(GL_TEXTURE_2D, id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::update_r8(int x, int y, int w, int h, const void *pixels) {
    if (!id) return;
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RED, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::set_filter(bool linear, bool mipmaps) {
    if (!id) return;
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id);
}

Framebuffer::~Framebuffer() { release(); }

bool Framebuffer::create(int w, int h, bool hdr_color) {
    release();
    width = w;
    height = h;
    hdr = hdr_color;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &color.id);
    color.width = w;
    color.height = h;
    glBindTexture(GL_TEXTURE_2D, color.id);
    glTexImage2D(GL_TEXTURE_2D, 0, hdr ? GL_RGBA16F : GL_RGBA8, w, h, 0, GL_RGBA,
                 hdr ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color.id, 0);

    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[gl] framebuffer incomplete: 0x%04x\n", status);
        release();
        return false;
    }
    return true;
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
}

void Framebuffer::unbind(int viewport_w, int viewport_h) const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewport_w, viewport_h);
}

void Framebuffer::release() {
    if (depth) glDeleteRenderbuffers(1, &depth);
    depth = 0;
    color.release();
    if (fbo) glDeleteFramebuffers(1, &fbo);
    fbo = 0;
    width = height = 0;
}

Texture2D load_texture(const std::string &path, bool mipmaps, bool srgb) {
    int width = 0, height = 0, channels = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    Texture2D texture;
    if (!pixels) {
        std::fprintf(stderr, "[texture] cannot load %s: %s\n", path.c_str(), stbi_failure_reason());
        return texture;
    }
    (void)srgb;
    texture.create_rgba(width, height, pixels, mipmaps);
    stbi_image_free(pixels);
    return texture;
}

bool write_png(const std::string &path, int width, int height, const void *rgba) {
    stbi_flip_vertically_on_write(1);
    return stbi_write_png(path.c_str(), width, height, 4, rgba, width * 4) != 0;
}

}  // namespace nimbus::gfx
