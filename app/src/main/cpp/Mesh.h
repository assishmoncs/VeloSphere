#pragma once

#include <GLES3/gl3.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include "Math3D.h"
#include "Shader.h"

namespace velo {

struct Vertex {
    Vec3  position{};
    Vec3  normal{};
    float u{0.0f};
    float v{0.0f};
};

struct ColorVertex {
    Vec3  position{};
    float u{0.0f};
    float v{0.0f};
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
    float a{1.0f};
};

// =============================================================================
// PROCEDURAL UV-SPHERE MESH
// =============================================================================

class SphereMesh {
public:
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};

    void init(float radius = 0.68f, int sectors = 24, int stacks = 16) {
        std::vector<Vertex> vertices;
        std::vector<GLushort> indices;
        vertices.reserve(static_cast<size_t>((stacks + 1) * (sectors + 1)));
        indices.reserve(static_cast<size_t>(stacks * sectors * 6));

        float const R = 1.0f / static_cast<float>(stacks);
        float const S = 1.0f / static_cast<float>(sectors);

        for (int r = 0; r <= stacks; ++r) {
            float phi = PI * static_cast<float>(r) * R;
            float sin_phi = std::sin(phi);
            float cos_phi = std::cos(phi);

            for (int s = 0; s <= sectors; ++s) {
                float theta = 2.0f * PI * static_cast<float>(s) * S;
                float sin_theta = std::sin(theta);
                float cos_theta = std::cos(theta);

                float x = cos_theta * sin_phi;
                float y = cos_phi;
                float z = sin_theta * sin_phi;

                Vertex v;
                v.position = {x * radius, y * radius, z * radius};
                v.normal = {x, y, z};
                v.u = static_cast<float>(s) * S;
                v.v = static_cast<float>(r) * R;
                vertices.push_back(v);
            }
        }

        for (int r = 0; r < stacks; ++r) {
            for (int s = 0; s < sectors; ++s) {
                int first = (r * (sectors + 1)) + s;
                int second = first + sectors + 1;
                indices.push_back(static_cast<GLushort>(first));
                indices.push_back(static_cast<GLushort>(first + 1));
                indices.push_back(static_cast<GLushort>(second));
                indices.push_back(static_cast<GLushort>(second));
                indices.push_back(static_cast<GLushort>(first + 1));
                indices.push_back(static_cast<GLushort>(second + 1));
            }
        }

        index_count = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLushort)), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        glBindVertexArray(0);
    }

    void draw() const {
        if (vao) {
            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, nullptr);
        }
    }

    void destroy() {
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            vao = vbo = ebo = 0;
            index_count = 0;
        }
    }
};

// =============================================================================
// PROCEDURAL 3D TORUS MESH (GYROSCOPIC RINGS, ROTORS & SHOCKWAVE RINGS)
// =============================================================================

class TorusMesh {
public:
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};

    void init(float major_r = 1.0f, float minor_r = 0.085f, int major_segs = 28, int minor_segs = 10) {
        std::vector<Vertex> vertices;
        std::vector<GLushort> indices;
        vertices.reserve(static_cast<size_t>((major_segs + 1) * (minor_segs + 1)));
        indices.reserve(static_cast<size_t>(major_segs * minor_segs * 6));

        for (int i = 0; i <= major_segs; ++i) {
            float u = static_cast<float>(i) / static_cast<float>(major_segs);
            float theta = u * 2.0f * PI;
            float cos_t = std::cos(theta);
            float sin_t = std::sin(theta);

            for (int j = 0; j <= minor_segs; ++j) {
                float v = static_cast<float>(j) / static_cast<float>(minor_segs);
                float phi = v * 2.0f * PI;
                float cos_p = std::cos(phi);
                float sin_p = std::sin(phi);

                float r = major_r + minor_r * cos_p;
                Vec3 pos{r * cos_t, minor_r * sin_p, r * sin_t};
                Vec3 nrm{cos_p * cos_t, sin_p, cos_p * sin_t};
                vertices.push_back({pos, nrm, u, v});
            }
        }

        for (int i = 0; i < major_segs; ++i) {
            for (int j = 0; j < minor_segs; ++j) {
                int a = i * (minor_segs + 1) + j;
                int b = (i + 1) * (minor_segs + 1) + j;
                indices.push_back(static_cast<GLushort>(a));
                indices.push_back(static_cast<GLushort>(b));
                indices.push_back(static_cast<GLushort>(a + 1));
                indices.push_back(static_cast<GLushort>(b));
                indices.push_back(static_cast<GLushort>(b + 1));
                indices.push_back(static_cast<GLushort>(a + 1));
            }
        }

        index_count = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLushort)), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        glBindVertexArray(0);
    }

    void draw() const {
        if (vao) {
            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, nullptr);
        }
    }

    void destroy() {
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            vao = vbo = ebo = 0;
            index_count = 0;
        }
    }
};

// =============================================================================
// PROCEDURAL CRYSTAL OCTAHEDRON MESH (DATA ORBS, POWER-UPS & PYLONS)
// =============================================================================

class CrystalMesh {
public:
    GLuint vao{0};
    GLuint vbo{0};
    GLsizei vertex_count{0};

    void init(float radius = 0.36f, float height_stretch = 1.32f) {
        const Vec3 top    {0.0f,  radius * height_stretch, 0.0f};
        const Vec3 bottom {0.0f, -radius * height_stretch, 0.0f};
        const Vec3 p0     { radius, 0.0f,  0.0f};
        const Vec3 p1     { 0.0f,   0.0f,  radius};
        const Vec3 p2     {-radius, 0.0f,  0.0f};
        const Vec3 p3     { 0.0f,   0.0f, -radius};

        std::vector<Vertex> verts;
        verts.reserve(24);
        auto add_tri = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
            Vec3 n = Vec3::cross(b - a, c - a).normalized();
            verts.push_back({a, n, 0.5f, 1.0f});
            verts.push_back({b, n, 0.0f, 0.0f});
            verts.push_back({c, n, 1.0f, 0.0f});
        };

        add_tri(top, p1, p0); add_tri(top, p2, p1);
        add_tri(top, p3, p2); add_tri(top, p0, p3);
        add_tri(bottom, p0, p1); add_tri(bottom, p1, p2);
        add_tri(bottom, p2, p3); add_tri(bottom, p3, p0);

        vertex_count = static_cast<GLsizei>(verts.size());

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)), verts.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        glBindVertexArray(0);
    }

    void draw() const {
        if (vao) {
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, vertex_count);
        }
    }

    void destroy() {
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            vao = vbo = 0;
            vertex_count = 0;
        }
    }
};

// =============================================================================
// PROCEDURAL 3D BOX MESH (ARTICULATED OBSTACLE PARTS, ARCHES & RAILS)
// =============================================================================

class BoxMesh {
public:
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};

    void init(float width = 1.0f, float height = 1.0f, float depth = 1.0f) {
        float hx = width * 0.5f;
        float hy = height * 0.5f;
        float hz = depth * 0.5f;

        std::vector<Vertex> vertices = {
            {{-hx, -hy,  hz}, {0, 0,  1}, 0.0f, 0.0f}, {{ hx, -hy,  hz}, {0, 0,  1}, 1.0f, 0.0f},
            {{ hx,  hy,  hz}, {0, 0,  1}, 1.0f, 1.0f}, {{-hx,  hy,  hz}, {0, 0,  1}, 0.0f, 1.0f},

            {{ hx, -hy, -hz}, {0, 0, -1}, 0.0f, 0.0f}, {{-hx, -hy, -hz}, {0, 0, -1}, 1.0f, 0.0f},
            {{-hx,  hy, -hz}, {0, 0, -1}, 1.0f, 1.0f}, {{ hx,  hy, -hz}, {0, 0, -1}, 0.0f, 1.0f},

            {{-hx, -hy, -hz}, {-1, 0, 0}, 0.0f, 0.0f}, {{-hx, -hy,  hz}, {-1, 0, 0}, 1.0f, 0.0f},
            {{-hx,  hy,  hz}, {-1, 0, 0}, 1.0f, 1.0f}, {{-hx,  hy, -hz}, {-1, 0, 0}, 0.0f, 1.0f},

            {{ hx, -hy,  hz}, {1, 0, 0}, 0.0f, 0.0f}, {{ hx, -hy, -hz}, {1, 0, 0}, 1.0f, 0.0f},
            {{ hx,  hy, -hz}, {1, 0, 0}, 1.0f, 1.0f}, {{ hx,  hy,  hz}, {1, 0, 0}, 0.0f, 1.0f},

            {{-hx,  hy,  hz}, {0, 1, 0}, 0.0f, 0.0f}, {{ hx,  hy,  hz}, {0, 1, 0}, 1.0f, 0.0f},
            {{ hx,  hy, -hz}, {0, 1, 0}, 1.0f, 1.0f}, {{-hx,  hy, -hz}, {0, 1, 0}, 0.0f, 1.0f},

            {{-hx, -hy, -hz}, {0, -1, 0}, 0.0f, 0.0f}, {{ hx, -hy, -hz}, {0, -1, 0}, 1.0f, 0.0f},
            {{ hx, -hy,  hz}, {0, -1, 0}, 1.0f, 1.0f}, {{-hx, -hy,  hz}, {0, -1, 0}, 0.0f, 1.0f}
        };

        std::vector<GLushort> indices;
        indices.reserve(36);
        for (GLushort i = 0; i < 6; ++i) {
            GLushort base = static_cast<GLushort>(i * 4);
            indices.push_back(static_cast<GLushort>(base + 0));
            indices.push_back(static_cast<GLushort>(base + 1));
            indices.push_back(static_cast<GLushort>(base + 2));
            indices.push_back(static_cast<GLushort>(base + 0));
            indices.push_back(static_cast<GLushort>(base + 2));
            indices.push_back(static_cast<GLushort>(base + 3));
        }

        index_count = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLushort)), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        glBindVertexArray(0);
    }

    void draw() const {
        if (vao) {
            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, nullptr);
        }
    }

    void destroy() {
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            vao = vbo = ebo = 0;
            index_count = 0;
        }
    }
};

// =============================================================================
// SUBDIVIDED RUNWAY TRACK MESH (8 Z-SLICES FOR SILKY-SMOOTH CURVED WORLD!)
// =============================================================================

class TrackMesh {
public:
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};

    void init(float width = 7.6f, float length = 10.0f, int slices = 8) {
        float hx = width * 0.5f;
        float hz = length * 0.5f;

        std::vector<Vertex> vertices;
        std::vector<GLushort> indices;
        vertices.reserve(static_cast<size_t>((slices + 1) * 2));
        indices.reserve(static_cast<size_t>(slices * 6));

        for (int i = 0; i <= slices; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(slices);
            float z = hz - t * length;
            vertices.push_back({{-hx, 0.0f, z}, {0, 1, 0}, 0.0f, t});
            vertices.push_back({{ hx, 0.0f, z}, {0, 1, 0}, 1.0f, t});
        }

        for (int i = 0; i < slices; ++i) {
            GLushort b = static_cast<GLushort>(i * 2);
            indices.push_back(static_cast<GLushort>(b + 0));
            indices.push_back(static_cast<GLushort>(b + 1));
            indices.push_back(static_cast<GLushort>(b + 3));
            indices.push_back(static_cast<GLushort>(b + 0));
            indices.push_back(static_cast<GLushort>(b + 3));
            indices.push_back(static_cast<GLushort>(b + 2));
        }

        index_count = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLushort)), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        glBindVertexArray(0);
    }

    void draw() const {
        if (vao) {
            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, nullptr);
        }
    }

    void destroy() {
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            vao = vbo = ebo = 0;
            index_count = 0;
        }
    }
};

// =============================================================================
// PROCEDURAL 2D/FULLSCREEN QUAD MESH
// =============================================================================

class QuadMesh {
public:
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};

    void init(float w = 1.0f, float h = 1.0f) {
        float hx = w * 0.5f;
        float hy = h * 0.5f;

        std::array<Vertex, 4> vertices = {{
            {{-hx, -hy, 0.0f}, {0, 0, 1}, 0.0f, 1.0f},
            {{ hx, -hy, 0.0f}, {0, 0, 1}, 1.0f, 1.0f},
            {{ hx,  hy, 0.0f}, {0, 0, 1}, 1.0f, 0.0f},
            {{-hx,  hy, 0.0f}, {0, 0, 1}, 0.0f, 0.0f}
        }};
        std::array<GLushort, 6> indices = {0, 1, 2, 0, 2, 3};
        index_count = 6;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(vertices)), vertices.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(indices)), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        glBindVertexArray(0);
    }

    void draw() const {
        if (vao) {
            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, nullptr);
        }
    }

    void destroy() {
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            vao = vbo = ebo = 0;
            index_count = 0;
        }
    }
};

// =============================================================================
// 3D EXPANDING SHOCKWAVE RING SYSTEM (JUMP, SLAM, NEAR-MISS, PHASE-DASH)
// =============================================================================

struct ShockwaveRing {
    Vec3   pos{};
    Color3 color{0.1f, 0.95f, 1.0f};
    float  life{0.0f};
    float  max_life{0.42f};
    float  max_radius{3.0f};
    bool   active{false};
};

class ShockwaveSystem {
public:
    static constexpr size_t POOL_SIZE = 10;
    std::array<ShockwaveRing, POOL_SIZE> rings{};

    void spawn(const Vec3& pos, const Color3& col, float max_radius = 2.8f, float max_life = 0.40f) {
        for (auto& r : rings) {
            if (!r.active) {
                r.active = true;
                r.pos = pos;
                r.color = col;
                r.life = 0.0f;
                r.max_life = max_life;
                r.max_radius = max_radius;
                return;
            }
        }
        // Recycle slot 0 if pool full
        rings[0] = {pos, col, 0.0f, max_life, max_radius, true};
    }

    void update(float dt) {
        for (auto& r : rings) {
            if (!r.active) continue;
            r.life += dt;
            if (r.life >= r.max_life) r.active = false;
        }
    }

    void draw(const SceneShader& shader, const TorusMesh& torus) const {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        shader.set_shading_mode(4);
        shader.set_inv_scale(1.0f, 1.0f, 1.0f);

        for (const auto& r : rings) {
            if (!r.active) continue;
            float t = r.life / r.max_life;
            float rad = 0.45f + (r.max_radius - 0.45f) * (1.0f - (1.0f - t) * (1.0f - t));
            float alpha = (1.0f - t) * 0.85f;
            Mat4 m = Mat4::translate(r.pos) * Mat4::scale(rad, 0.45f, rad);
            shader.set_model(m);
            shader.set_color(r.color.r, r.color.g, r.color.b, alpha);
            shader.set_emission(1.6f);
            torus.draw();
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
};

// =============================================================================
// 3D NEON ENERGY TRAIL RIBBON & GROUND CONTACT SHADOW
// =============================================================================

class TrailRibbon {
public:
    static constexpr size_t MAX_POINTS = 18;

    GLuint vao{0};
    GLuint vbo{0};
    GLuint shadow_vao{0};
    GLuint shadow_vbo{0};
    std::array<Vec3, MAX_POINTS> points{};
    std::array<ColorVertex, MAX_POINTS * 2> ribbon_verts{};
    bool initialized_points{false};

    void init() {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(ribbon_verts)), nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(offsetof(ColorVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(offsetof(ColorVertex, u)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(offsetof(ColorVertex, r)));

        glGenVertexArrays(1, &shadow_vao);
        glBindVertexArray(shadow_vao);
        glGenBuffers(1, &shadow_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, shadow_vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(4 * sizeof(ColorVertex)), nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(offsetof(ColorVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(offsetof(ColorVertex, u)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(offsetof(ColorVertex, r)));
        glBindVertexArray(0);
    }

    void reset(const Vec3& origin) {
        for (size_t i = 0; i < MAX_POINTS; ++i) {
            points[i] = {origin.x, origin.y, origin.z - static_cast<float>(i) * 0.35f};
        }
        initialized_points = true;
    }

    void update(const Vec3& player_pos, float forward_speed, float dt) {
        if (!initialized_points) {
            reset(player_pos);
            return;
        }
        const float dz = forward_speed * dt;
        for (size_t i = MAX_POINTS - 1; i > 0; --i) {
            points[i] = points[i - 1];
            points[i].z -= dz;
        }
        points[0] = {player_pos.x, std::max(0.08f, player_pos.y - 0.28f), player_pos.z - 0.25f};
    }

    void draw_trail(const ParticleShader& shader, const Mat4& vp, const Vec2& curve, const Color3& col, float intensity) {
        if (!vao || !initialized_points) return;

        for (size_t i = 0; i < MAX_POINTS; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(MAX_POINTS - 1);
            float half_w = (1.0f - t) * 0.44f * intensity;
            float alpha = (1.0f - t) * (1.0f - t) * 0.76f * intensity;
            const Vec3& p = points[i];
            ribbon_verts[i * 2 + 0] = {{p.x - half_w, p.y, p.z}, 0.0f, t, col.r, col.g, col.b, alpha};
            ribbon_verts[i * 2 + 1] = {{p.x + half_w, p.y, p.z}, 1.0f, t, col.r, col.g, col.b, alpha};
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        shader.use();
        shader.set_vp(vp);
        shader.set_curve(curve.x, curve.y);
        shader.set_mode(1);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(ribbon_verts)), ribbon_verts.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(MAX_POINTS * 2));

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void draw_contact_shadow(const ParticleShader& shader, const Mat4& vp, const Vec2& curve, const Vec3& player_pos, float jump_y) const {
        if (!shadow_vao) return;
        float scale = std::clamp(1.0f - jump_y * 0.16f, 0.35f, 1.0f);
        float alpha = std::clamp(0.55f - jump_y * 0.14f, 0.12f, 0.55f);
        float r = 0.88f * scale;
        float y = 0.028f;

        std::array<ColorVertex, 4> sv = {{
            {{player_pos.x - r, y, player_pos.z + r}, 0.0f, 1.0f, 0.0f, 0.0f, 0.02f, alpha},
            {{player_pos.x + r, y, player_pos.z + r}, 1.0f, 1.0f, 0.0f, 0.0f, 0.02f, alpha},
            {{player_pos.x - r, y, player_pos.z - r}, 0.0f, 0.0f, 0.0f, 0.0f, 0.02f, alpha},
            {{player_pos.x + r, y, player_pos.z - r}, 1.0f, 0.0f, 0.0f, 0.0f, 0.02f, alpha}
        }};

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        shader.use();
        shader.set_vp(vp);
        shader.set_curve(curve.x, curve.y);
        shader.set_mode(2);

        glBindVertexArray(shadow_vao);
        glBindBuffer(GL_ARRAY_BUFFER, shadow_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(sv)), sv.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void destroy() {
        if (vao) { glDeleteVertexArrays(1, &vao); glDeleteBuffers(1, &vbo); vao = vbo = 0; }
        if (shadow_vao) { glDeleteVertexArrays(1, &shadow_vao); glDeleteBuffers(1, &shadow_vbo); shadow_vao = shadow_vbo = 0; }
    }
};

// =============================================================================
// BATCHED PARTICLE SYSTEM (1 SINGLE DRAW CALL FOR UP TO 160 PARTICLES!)
// =============================================================================

struct SparkParticle {
    Vec3  pos{};
    Vec3  vel{};
    Vec3  color{};
    float size{0.15f};
    float life{0.0f};
    float max_life{1.0f};
    bool  active{false};
};

class ParticleSystem {
public:
    static constexpr size_t POOL_SIZE = 160;

    std::array<SparkParticle, POOL_SIZE>   particles{};
    std::array<ColorVertex, POOL_SIZE * 4> batch_verts{};
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    size_t active_limit{POOL_SIZE};

    void set_quality_limit(int quality) {
        if (quality <= 0)      active_limit = 64;
        else if (quality == 1) active_limit = 112;
        else                   active_limit = POOL_SIZE;
    }

    void init() {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(batch_verts)), nullptr, GL_DYNAMIC_DRAW);

        std::vector<GLushort> indices;
        indices.reserve(POOL_SIZE * 6);
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            GLushort b = static_cast<GLushort>(i * 4);
            indices.push_back(static_cast<GLushort>(b + 0));
            indices.push_back(static_cast<GLushort>(b + 1));
            indices.push_back(static_cast<GLushort>(b + 2));
            indices.push_back(static_cast<GLushort>(b + 0));
            indices.push_back(static_cast<GLushort>(b + 2));
            indices.push_back(static_cast<GLushort>(b + 3));
        }

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLushort)), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(offsetof(ColorVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(offsetof(ColorVertex, u)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(offsetof(ColorVertex, r)));
        glBindVertexArray(0);
    }

    void spawn_explosion(const Vec3& origin) {
        size_t count = std::min<size_t>(56, active_limit);
        for (size_t i = 0; i < count; ++i) {
            auto& p = particles[i];
            p.active = true;
            p.pos = origin;
            float angle = (static_cast<float>(i) / static_cast<float>(count)) * 2.0f * PI;
            float speed = 4.0f + static_cast<float>(i % 7) * 1.5f;
            p.vel = {std::cos(angle) * speed, 2.2f + static_cast<float>((i * 3) % 8), -3.0f + std::sin(angle) * speed};
            if (i % 3 == 0)      p.color = {1.0f, 0.85f, 0.1f};
            else if (i % 3 == 1) p.color = {1.0f, 0.2f, 0.4f};
            else                 p.color = {0.1f, 0.9f, 1.0f};
            p.size = 0.22f + static_cast<float>(i % 4) * 0.06f;
            p.life = 0.0f;
            p.max_life = 0.6f + static_cast<float>(i % 5) * 0.12f;
        }
    }

    void spawn_orb_burst(const Vec3& origin, const Vec3& burst_color = {1.0f, 0.88f, 0.15f}) {
        size_t spawned = 0;
        for (size_t i = 0; i < active_limit && spawned < 16; ++i) {
            auto& p = particles[i];
            if (p.active) continue;
            p.active = true;
            p.pos = origin;
            float angle = (static_cast<float>(spawned) / 16.0f) * 2.0f * PI;
            float spd = 3.5f + static_cast<float>(spawned % 4) * 1.2f;
            p.vel = {std::cos(angle) * spd, 2.5f + static_cast<float>(spawned % 3) * 1.1f, std::sin(angle) * spd - 4.0f};
            p.color = burst_color;
            p.size = 0.18f;
            p.life = 0.0f;
            p.max_life = 0.38f;
            ++spawned;
        }
    }

    void spawn_shield_shatter(const Vec3& origin) {
        size_t spawned = 0;
        for (size_t i = 0; i < active_limit && spawned < 30; ++i) {
            auto& p = particles[i];
            if (p.active) continue;
            p.active = true;
            p.pos = origin;
            float angle = (static_cast<float>(spawned) / 30.0f) * 2.0f * PI;
            float spd = 5.5f + static_cast<float>(spawned % 5) * 1.6f;
            p.vel = {std::cos(angle) * spd, 3.0f + static_cast<float>(spawned % 4) * 1.2f, std::sin(angle) * spd};
            p.color = (spawned % 2 == 0) ? Vec3{0.0f, 0.95f, 1.0f} : Vec3{1.0f, 0.3f, 0.9f};
            p.size = 0.24f;
            p.life = 0.0f;
            p.max_life = 0.50f;
            ++spawned;
        }
    }

    void spawn_trail_sparks(const Vec3& origin, float lateral_vel, const Vec3& spark_col = {0.2f, 0.95f, 1.0f}) {
        size_t spawned = 0;
        for (size_t i = 0; i < active_limit && spawned < 2; ++i) {
            auto& p = particles[i];
            if (!p.active) {
                p.active = true;
                p.pos = origin;
                p.pos.y = std::max(0.10f, origin.y - 0.55f);
                p.vel = {-lateral_vel * 0.3f + (static_cast<float>(spawned) - 0.5f) * 1.5f,
                         1.2f + static_cast<float>(spawned) * 0.5f, -12.0f};
                p.color = spark_col;
                p.size = 0.14f;
                p.life = 0.0f;
                p.max_life = 0.25f;
                ++spawned;
            }
        }
    }

    void update(float dt) {
        for (auto& p : particles) {
            if (!p.active) continue;
            p.life += dt;
            if (p.life >= p.max_life) { p.active = false; continue; }
            p.pos += p.vel * dt;
            p.vel.y -= 14.0f * dt;
            if (p.pos.y < 0.05f) {
                p.pos.y = 0.05f;
                p.vel.y = -p.vel.y * 0.3f;
                p.vel.x *= 0.8f;
                p.vel.z *= 0.8f;
            }
        }
    }

    void draw(const ParticleShader& shader, const Mat4& vp, const Vec2& curve) {
        if (!vao) return;
        size_t active_count = 0;
        for (size_t i = 0; i < active_limit; ++i) {
            const auto& p = particles[i];
            if (!p.active) continue;
            float alpha = 1.0f - (p.life / p.max_life);
            float hs = p.size * 0.5f;
            size_t base = active_count * 4;
            batch_verts[base + 0] = {{p.pos.x - hs, p.pos.y - hs, p.pos.z}, 0.0f, 1.0f, p.color.x, p.color.y, p.color.z, alpha};
            batch_verts[base + 1] = {{p.pos.x + hs, p.pos.y - hs, p.pos.z}, 1.0f, 1.0f, p.color.x, p.color.y, p.color.z, alpha};
            batch_verts[base + 2] = {{p.pos.x + hs, p.pos.y + hs, p.pos.z}, 1.0f, 0.0f, p.color.x, p.color.y, p.color.z, alpha};
            batch_verts[base + 3] = {{p.pos.x - hs, p.pos.y + hs, p.pos.z}, 0.0f, 0.0f, p.color.x, p.color.y, p.color.z, alpha};
            ++active_count;
        }
        if (active_count == 0) return;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        shader.use();
        shader.set_vp(vp);
        shader.set_curve(curve.x, curve.y);
        shader.set_mode(0);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(active_count * 4 * sizeof(ColorVertex)), batch_verts.data());
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(active_count * 6), GL_UNSIGNED_SHORT, nullptr);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void destroy() {
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            vao = vbo = ebo = 0;
        }
    }
};

// =============================================================================
// CRISP HIGH-RES ANTI-ALIASED SMOOTH VECTOR FONT & GLASSMORPHIC HUD RENDERER
// =============================================================================

inline const uint8_t FONT_5X7[64][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00},
    {0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7F, 0x14, 0x7F, 0x14},
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00},
    {0x00, 0x1C, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1C, 0x00},
    {0x14, 0x08, 0x3E, 0x08, 0x14}, {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x60, 0x60, 0x00, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x56, 0x36, 0x00, 0x00},
    {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x32, 0x49, 0x79, 0x41, 0x3E}, {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01}, {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40}, {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x7F, 0x20, 0x18, 0x20, 0x7F},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7F, 0x41, 0x41, 0x00},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7F, 0x00},
    {0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40}
};

class HudRenderer {
public:
    static constexpr int MAX_CHARS = 768;
    static constexpr int ATLAS_W   = 512;
    static constexpr int ATLAS_H   = 256;
    static constexpr int CELL_W    = 32;
    static constexpr int CELL_H    = 64;

    GLuint font_texture{0};
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    QuadMesh box_mesh;
    std::array<Vertex, MAX_CHARS * 4> text_verts{};

    static float dist_point_to_seg(float px, float py, float ax, float ay, float bx, float by) {
        float vx = bx - ax, vy = by - ay;
        float wx = px - ax, wy = py - ay;
        float len2 = vx * vx + vy * vy;
        float t = (len2 > 1e-5f) ? std::clamp((wx * vx + wy * vy) / len2, 0.0f, 1.0f) : 0.0f;
        float dx = px - (ax + t * vx);
        float dy = py - (ay + t * vy);
        return std::sqrt(dx * dx + dy * dy);
    }

    void init() {
        // Generate a crisp 512x256 smooth SDF-like anti-aliased vector glyph atlas!
        std::vector<uint8_t> tex_data(static_cast<size_t>(ATLAS_W * ATLAS_H), 0);

        for (int i = 0; i < 64; ++i) {
            int cx = (i % 16) * CELL_W;
            int cy = (i / 16) * CELL_H;

            bool grid[5][7]{};
            for (int col = 0; col < 5; ++col) {
                uint8_t bits = FONT_5X7[i][col];
                for (int r = 0; r < 7; ++r) {
                    grid[col][r] = ((bits >> r) & 1) != 0;
                }
            }

            // Collect smooth stroke segments between adjacent/diagonal active nodes
            struct Seg { float ax, ay, bx, by; };
            std::vector<Seg> segs;
            segs.reserve(32);

            auto node_x = [](int col) { return 4.5f + static_cast<float>(col) * 5.75f; };
            auto node_y = [](int row) { return 11.0f + static_cast<float>(row) * 7.0f; };

            for (int c = 0; c < 5; ++c) {
                for (int r = 0; r < 7; ++r) {
                    if (!grid[c][r]) continue;
                    float ax = node_x(c), ay = node_y(r);
                    bool linked = false;
                    // Connect right, down, down-right, down-left for sleek geometric strokes
                    if (c + 1 < 5 && grid[c + 1][r]) { segs.push_back({ax, ay, node_x(c + 1), ay}); linked = true; }
                    if (r + 1 < 7 && grid[c][r + 1]) { segs.push_back({ax, ay, ax, node_y(r + 1)}); linked = true; }
                    if (c + 1 < 5 && r + 1 < 7 && grid[c + 1][r + 1] && !grid[c + 1][r] && !grid[c][r + 1]) {
                        segs.push_back({ax, ay, node_x(c + 1), node_y(r + 1)}); linked = true;
                    }
                    if (c - 1 >= 0 && r + 1 < 7 && grid[c - 1][r + 1] && !grid[c - 1][r] && !grid[c][r + 1]) {
                        segs.push_back({ax, ay, node_x(c - 1), node_y(r + 1)}); linked = true;
                    }
                    if (!linked) {
                        segs.push_back({ax, ay, ax, ay});
                    }
                }
            }

            if (segs.empty()) continue;

            for (int py = 0; py < CELL_H; ++py) {
                for (int px = 0; px < CELL_W; ++px) {
                    float min_d = 100.0f;
                    float fx = static_cast<float>(px) + 0.5f;
                    float fy = static_cast<float>(py) + 0.5f;
                    for (const auto& s : segs) {
                        float d = dist_point_to_seg(fx, fy, s.ax, s.ay, s.bx, s.by);
                        if (d < min_d) min_d = d;
                    }
                    // Smooth SDF falloff around stroke radius 2.55px
                    float val = std::clamp(1.0f - (min_d - 1.55f) / 2.2f, 0.0f, 1.0f);
                    tex_data[static_cast<size_t>((cy + py) * ATLAS_W + (cx + px))] =
                        static_cast<uint8_t>(val * 255.0f);
                }
            }
        }

        glGenTextures(1, &font_texture);
        glBindTexture(GL_TEXTURE_2D, font_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_W, ATLAS_H, 0, GL_RED, GL_UNSIGNED_BYTE, tex_data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(text_verts)), nullptr, GL_DYNAMIC_DRAW);

        std::vector<GLushort> indices;
        indices.reserve(MAX_CHARS * 6);
        for (int i = 0; i < MAX_CHARS; ++i) {
            GLushort b = static_cast<GLushort>(i * 4);
            indices.push_back(static_cast<GLushort>(b + 0));
            indices.push_back(static_cast<GLushort>(b + 1));
            indices.push_back(static_cast<GLushort>(b + 2));
            indices.push_back(static_cast<GLushort>(b + 0));
            indices.push_back(static_cast<GLushort>(b + 2));
            indices.push_back(static_cast<GLushort>(b + 3));
        }

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLushort)), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        glBindVertexArray(0);

        box_mesh.init(1.0f, 1.0f);
    }

    static void begin_hud(const UiShader& ui_shader) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        ui_shader.use();
    }

    static void end_hud() {
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    static float measure_text_width(const char* text, float char_w) {
        if (!text || *text == '\0') return 0.0f;
        return static_cast<float>(std::strlen(text)) * char_w * 1.08f;
    }

    void draw_text(const UiShader& shader, const Mat4& ortho_vp,
                   float start_x, float start_y, float char_w, float char_h,
                   const char* text, float r, float g, float b, float a = 1.0f) {
        if (!text || !vao) return;

        float cur_x = start_x;
        int count = 0;

        for (const char* p = text; *p != '\0' && count < MAX_CHARS; ++p) {
            char c = *p;
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);

            if (c == '\n') {
                cur_x = start_x;
                start_y += char_h * 1.32f;
                continue;
            }

            int idx = (c >= 32 && c <= 95) ? (c - 32) : 0;
            float u0 = static_cast<float>((idx % 16) * CELL_W) / static_cast<float>(ATLAS_W);
            float u1 = static_cast<float>((idx % 16 + 1) * CELL_W) / static_cast<float>(ATLAS_W);
            float v0 = static_cast<float>((idx / 16) * CELL_H) / static_cast<float>(ATLAS_H);
            float v1 = static_cast<float>((idx / 16 + 1) * CELL_H) / static_cast<float>(ATLAS_H);

            float x0 = cur_x;
            float x1 = cur_x + char_w;
            float y0 = start_y;
            float y1 = start_y + char_h;

            size_t base = static_cast<size_t>(count) * 4;
            text_verts[base + 0] = {{x0, y1, 0.0f}, {0, 0, 1}, u0, v1};
            text_verts[base + 1] = {{x1, y1, 0.0f}, {0, 0, 1}, u1, v1};
            text_verts[base + 2] = {{x1, y0, 0.0f}, {0, 0, 1}, u1, v0};
            text_verts[base + 3] = {{x0, y0, 0.0f}, {0, 0, 1}, u0, v0};

            cur_x += char_w * 1.08f;
            ++count;
        }

        if (count == 0) return;

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(static_cast<size_t>(count) * 4 * sizeof(Vertex)), text_verts.data());

        shader.set_mode(0);
        shader.set_mvp(ortho_vp);
        shader.set_color(r, g, b, a);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, font_texture);
        shader.set_texture_unit(0);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, count * 6, GL_UNSIGNED_SHORT, nullptr);
    }

    void draw_text_centered(const UiShader& shader, const Mat4& ortho_vp,
                            float center_x, float y, float char_w, float char_h,
                            const char* text, float r, float g, float b, float a = 1.0f) {
        if (!text) return;
        float w = measure_text_width(text, char_w);
        draw_text(shader, ortho_vp, center_x - w * 0.5f, y, char_w, char_h, text, r, g, b, a);
    }

    void draw_text_shadow(const UiShader& shader, const Mat4& ortho_vp,
                          float center_x, float y, float char_w, float char_h,
                          const char* text, float r, float g, float b, float a = 1.0f) {
        draw_text_centered(shader, ortho_vp, center_x + 2.0f, y + 2.0f, char_w, char_h, text, 0.0f, 0.0f, 0.06f, a * 0.68f);
        draw_text_centered(shader, ortho_vp, center_x, y, char_w, char_h, text, r, g, b, a);
    }

    void draw_box(const UiShader& shader, const Mat4& ortho_vp,
                  float x, float y, float w, float h,
                  float r, float g, float b, float a) const {
        shader.set_mode(2);
        Mat4 model = Mat4::translate(x + w * 0.5f, y + h * 0.5f, 0.0f) * Mat4::scale(w, h, 1.0f);
        shader.set_mvp(ortho_vp * model);
        shader.set_color(r, g, b, a);
        box_mesh.draw();
    }

    void draw_panel(const UiShader& shader, const Mat4& ortho_vp,
                    const Rect2D& rect, float radius, float border_w,
                    float top_r, float top_g, float top_b, float top_a,
                    float bot_r, float bot_g, float bot_b, float bot_a,
                    float brd_r, float brd_g, float brd_b, float brd_a) const {
        shader.set_mode(1);
        Mat4 model = Mat4::translate(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, 0.0f) *
                     Mat4::scale(rect.w, rect.h, 1.0f);
        shader.set_mvp(ortho_vp * model);
        shader.set_top_color(top_r, top_g, top_b, top_a);
        shader.set_color(bot_r, bot_g, bot_b, bot_a);
        shader.set_border_color(brd_r, brd_g, brd_b, brd_a);
        shader.set_rect_params(rect.w, rect.h, radius, border_w);
        box_mesh.draw();
    }

    void draw_button(const UiShader& shader, const Mat4& ortho_vp,
                     const Rect2D& rect, const char* label,
                     float char_w, float char_h, float radius,
                     const Color3& accent, bool filled = false) {
        if (filled) {
            draw_panel(shader, ortho_vp, rect, radius, 2.0f,
                       accent.r * 0.36f, accent.g * 0.36f, accent.b * 0.36f, 0.94f,
                       accent.r * 0.16f, accent.g * 0.16f, accent.b * 0.16f, 0.96f,
                       accent.r, accent.g, accent.b, 1.0f);
            draw_text_shadow(shader, ortho_vp,
                             rect.x + rect.w * 0.5f,
                             rect.y + (rect.h - char_h) * 0.5f,
                             char_w, char_h, label, 1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            draw_panel(shader, ortho_vp, rect, radius, 1.6f,
                       0.06f, 0.10f, 0.22f, 0.88f,
                       0.02f, 0.04f, 0.10f, 0.93f,
                       accent.r, accent.g, accent.b, 0.85f);
            draw_text_centered(shader, ortho_vp,
                               rect.x + rect.w * 0.5f,
                               rect.y + (rect.h - char_h) * 0.5f,
                               char_w, char_h, label, accent.r, accent.g, accent.b, 1.0f);
        }
    }

    void draw_progress_bar(const UiShader& shader, const Mat4& ortho_vp,
                           const Rect2D& rect, float progress, float radius,
                           const Color3& fill_col) const {
        draw_panel(shader, ortho_vp, rect, radius, 1.2f,
                   0.04f, 0.07f, 0.14f, 0.90f,
                   0.02f, 0.03f, 0.08f, 0.92f,
                   fill_col.r * 0.5f, fill_col.g * 0.5f, fill_col.b * 0.5f, 0.65f);
        float p = std::clamp(progress, 0.0f, 1.0f);
        if (p > 0.01f) {
            Rect2D inner = {rect.x + 2.0f, rect.y + 2.0f,
                            std::max(radius, (rect.w - 4.0f) * p),
                            std::max(2.0f, rect.h - 4.0f)};
            draw_panel(shader, ortho_vp, inner, std::max(1.0f, radius - 1.5f), 0.0f,
                       fill_col.r, fill_col.g, fill_col.b, 0.98f,
                       fill_col.r * 0.65f, fill_col.g * 0.65f, fill_col.b * 0.65f, 0.98f,
                       0.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    void destroy() {
        if (font_texture) { glDeleteTextures(1, &font_texture); font_texture = 0; }
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            vao = vbo = ebo = 0;
        }
        box_mesh.destroy();
    }
};

} // namespace velo
