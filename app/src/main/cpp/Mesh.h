#pragma once

#include <GLES3/gl3.h>
#include <vector>
#include <cmath>
#include <cstddef>
#include "Math3D.h"
#include "Shader.h"

namespace velo {

/**
 * Vertex structure for GLES 3.0.
 * Default initializers are provided to satisfy Clang-Tidy warnings,
 * though fields are primarily used for raw GPU buffer uploads.
 */
struct Vertex {
    Vec3  position{};
    Vec3  normal{};
    float u{0.0f};
    float v{0.0f};
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

    void init(float radius = 0.75f, int sectors = 24, int stacks = 16) {
        std::vector<Vertex> vertices;
        std::vector<GLushort> indices;

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
            glBindVertexArray(0);
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
// PROCEDURAL 3D RECTANGULAR BOX MESH (OBSTACLES & BARRIERS)
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
            // Front (+Z)
            {{-hx, -hy,  hz}, {0, 0,  1}, 0.0f, 0.0f},
            {{ hx, -hy,  hz}, {0, 0,  1}, 1.0f, 0.0f},
            {{ hx,  hy,  hz}, {0, 0,  1}, 1.0f, 1.0f},
            {{-hx,  hy,  hz}, {0, 0,  1}, 0.0f, 1.0f},
            // Back (-Z)
            {{ hx, -hy, -hz}, {0, 0, -1}, 0.0f, 0.0f},
            {{-hx, -hy, -hz}, {0, 0, -1}, 1.0f, 0.0f},
            {{-hx,  hy, -hz}, {0, 0, -1}, 1.0f, 1.0f},
            {{ hx,  hy, -hz}, {0, 0, -1}, 0.0f, 1.0f},
            // Left (-X)
            {{-hx, -hy, -hz}, {-1, 0, 0}, 0.0f, 0.0f},
            {{-hx, -hy,  hz}, {-1, 0, 0}, 1.0f, 0.0f},
            {{-hx,  hy,  hz}, {-1, 0, 0}, 1.0f, 1.0f},
            {{-hx,  hy, -hz}, {-1, 0, 0}, 0.0f, 1.0f},
            // Right (+X)
            {{ hx, -hy,  hz}, {1, 0, 0}, 0.0f, 0.0f},
            {{ hx, -hy, -hz}, {1, 0, 0}, 1.0f, 0.0f},
            {{ hx,  hy, -hz}, {1, 0, 0}, 1.0f, 1.0f},
            {{ hx,  hy,  hz}, {1, 0, 0}, 0.0f, 1.0f},
            // Top (+Y)
            {{-hx,  hy,  hz}, {0, 1, 0}, 0.0f, 0.0f},
            {{ hx,  hy,  hz}, {0, 1, 0}, 1.0f, 0.0f},
            {{ hx,  hy, -hz}, {0, 1, 0}, 1.0f, 1.0f},
            {{-hx,  hy, -hz}, {0, 1, 0}, 0.0f, 1.0f},
            // Bottom (-Y)
            {{-hx, -hy, -hz}, {0, -1, 0}, 0.0f, 0.0f},
            {{ hx, -hy, -hz}, {0, -1, 0}, 1.0f, 0.0f},
            {{ hx, -hy,  hz}, {0, -1, 0}, 1.0f, 1.0f},
            {{-hx, -hy,  hz}, {0, -1, 0}, 0.0f, 1.0f}
        };

        std::vector<GLushort> indices;
        for (GLushort i = 0; i < 6; ++i) {
            GLushort base = i * 4;
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
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
            glBindVertexArray(0);
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
// PROCEDURAL RUNWAY TRACK PLANE MESH
// =============================================================================

class TrackMesh {
public:
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    GLsizei index_count{0};

    void init(float width = 8.0f, float length = 10.0f) {
        float hx = width * 0.5f;
        float hz = length * 0.5f;

        std::vector<Vertex> vertices = {
            {{-hx, 0.0f,  hz}, {0, 1, 0}, 0.0f, 0.0f},
            {{ hx, 0.0f,  hz}, {0, 1, 0}, 1.0f, 0.0f},
            {{ hx, 0.0f, -hz}, {0, 1, 0}, 1.0f, 1.0f},
            {{-hx, 0.0f, -hz}, {0, 1, 0}, 0.0f, 1.0f}
        };

        std::vector<GLushort> indices = {0, 1, 2, 0, 2, 3};
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
            glBindVertexArray(0);
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
// PROCEDURAL 2D/3D QUAD MESH
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

        std::vector<Vertex> vertices = {
            {{-hx, -hy, 0.0f}, {0, 0, 1}, 0.0f, 1.0f},
            {{ hx, -hy, 0.0f}, {0, 0, 1}, 1.0f, 1.0f},
            {{ hx,  hy, 0.0f}, {0, 0, 1}, 1.0f, 0.0f},
            {{-hx,  hy, 0.0f}, {0, 0, 1}, 0.0f, 0.0f}
        };

        std::vector<GLushort> indices = {0, 1, 2, 0, 2, 3};
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
            glBindVertexArray(0);
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
// ARCADE HUD RENDERER (NEON TEXT & UI GLYPHS)
// =============================================================================

inline const uint8_t FONT_5X7[64][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 ' '
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 '!'
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 '"'
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 '#'
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 '$'
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 '%'
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 '&'
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '''
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 '('
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 ')'
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 '*'
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 '+'
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ','
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 '-'
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 '.'
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 '/'
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 '9'
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 ':'
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ';'
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 '<'
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 '='
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 '>'
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 '?'
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 '@'
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 'A'
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 'E'
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 'F'
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 'G'
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 'I'
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 'J'
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 'K'
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 'L'
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 77 'M'
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 'P'
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 'Q'
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 'U'
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 'V'
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // 87 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 'X'
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 'Z'
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 '['
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 '\'
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ']'
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 '^'
    {0x40, 0x40, 0x40, 0x40, 0x40}  // 95 '_'
};

class HudRenderer {
public:
    GLuint font_texture{0};
    GLuint vao{0};
    GLuint vbo{0};
    GLuint ebo{0};
    QuadMesh box_mesh;

    static constexpr int MAX_CHARS = 128;

    void init() {
        // Generate procedural font texture atlas (128x64)
        uint8_t tex_data[128 * 64];
        std::fill_n(tex_data, 128 * 64, 0);

        for (int i = 0; i < 64; ++i) {
            int cell_x = (i % 16) * 8;
            int cell_y = (i / 16) * 16;

            for (int col = 0; col < 5; ++col) {
                uint8_t bits = FONT_5X7[i][col];
                for (int r = 0; r < 7; ++r) {
                    if ((bits >> r) & 1) {
                        int px = cell_x + 1 + col;
                        int py = cell_y + 4 + r;
                        tex_data[py * 128 + px] = 255;
                    }
                }
            }
        }

        glGenTextures(1, &font_texture);
        glBindTexture(GL_TEXTURE_2D, font_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 128, 64, 0, GL_RED, GL_UNSIGNED_BYTE, tex_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Dynamic buffer for batching quads
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, MAX_CHARS * 4 * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

        std::vector<GLushort> indices;
        indices.reserve(MAX_CHARS * 6);
        for (int i = 0; i < MAX_CHARS; ++i) {
            GLushort b = static_cast<GLushort>(i * 4);
            indices.push_back(b + 0);
            indices.push_back(b + 1);
            indices.push_back(b + 2);
            indices.push_back(b + 0);
            indices.push_back(b + 2);
            indices.push_back(b + 3);
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

    void draw_text(const ShaderProgram& shader, const Mat4& ortho_vp,
                   float start_x, float start_y, float char_w, float char_h,
                   const char* text, float r, float g, float b, float a = 1.0f) {
        if (!text || !vao) return;

        std::vector<Vertex> verts;
        float cur_x = start_x;
        int count = 0;

        for (const char* p = text; *p != '\0' && count < MAX_CHARS; ++p) {
            char c = *p;
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);

            if (c == '\n') {
                cur_x = start_x;
                start_y += char_h * 1.35f;
                continue;
            }

            int idx = (c >= 32 && c <= 95) ? (c - 32) : 0;
            float u0 = static_cast<float>((idx % 16) * 8) / 128.0f;
            float u1 = static_cast<float>((idx % 16 + 1) * 8) / 128.0f;
            float v0 = static_cast<float>((idx / 16) * 16) / 64.0f;
            float v1 = static_cast<float>((idx / 16 + 1) * 16) / 64.0f;

            float x0 = cur_x;
            float x1 = cur_x + char_w;
            float y0 = start_y;
            float y1 = start_y + char_h;

            verts.push_back({ {x0, y1, 0.0f}, {0, 0, 1}, u0, v1 });
            verts.push_back({ {x1, y1, 0.0f}, {0, 0, 1}, u1, v1 });
            verts.push_back({ {x1, y0, 0.0f}, {0, 0, 1}, u1, v0 });
            verts.push_back({ {x0, y0, 0.0f}, {0, 0, 1}, u0, v0 });

            cur_x += char_w * 1.12f;
            count++;
        }

        if (count == 0) return;

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)), verts.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        shader.use();
        shader.set_shading_mode(4); // HUD Text Mode
        shader.set_mvp(ortho_vp);
        shader.set_color(r, g, b, a);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, font_texture);
        shader.set_texture_unit(0);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, count * 6, GL_UNSIGNED_SHORT, nullptr);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    void draw_box(const ShaderProgram& shader, const Mat4& ortho_vp,
                  float x, float y, float w, float h,
                  float r, float g, float b, float a) const {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        shader.use();
        shader.set_shading_mode(5); // Solid HUD Quad Mode
        Mat4 model = Mat4::translate(x + w * 0.5f, y + h * 0.5f, 0.0f) * Mat4::scale(w, h, 1.0f);
        shader.set_mvp(ortho_vp * model);
        shader.set_color(r, g, b, a);

        box_mesh.draw();

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    void destroy() {
        if (font_texture) {
            glDeleteTextures(1, &font_texture);
            font_texture = 0;
        }
        if (vao) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            vao = vbo = ebo = 0;
        }
        box_mesh.destroy();
    }
};

// =============================================================================
// SPARK & IMPACT PARTICLE SYSTEM
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
    static constexpr size_t POOL_SIZE = 64;
    std::array<SparkParticle, POOL_SIZE> particles{};
    QuadMesh quad;

    void init() {
        quad.init(1.0f, 1.0f);
    }

    void spawn_explosion(const Vec3& origin) {
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            auto& p = particles[i];
            p.active = true;
            p.pos = origin;
            float angle = (static_cast<float>(i) / static_cast<float>(POOL_SIZE)) * 2.0f * PI;
            float speed = 4.0f + static_cast<float>(i % 7) * 1.5f;

            p.vel = {
                std::cos(angle) * speed,
                2.0f + static_cast<float>((i * 3) % 8) * 1.0f,
                -3.0f + std::sin(angle) * speed
            };

            // Electric neon fire colors (yellow, amber, magenta, cyan)
            if (i % 3 == 0)      p.color = {1.0f, 0.85f, 0.1f};
            else if (i % 3 == 1) p.color = {1.0f, 0.2f, 0.4f};
            else                 p.color = {0.1f, 0.9f, 1.0f};

            p.size = 0.20f + static_cast<float>(i % 4) * 0.06f;
            p.life = 0.0f;
            p.max_life = 0.6f + static_cast<float>(i % 5) * 0.12f;
        }
    }

    void spawn_trail_sparks(const Vec3& origin, float lateral_vel) {
        // Spawn 2 subtle friction sparks on hard turns
        size_t spawned = 0;
        for (auto& p : particles) {
            if (!p.active && spawned < 2) {
                p.active = true;
                p.pos = origin;
                p.pos.y = 0.10f; // Track surface contact
                p.vel = {
                    -lateral_vel * 0.3f + (static_cast<float>(spawned) - 0.5f) * 1.5f,
                    1.2f + static_cast<float>(spawned) * 0.5f,
                    -12.0f // fly backward relative to treadmill
                };
                p.color = {0.2f, 0.95f, 1.0f};
                p.size = 0.12f;
                p.life = 0.0f;
                p.max_life = 0.25f;
                spawned++;
            }
        }
    }

    void update(float dt) {
        for (auto& p : particles) {
            if (!p.active) continue;
            p.life += dt;
            if (p.life >= p.max_life) {
                p.active = false;
                continue;
            }

            p.pos += p.vel * dt;
            p.vel.y -= 14.0f * dt; // gravity
            if (p.pos.y < 0.05f) {
                p.pos.y = 0.05f;
                p.vel.y = -p.vel.y * 0.3f;
                p.vel.x *= 0.8f;
                p.vel.z *= 0.8f;
            }
        }
    }

    void draw(const ShaderProgram& shader, const Mat4& vp) const {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for neon sparks
        glDepthMask(GL_FALSE);

        shader.use();
        shader.set_shading_mode(6); // Radial spark mode

        for (const auto& p : particles) {
            if (!p.active) continue;
            float alpha = 1.0f - (p.life / p.max_life);

            Mat4 model = Mat4::translate(p.pos) * Mat4::scale(p.size, p.size, p.size);
            shader.set_model(model);
            shader.set_mvp(vp * model);
            shader.set_color(p.color.x, p.color.y, p.color.z, alpha);
            quad.draw();
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void destroy() {
        quad.destroy();
    }
};

} // namespace velo
