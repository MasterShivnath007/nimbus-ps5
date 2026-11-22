// Small, dependency-free vector / matrix helpers.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstring>

namespace nimbus {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kDeg = kPi / 180.0;
constexpr double kEarthRadius = 6371000.0;

inline double clamp(double value, double low, double high) {
    return value < low ? low : (value > high ? high : value);
}
inline float clampf(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline double smoothstep(double edge0, double edge1, double x) {
    const double t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
inline double ease_in_out(double t) {
    return t < 0.5 ? 2.0 * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 2.0) * 0.5;
}
inline double ease_out_cubic(double t) { return 1.0 - std::pow(1.0 - t, 3.0); }

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float length() const { return std::sqrt(x * x + y * y); }
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3 &operator+=(const Vec3 &o) { x += o.x; y += o.y; z += o.z; return *this; }
    float dot(const Vec3 &o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3 &o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float length_sq() const { return x * x + y * y + z * z; }
    Vec3 normalized() const {
        const float len = length();
        return len > 1e-20f ? *this / len : Vec3(0.0f, 0.0f, 0.0f);
    }
};

inline Vec3 operator*(float s, const Vec3 &v) { return v * s; }
inline Vec3 lerp(const Vec3 &a, const Vec3 &b, float t) {
    return a + (b - a) * t;
}

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

struct Mat4 {
    float m[16]{};

    static Mat4 identity() {
        Mat4 out;
        out.m[0] = out.m[5] = out.m[10] = out.m[15] = 1.0f;
        return out;
    }

    float &at(int row, int column) { return m[column * 4 + row]; }
    float at(int row, int column) const { return m[column * 4 + row]; }

    Mat4 operator*(const Mat4 &rhs) const {
        Mat4 out;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                float sum = 0;
                for (int k = 0; k < 4; ++k) sum += at(r, k) * rhs.at(k, c);
                out.at(r, c) = sum;
            }
        }
        return out;
    }

    Vec4 operator*(const Vec4 &v) const {
        return {
            at(0, 0) * v.x + at(0, 1) * v.y + at(0, 2) * v.z + at(0, 3) * v.w,
            at(1, 0) * v.x + at(1, 1) * v.y + at(1, 2) * v.z + at(1, 3) * v.w,
            at(2, 0) * v.x + at(2, 1) * v.y + at(2, 2) * v.z + at(2, 3) * v.w,
            at(3, 0) * v.x + at(3, 1) * v.y + at(3, 2) * v.z + at(3, 3) * v.w,
        };
    }
};

inline Mat4 mat4_translate(const Vec3 &t) {
    Mat4 out = Mat4::identity();
    out.at(0, 3) = t.x;
    out.at(1, 3) = t.y;
    out.at(2, 3) = t.z;
    return out;
}

inline Mat4 mat4_scale(float s) {
    Mat4 out = Mat4::identity();
    out.at(0, 0) = out.at(1, 1) = out.at(2, 2) = s;
    return out;
}

inline Mat4 mat4_rotate_z(float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    Mat4 out = Mat4::identity();
    out.at(0, 0) = c;
    out.at(0, 1) = -s;
    out.at(1, 0) = s;
    out.at(1, 1) = c;
    return out;
}

inline Mat4 mat4_look_at(const Vec3 &eye, const Vec3 &target, const Vec3 &up) {
    const Vec3 f = (target - eye).normalized();
    const Vec3 s = f.cross(up).normalized();
    const Vec3 u = s.cross(f);
    Mat4 out = Mat4::identity();
    out.at(0, 0) = s.x;
    out.at(0, 1) = s.y;
    out.at(0, 2) = s.z;
    out.at(1, 0) = u.x;
    out.at(1, 1) = u.y;
    out.at(1, 2) = u.z;
    out.at(2, 0) = -f.x;
    out.at(2, 1) = -f.y;
    out.at(2, 2) = -f.z;
    out.at(0, 3) = -s.dot(eye);
    out.at(1, 3) = -u.dot(eye);
    out.at(2, 3) = f.dot(eye);
    return out;
}

inline Mat4 mat4_perspective(float fov_y_radians, float aspect, float near_plane, float far_plane) {
    const float f = 1.0f / std::tan(fov_y_radians * 0.5f);
    Mat4 out{};
    out.at(0, 0) = f / aspect;
    out.at(1, 1) = f;
    out.at(2, 2) = (far_plane + near_plane) / (near_plane - far_plane);
    out.at(2, 3) = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    out.at(3, 2) = -1.0f;
    return out;
}

inline Mat4 mat4_ortho(float left, float right, float bottom, float top) {
    Mat4 out = Mat4::identity();
    out.at(0, 0) = 2.0f / (right - left);
    out.at(1, 1) = 2.0f / (top - bottom);
    out.at(0, 3) = -(right + left) / (right - left);
    out.at(1, 3) = -(top + bottom) / (top - bottom);
    out.at(2, 2) = -1.0f;
    return out;
}

// Inverse of a general 4x4 matrix (used by the sky pass and screen raycasts).
inline Mat4 mat4_inverse(const Mat4 &mat) {
    // Work in row-major order, then write back into the column-major Mat4.
    float m[16];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) m[r * 4 + c] = mat.at(r, c);
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    Mat4 out;
    if (std::fabs(det) < 1e-24f) return Mat4::identity();
    const float inv_det = 1.0f / det;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) out.at(r, c) = inv[r * 4 + c] * inv_det;
    return out;
}

// ECEF unit vector. x = cos(lat) cos(lon), y = cos(lat) sin(lon), z = sin(lat).
inline Vec3 latlon_to_unit(double lat_degrees, double lon_degrees) {
    const double lat = lat_degrees * kDeg;
    const double lon = lon_degrees * kDeg;
    const double c = std::cos(lat);
    return Vec3(static_cast<float>(c * std::cos(lon)),
                static_cast<float>(c * std::sin(lon)),
                static_cast<float>(std::sin(lat)));
}

inline void unit_to_latlon(const Vec3 &unit, double &lat_degrees, double &lon_degrees) {
    const Vec3 v = unit.normalized();
    lat_degrees = std::asin(clamp(static_cast<double>(v.z), -1.0, 1.0)) / kDeg;
    lon_degrees = std::atan2(static_cast<double>(v.y), static_cast<double>(v.x)) / kDeg;
}

inline double wrap_longitude(double lon) {
    while (lon > 180.0) lon -= 360.0;
    while (lon < -180.0) lon += 360.0;
    return lon;
}

inline double great_circle_degrees(double lat1, double lon1, double lat2, double lon2) {
    const Vec3 a = latlon_to_unit(lat1, lon1);
    const Vec3 b = latlon_to_unit(lat2, lon2);
    return std::acos(clamp(static_cast<double>(a.dot(b)), -1.0, 1.0)) / kDeg;
}

inline Vec3 slerp_unit(const Vec3 &a, const Vec3 &b, double t) {
    const double dot = clamp(static_cast<double>(a.dot(b)), -1.0, 1.0);
    const double theta = std::acos(dot);
    if (theta < 1e-6) return a;
    const double s = std::sin(theta);
    return (a * static_cast<float>(std::sin((1.0 - t) * theta) / s) +
            b * static_cast<float>(std::sin(t * theta) / s));
}

}  // namespace nimbus
