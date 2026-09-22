#pragma once

#include <GLES3/gl3.h>
#include <android/log.h>
#include <string>
#include "Math3D.h"

namespace velo {

#ifndef LOG_TAG
#define LOG_TAG "VeloSphere"
#endif
#ifndef LOGE
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif
#ifndef LOGI
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__))
#endif

class ShaderProgram {
public:
    GLuint program_id{0};

    // Uniform locations
    GLint loc_mvp{-1};
    GLint loc_model{-1};
    GLint loc_color{-1};
    GLint loc_light_pos{-1};
    GLint loc_view_pos{-1};
    GLint loc_emission{-1};
    GLint loc_shading_mode{-1}; // 0 = Lit, 1 = Neon Grid Track, 2 = Glowing Obstacle
    GLint loc_texture{-1};

    bool build(const char* vert_source, const char* frag_source) {
        GLuint vert_shader = compile_shader(GL_VERTEX_SHADER, vert_source);
        if (!vert_shader) return false;

        GLuint frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag_source);
        if (!frag_shader) {
            glDeleteShader(vert_shader);
            return false;
        }

        program_id = glCreateProgram();
        glAttachShader(program_id, vert_shader);
        glAttachShader(program_id, frag_shader);
        glLinkProgram(program_id);

        GLint link_status = 0;
        glGetProgramiv(program_id, GL_LINK_STATUS, &link_status);
        if (!link_status) {
            GLint log_length = 0;
            glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);
            if (log_length > 0) {
                std::string log(static_cast<size_t>(log_length), '\0');
                glGetProgramInfoLog(program_id, log_length, nullptr, &log[0]);
                LOGE("Shader Program Link Failure: %s", log.c_str());
            }
            glDeleteShader(vert_shader);
            glDeleteShader(frag_shader);
            glDeleteProgram(program_id);
            program_id = 0;
            return false;
        }

        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);

        // Cache uniform locations
        loc_mvp          = glGetUniformLocation(program_id, "u_MVP");
        loc_model        = glGetUniformLocation(program_id, "u_Model");
        loc_color        = glGetUniformLocation(program_id, "u_Color");
        loc_light_pos    = glGetUniformLocation(program_id, "u_LightPos");
        loc_view_pos     = glGetUniformLocation(program_id, "u_ViewPos");
        loc_emission     = glGetUniformLocation(program_id, "u_Emission");
        loc_shading_mode = glGetUniformLocation(program_id, "u_ShadingMode");
        loc_texture      = glGetUniformLocation(program_id, "u_Texture");

        LOGI("ShaderProgram: Successfully compiled and linked (ID: %u)", program_id);
        return true;
    }

    void use() const {
        if (program_id) {
            glUseProgram(program_id);
        }
    }

    void set_texture_unit(int unit) const {
        if (loc_texture >= 0) glUniform1i(loc_texture, unit);
    }

    void set_mvp(const Mat4& mat) const {
        if (loc_mvp >= 0) glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, mat.data());
    }

    void set_model(const Mat4& mat) const {
        if (loc_model >= 0) glUniformMatrix4fv(loc_model, 1, GL_FALSE, mat.data());
    }

    void set_color(float r, float g, float b, float a = 1.0f) const {
        if (loc_color >= 0) glUniform4f(loc_color, r, g, b, a);
    }

    void set_light_pos(const Vec3& p) const {
        if (loc_light_pos >= 0) glUniform3f(loc_light_pos, p.x, p.y, p.z);
    }

    void set_view_pos(const Vec3& p) const {
        if (loc_view_pos >= 0) glUniform3f(loc_view_pos, p.x, p.y, p.z);
    }

    void set_emission(float e) const {
        if (loc_emission >= 0) glUniform1f(loc_emission, e);
    }

    void set_shading_mode(int mode) const {
        if (loc_shading_mode >= 0) glUniform1i(loc_shading_mode, mode);
    }

    void destroy() {
        if (program_id) {
            glDeleteProgram(program_id);
            program_id = 0;
        }
    }

private:
    static GLuint compile_shader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint log_length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
            if (log_length > 0) {
                std::string log(static_cast<size_t>(log_length), '\0');
                glGetShaderInfoLog(shader, log_length, nullptr, &log[0]);
                LOGE("Shader Compilation Failed (%s): %s",
                     type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT", log.c_str());
            }
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
};

// =============================================================================
// SHADER SOURCE CODES (OpenGL ES 3.0)
// =============================================================================

inline const char* GLES_VERTEX_SHADER = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_MVP;
uniform mat4 u_Model;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_FragPos = worldPos.xyz;
    v_Normal = normalize(mat3(u_Model) * a_Normal);
    v_TexCoord = a_TexCoord;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
)";

inline const char* GLES_FRAGMENT_SHADER = R"(#version 300 es
precision highp float;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec4 u_Color;
uniform vec3 u_LightPos;
uniform vec3 u_ViewPos;
uniform float u_Emission;
uniform int u_ShadingMode; // 0 = Lit, 1 = Neon Grid Floor, 2 = Glowing Obstacle, 3 = Player Sphere, 4 = HUD Text, 5 = HUD Quad, 6 = Particle
uniform sampler2D u_Texture;

out vec4 fragColor;

void main() {
    if (u_ShadingMode == 4) {
        // --- 2D HUD TEXT RENDERING ---
        float fontAlpha = texture(u_Texture, v_TexCoord).r;
        fragColor = vec4(u_Color.rgb, u_Color.a * fontAlpha);
        return;
    }

    if (u_ShadingMode == 5) {
        // --- 2D UI BANNER / SOLID QUAD ---
        fragColor = u_Color;
        return;
    }

    if (u_ShadingMode == 6) {
        // --- RADIAL GLOW SPARK PARTICLES ---
        vec2 p = v_TexCoord - vec2(0.5);
        float d = length(p);
        float alpha = smoothstep(0.5, 0.05, d);
        fragColor = vec4(u_Color.rgb, u_Color.a * alpha);
        return;
    }

    vec3 norm = normalize(v_Normal);
    vec3 lightDir = normalize(u_LightPos - v_FragPos);
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);

    if (u_ShadingMode == 1) {
        // --- NEON RUNWAY / GRID SHADING ---
        vec2 uv = v_TexCoord;
        float gridX = abs(fract(uv.x * 3.0 - 0.5) - 0.5) / fwidth(uv.x * 3.0);
        float gridZ = abs(fract(uv.y * 2.0 - 0.5) - 0.5) / fwidth(uv.y * 2.0);
        float lineX = 1.0 - min(gridX, 1.0);
        float lineZ = 1.0 - min(gridZ, 1.0);
        float grid = max(lineX, lineZ);

        // Center line pulse
        float centerDist = abs(uv.x - 0.5);
        float centerGlow = smoothstep(0.04, 0.0, centerDist);

        // Edge rails glow
        float edgeGlow = smoothstep(0.42, 0.50, centerDist);

        vec3 baseFloor = vec3(0.03, 0.04, 0.09);
        vec3 gridColor = vec3(0.0, 0.85, 1.0) * grid * 0.45;
        vec3 centerColor = vec3(1.0, 0.35, 0.85) * centerGlow * 0.9;
        vec3 edgeColor = vec3(0.1, 0.95, 0.9) * edgeGlow * 1.2;

        // Depth fog effect along the Z axis
        float fogFactor = clamp((v_FragPos.z - 5.0) / 75.0, 0.0, 1.0);
        vec3 finalTrack = baseFloor + gridColor + centerColor + edgeColor;
        vec3 skyBackground = vec3(0.02, 0.03, 0.06);

        fragColor = vec4(mix(finalTrack, skyBackground, fogFactor), 1.0);
        return;
    }

    if (u_ShadingMode == 2) {
        // --- NEON HAZARD OBSTACLE SHADING ---
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 ambient = 0.35 * u_Color.rgb;
        vec3 diffuse = diff * u_Color.rgb * 0.85;

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
        vec3 specular = vec3(1.0, 0.9, 0.8) * spec * 0.7;

        vec3 emission = u_Color.rgb * u_Emission;
        vec3 result = ambient + diffuse + specular + emission;

        float fogFactor = clamp((v_FragPos.z - 5.0) / 80.0, 0.0, 1.0);
        vec3 skyBackground = vec3(0.02, 0.03, 0.06);
        fragColor = vec4(mix(result, skyBackground, fogFactor), u_Color.a);
        return;
    }

    if (u_ShadingMode == 3) {
        // --- REALISTIC HIGH-TECH PLAYER SPHERE WITH VISIBLE ROLLING BANDS ---
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 ambient = 0.35 * u_Color.rgb;
        vec3 diffuse = diff * vec3(1.0, 0.98, 0.92);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), 48.0);
        vec3 specular = vec3(1.0, 1.0, 1.0) * spec * 0.90;

        // Visible procedural surface markings that roll directly with sphere geometry
        // Latitude bands (roll with forward pitch rotation)
        float latStripes = abs(fract(v_TexCoord.y * 6.0) - 0.5) * 2.0;
        float latBand = smoothstep(0.20, 0.0, latStripes);

        // Glowing equator core line
        float eqDist = abs(v_TexCoord.y - 0.5);
        float eqBand = smoothstep(0.07, 0.0, eqDist);

        // Meridian longitude segments (roll with lateral yaw/roll rotation)
        float lonStripes = abs(fract(v_TexCoord.x * 8.0) - 0.5) * 2.0;
        float lonBand = smoothstep(0.16, 0.0, lonStripes);

        // Dynamic surface energy lines
        vec3 energyColor = vec3(1.0, 0.25, 0.80); // Radiant neon magenta energy
        float patternMask = max(eqBand * 1.5, max(latBand * 0.85, lonBand * 0.65));
        vec3 baseSurface = mix(u_Color.rgb, energyColor, patternMask);

        float totalEmission = u_Emission + patternMask * 1.25;
        vec3 result = (ambient + diffuse + specular) * baseSurface + energyColor * totalEmission;
        fragColor = vec4(result, u_Color.a);
        return;
    }

    // --- STANDARD LIT SHADING (Rails & Props) ---
    float ambientStrength = 0.30;
    vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 0.95, 0.9);

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 48.0);
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * 0.8;

    vec3 result = (ambient + diffuse + specular) * u_Color.rgb + (u_Color.rgb * u_Emission);
    fragColor = vec4(result, u_Color.a);
}
)";

} // namespace velo
