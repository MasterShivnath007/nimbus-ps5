// Nimbus - OpenGL entry points.
#pragma once

// The PS5 SDK ships Mesa's GL headers; modern entry points are only declared
// when GL_GLEXT_PROTOTYPES is defined before the include.
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <cstdint>

namespace nimbus::gfx {
using GLuint = unsigned int;
using GLint = int;
using GLenum = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;

inline bool check_gl(const char *where) {
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::fprintf(stderr, "[gl] error 0x%04x at %s\n", error, where);
        return false;
    }
    return true;
}
}  // namespace nimbus::gfx
