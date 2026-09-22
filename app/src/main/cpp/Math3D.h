#pragma once

#include <cmath>
#include <array>

namespace velo {

constexpr float PI = 3.14159265358979323846f;
inline float to_radians(float degrees) { return degrees * (PI / 180.0f); }

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float length_sq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(length_sq()); }

    Vec3 normalized() const {
        float l = length();
        if (l > 1e-6f) {
            float inv = 1.0f / l;
            return {x * inv, y * inv, z * inv};
        }
        return {0.0f, 0.0f, 0.0f};
    }

    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
};

/**
 * 4x4 Column-Major Matrix for OpenGL ES 3.0
 * Index layout:
 * [ 0  4   8  12 ]
 * [ 1  5   9  13 ]
 * [ 2  6  10  14 ]
 * [ 3  7  11  15 ]
 */
struct Mat4 {
    std::array<float, 16> m{};

    Mat4() {
        set_identity();
    }

    void set_identity() {
        m.fill(0.0f);
        m[0] = 1.0f;
        m[5] = 1.0f;
        m[10] = 1.0f;
        m[15] = 1.0f;
    }

    const float* data() const { return m.data(); }
    float* data() { return m.data(); }

    static Mat4 identity() {
        return Mat4();
    }

    static Mat4 translate(float tx, float ty, float tz) {
        Mat4 res;
        res.m[12] = tx;
        res.m[13] = ty;
        res.m[14] = tz;
        return res;
    }

    static Mat4 translate(const Vec3& v) {
        return translate(v.x, v.y, v.z);
    }

    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 res;
        res.m[0] = sx;
        res.m[5] = sy;
        res.m[10] = sz;
        return res;
    }

    static Mat4 rotate_x(float rad) {
        Mat4 res;
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[5] = c;
        res.m[6] = s;
        res.m[9] = -s;
        res.m[10] = c;
        return res;
    }

    static Mat4 rotate_y(float rad) {
        Mat4 res;
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[0] = c;
        res.m[2] = -s;
        res.m[8] = s;
        res.m[10] = c;
        return res;
    }

    static Mat4 rotate_z(float rad) {
        Mat4 res;
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[0] = c;
        res.m[1] = s;
        res.m[4] = -s;
        res.m[5] = c;
        return res;
    }

    static Mat4 ortho(float left, float right, float bottom, float top, float near_val = -1.0f, float far_val = 1.0f) {
        Mat4 res;
        res.m.fill(0.0f);
        res.m[0]  =  2.0f / (right - left);
        res.m[5]  =  2.0f / (top - bottom);
        res.m[10] = -2.0f / (far_val - near_val);
        res.m[12] = -(right + left) / (right - left);
        res.m[13] = -(top + bottom) / (top - bottom);
        res.m[14] = -(far_val + near_val) / (far_val - near_val);
        res.m[15] =  1.0f;
        return res;
    }

    static Mat4 perspective(float fov_y_rad, float aspect, float z_near, float z_far) {
        Mat4 res;
        res.m.fill(0.0f);
        float tan_half_fov = std::tan(fov_y_rad * 0.5f);
        res.m[0] = 1.0f / (aspect * tan_half_fov);
        res.m[5] = 1.0f / tan_half_fov;
        res.m[10] = -(z_far + z_near) / (z_far - z_near);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * z_far * z_near) / (z_far - z_near);
        return res;
    }

    static Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).normalized();
        Vec3 s = Vec3::cross(up, f).normalized();
        Vec3 u = Vec3::cross(f, s).normalized();

        Mat4 res;
        res.m[0] = s.x;
        res.m[4] = s.y;
        res.m[8] = s.z;

        res.m[1] = u.x;
        res.m[5] = u.y;
        res.m[9] = u.z;

        res.m[2] = -f.x;
        res.m[6] = -f.y;
        res.m[10] = -f.z;

        res.m[12] = -Vec3::dot(s, eye);
        res.m[13] = -Vec3::dot(u, eye);
        res.m[14] = Vec3::dot(f, eye);
        res.m[15] = 1.0f;
        return res;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 res;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                res.m[c * 4 + r] =
                    m[0 * 4 + r] * o.m[c * 4 + 0] +
                    m[1 * 4 + r] * o.m[c * 4 + 1] +
                    m[2 * 4 + r] * o.m[c * 4 + 2] +
                    m[3 * 4 + r] * o.m[c * 4 + 3];
            }
        }
        return res;
    }
};

} // namespace velo
