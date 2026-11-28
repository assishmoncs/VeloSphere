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

// =============================================================================
// SHADER COMPILER HELPERS
// =============================================================================

inline GLuint compile_gl_shader(GLenum type, const char* source) {
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

inline GLuint link_gl_program(const char* vert_source, const char* frag_source) {
    GLuint vert_shader = compile_gl_shader(GL_VERTEX_SHADER, vert_source);
    if (!vert_shader) return 0;

    GLuint frag_shader = compile_gl_shader(GL_FRAGMENT_SHADER, frag_source);
    if (!frag_shader) {
        glDeleteShader(vert_shader);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert_shader);
    glAttachShader(prog, frag_shader);
    glLinkProgram(prog);

    GLint link_status = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &link_status);
    if (!link_status) {
        GLint log_length = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_length);
        if (log_length > 0) {
            std::string log(static_cast<size_t>(log_length), '\0');
            glGetProgramInfoLog(prog, log_length, nullptr, &log[0]);
            LOGE("Shader Program Link Failure: %s", log.c_str());
        }
        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);
        glDeleteProgram(prog);
        return 0;
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    return prog;
}

// =============================================================================
// 1. 3D CURVED-WORLD SCENE SHADER (ZERO GPU INVERSE, FRESNEL RIM, NEON GRID)
// =============================================================================

class SceneShader {
public:
    GLuint program_id{0};
    GLint loc_vp{-1};
    GLint loc_model{-1};
    GLint loc_inv_scale{-1};
    GLint loc_curve{-1};
    GLint loc_color{-1};
    GLint loc_energy_color{-1};
    GLint loc_sky_color{-1};
    GLint loc_light_pos{-1};
    GLint loc_view_pos{-1};
    GLint loc_emission{-1};
    GLint loc_time{-1};
    GLint loc_shading_mode{-1};

    bool build();

    void use() const {
        if (program_id) glUseProgram(program_id);
    }

    void set_vp(const Mat4& vp) const {
        if (loc_vp >= 0) glUniformMatrix4fv(loc_vp, 1, GL_FALSE, vp.data());
    }
    void set_model(const Mat4& mat) const {
        if (loc_model >= 0) glUniformMatrix4fv(loc_model, 1, GL_FALSE, mat.data());
    }
    void set_inv_scale(float ix, float iy, float iz) const {
        if (loc_inv_scale >= 0) glUniform3f(loc_inv_scale, ix, iy, iz);
    }
    void set_curve(float cx, float cy) const {
        if (loc_curve >= 0) glUniform2f(loc_curve, cx, cy);
    }
    void set_color(float r, float g, float b, float a = 1.0f) const {
        if (loc_color >= 0) glUniform4f(loc_color, r, g, b, a);
    }
    void set_energy_color(float r, float g, float b) const {
        if (loc_energy_color >= 0) glUniform3f(loc_energy_color, r, g, b);
    }
    void set_sky_color(float r, float g, float b) const {
        if (loc_sky_color >= 0) glUniform3f(loc_sky_color, r, g, b);
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
    void set_time(float t) const {
        if (loc_time >= 0) glUniform1f(loc_time, t);
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
};

// =============================================================================
// 2. PROCEDURAL SYNTHWAVE HORIZON SKY SHADER (1 DRAW CALL)
// =============================================================================

class SkyShader {
public:
    GLuint program_id{0};
    GLint loc_sky_fog{-1};
    GLint loc_sun_top{-1};
    GLint loc_sun_bottom{-1};
    GLint loc_grid_color{-1};
    GLint loc_time{-1};
    GLint loc_parallax_x{-1};
    GLint loc_fever{-1};
    GLint loc_quality{-1};

    bool build();

    void use() const {
        if (program_id) glUseProgram(program_id);
    }
    void set_params(const Color3& fog, const Color3& sun_top, const Color3& sun_bot,
                    const Color3& grid, float time_sec, float parallax_x,
                    float fever_amt, int quality) const {
        if (loc_sky_fog >= 0)    glUniform3f(loc_sky_fog, fog.r, fog.g, fog.b);
        if (loc_sun_top >= 0)    glUniform3f(loc_sun_top, sun_top.r, sun_top.g, sun_top.b);
        if (loc_sun_bottom >= 0) glUniform3f(loc_sun_bottom, sun_bot.r, sun_bot.g, sun_bot.b);
        if (loc_grid_color >= 0) glUniform3f(loc_grid_color, grid.r, grid.g, grid.b);
        if (loc_time >= 0)       glUniform1f(loc_time, time_sec);
        if (loc_parallax_x >= 0) glUniform1f(loc_parallax_x, parallax_x);
        if (loc_fever >= 0)      glUniform1f(loc_fever, fever_amt);
        if (loc_quality >= 0)    glUniform1i(loc_quality, quality);
    }
    void destroy() {
        if (program_id) {
            glDeleteProgram(program_id);
            program_id = 0;
        }
    }
};

// =============================================================================
// 3. CURVED-WORLD BATCHED PARTICLE, TRAIL RIBBON & SHADOW SHADER
// =============================================================================

class ParticleShader {
public:
    GLuint program_id{0};
    GLint loc_vp{-1};
    GLint loc_curve{-1};
    GLint loc_mode{-1}; // 0 = Radial Glow Particle, 1 = Trail Ribbon, 2 = Contact Shadow

    bool build();

    void use() const {
        if (program_id) glUseProgram(program_id);
    }
    void set_vp(const Mat4& vp) const {
        if (loc_vp >= 0) glUniformMatrix4fv(loc_vp, 1, GL_FALSE, vp.data());
    }
    void set_curve(float cx, float cy) const {
        if (loc_curve >= 0) glUniform2f(loc_curve, cx, cy);
    }
    void set_mode(int mode) const {
        if (loc_mode >= 0) glUniform1i(loc_mode, mode);
    }
    void destroy() {
        if (program_id) {
            glDeleteProgram(program_id);
            program_id = 0;
        }
    }
};

// =============================================================================
// 4. MODERN SDF GLASSMORPHIC UI & ANTI-ALIASED VECTOR FONT SHADER
// =============================================================================

class UiShader {
public:
    GLuint program_id{0};
    GLint loc_mvp{-1};
    GLint loc_color{-1};
    GLint loc_top_color{-1};
    GLint loc_border_color{-1};
    GLint loc_rect_size{-1};
    GLint loc_radius{-1};
    GLint loc_border_width{-1};
    GLint loc_mode{-1}; // 0 = Anti-Aliased Vector Font, 1 = SDF Glass Panel, 2 = Solid Quad
    GLint loc_texture{-1};

    bool build();

    void use() const {
        if (program_id) glUseProgram(program_id);
    }
    void set_mvp(const Mat4& mvp) const {
        if (loc_mvp >= 0) glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, mvp.data());
    }
    void set_color(float r, float g, float b, float a) const {
        if (loc_color >= 0) glUniform4f(loc_color, r, g, b, a);
    }
    void set_top_color(float r, float g, float b, float a) const {
        if (loc_top_color >= 0) glUniform4f(loc_top_color, r, g, b, a);
    }
    void set_border_color(float r, float g, float b, float a) const {
        if (loc_border_color >= 0) glUniform4f(loc_border_color, r, g, b, a);
    }
    void set_rect_params(float w, float h, float radius, float border_w) const {
        if (loc_rect_size >= 0) glUniform2f(loc_rect_size, w, h);
        if (loc_radius >= 0) glUniform1f(loc_radius, radius);
        if (loc_border_width >= 0) glUniform1f(loc_border_width, border_w);
    }
    void set_mode(int mode) const {
        if (loc_mode >= 0) glUniform1i(loc_mode, mode);
    }
    void set_texture_unit(int unit) const {
        if (loc_texture >= 0) glUniform1i(loc_texture, unit);
    }
    void destroy() {
        if (program_id) {
            glDeleteProgram(program_id);
            program_id = 0;
        }
    }
};

// =============================================================================
// GLSL ES 3.0 SHADER SOURCES
// =============================================================================

inline const char* SCENE_VERT_SRC = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_VP;
uniform mat4 u_Model;
uniform vec3 u_InvScale;
uniform vec2 u_Curve;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    // Camera-relative Curved World Horizon deformation (zero impact on physics!)
    float dz = max(0.0, worldPos.z);
    float dz2 = dz * dz;
    worldPos.x += u_Curve.x * dz2;
    worldPos.y += u_Curve.y * dz2;

    v_FragPos = worldPos.xyz;
    v_Normal = normalize(mat3(u_Model) * (a_Normal * u_InvScale));
    v_TexCoord = a_TexCoord;
    gl_Position = u_VP * worldPos;
}
)";

inline const char* SCENE_FRAG_SRC = R"(#version 300 es
precision mediump float;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec4  u_Color;
uniform vec3  u_EnergyColor;
uniform vec3  u_SkyColor;
uniform vec3  u_LightPos;
uniform vec3  u_ViewPos;
uniform float u_Emission;
uniform float u_Time;
uniform int   u_ShadingMode;
// 0 = Lit Rail/Arch/Prop, 1 = Neon Grid Runway, 2 = Glowing Hazard/Crystal,
// 3 = Player Core Sphere, 4 = Fresnel Holographic Shield/Ring

out vec4 fragColor;

void main() {
    vec3 norm = normalize(v_Normal);
    vec3 lightDir = normalize(u_LightPos - v_FragPos);
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);
    float ndotv = max(dot(norm, viewDir), 0.0);

    if (u_ShadingMode == 4) {
        // --- FRESNEL HOLOGRAPHIC SHIELD / SHOCKWAVE RING ---
        float fresnel = pow(1.0 - ndotv, 1.9);
        float scan = 0.72 + 0.28 * sin(v_TexCoord.y * 40.0 - u_Time * 9.0);
        float alpha = clamp((0.20 + fresnel * 0.76) * scan * u_Color.a, 0.0, 0.96);
        vec3 col = u_Color.rgb * (1.05 + fresnel * u_Emission);
        fragColor = vec4(col, alpha);
        return;
    }

    if (u_ShadingMode == 1) {
        // --- ANTI-ALIASED SYNTHWAVE RUNWAY GRID WITH SPEED PULSE ---
        vec2 uv = v_TexCoord;
        float gx = abs(fract(uv.x * 3.0 - 0.5) - 0.5) / max(fwidth(uv.x * 3.0), 0.001);
        float gz = abs(fract(uv.y * 2.0 - 0.5) - 0.5) / max(fwidth(uv.y * 2.0), 0.001);
        float grid = max(1.0 - min(gx, 1.0), 1.0 - min(gz, 1.0));

        float centerDist = abs(uv.x - 0.5);
        float centerGlow = smoothstep(0.032, 0.0, centerDist);
        float edgeGlow = smoothstep(0.41, 0.50, centerDist);
        float wave = 0.86 + 0.14 * sin(u_Time * 4.5 - v_FragPos.z * 0.22);

        vec3 track = u_Color.rgb
                   + u_EnergyColor * grid * 0.62 * wave
                   + vec3(1.0, 0.30, 0.88) * centerGlow * 0.82
                   + u_EnergyColor * edgeGlow * 1.30;

        float fogFactor = clamp((v_FragPos.z - 10.0) / 96.0, 0.0, 1.0);
        fogFactor = fogFactor * fogFactor;
        fragColor = vec4(mix(track, u_SkyColor, fogFactor), 1.0);
        return;
    }

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), 44.0);
    float fresnel = pow(1.0 - ndotv, 2.3);

    if (u_ShadingMode == 2) {
        // --- ARTICULATED NEON HAZARD & CRYSTAL PICKUP ---
        float edgeX = min(v_TexCoord.x, 1.0 - v_TexCoord.x);
        float edgeY = min(v_TexCoord.y, 1.0 - v_TexCoord.y);
        float edgeRim = smoothstep(0.10, 0.0, min(edgeX, edgeY)) * 0.62;

        vec3 base = u_Color.rgb * (0.34 + diff * 0.78);
        vec3 specCol = vec3(1.0, 0.96, 0.92) * spec * 0.85;
        vec3 glow = u_Color.rgb * (u_Emission + edgeRim + fresnel * 0.55);
        vec3 res = base + specCol + glow;

        float fogFactor = clamp((v_FragPos.z - 10.0) / 98.0, 0.0, 1.0);
        fogFactor = fogFactor * fogFactor;
        fragColor = vec4(mix(res, u_SkyColor, fogFactor), u_Color.a);
        return;
    }

    if (u_ShadingMode == 3) {
        // --- METALLIC APEX GYRO-CORE SPHERE ---
        float latBand = smoothstep(0.18, 0.0, abs(fract(v_TexCoord.y * 6.0) - 0.5) * 2.0);
        float eqBand  = smoothstep(0.065, 0.0, abs(v_TexCoord.y - 0.5));
        float lonBand = smoothstep(0.15, 0.0, abs(fract(v_TexCoord.x * 8.0) - 0.5) * 2.0);

        float pattern = max(eqBand * 1.50, max(latBand * 0.85, lonBand * 0.65));
        vec3 surface = mix(u_Color.rgb, u_EnergyColor, clamp(pattern, 0.0, 1.0));

        vec3 lit = surface * (0.38 + diff * 0.88) + vec3(1.0) * spec * 1.05;
        vec3 rim = u_EnergyColor * (fresnel * 0.85 + u_Emission + pattern * 1.25);
        fragColor = vec4(lit + rim, u_Color.a);
        return;
    }

    // --- MODE 0: LIT ARCHES, RAILS & PROPS ---
    vec3 res = u_Color.rgb * (0.34 + diff * 0.80 + u_Emission)
             + vec3(1.0) * spec * 0.75
             + u_Color.rgb * fresnel * 0.45;
    float fogFactor = clamp((v_FragPos.z - 10.0) / 98.0, 0.0, 1.0);
    fogFactor = fogFactor * fogFactor;
    fragColor = vec4(mix(res, u_SkyColor, fogFactor), u_Color.a);
}
)";

inline const char* SKY_VERT_SRC = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;

out vec2 v_UV;

void main() {
    v_UV = a_TexCoord;
    gl_Position = vec4(a_Position.xy, 0.9999, 1.0);
}
)";

inline const char* SKY_FRAG_SRC = R"(#version 300 es
precision mediump float;

in vec2 v_UV;

uniform vec3  u_SkyFog;
uniform vec3  u_SunTop;
uniform vec3  u_SunBottom;
uniform vec3  u_GridColor;
uniform float u_Time;
uniform float u_ParallaxX;
uniform float u_Fever;
uniform int   u_Quality;

out vec4 fragColor;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec2 uv = vec2(v_UV.x, 1.0 - v_UV.y);
    float horizon = 0.44;

    vec3 zenith = u_SkyFog * 0.30 + vec3(0.01, 0.004, 0.04);
    vec3 midSky = mix(u_SkyFog * 1.40, u_SunBottom * 0.42, 0.48);
    float skyT = clamp((uv.y - horizon) / (1.0 - horizon), 0.0, 1.0);
    vec3 col = mix(midSky, zenith, pow(skyT, 0.62));

    float hGlow = exp(-abs(uv.y - horizon) * 13.0);
    col += mix(u_SunBottom, u_GridColor, 0.35) * hGlow * (0.60 + 0.40 * u_Fever);

    if (u_Quality >= 1 && uv.y > horizon - 0.02) {
        vec2 sunCenter = vec2(0.5 - u_ParallaxX * 0.02, horizon + 0.155);
        vec2 d = uv - sunCenter;
        d.x *= 0.60;
        float r = length(d);
        float sunR = 0.155;

        col += u_SunBottom * exp(-r * 6.0) * 0.46;

        if (r < sunR) {
            float sunGrad = clamp((uv.y - (sunCenter.y - sunR)) / (2.0 * sunR), 0.0, 1.0);
            vec3 sunCol = mix(u_SunBottom, u_SunTop, sunGrad);

            float slice = 1.0;
            if (sunGrad < 0.58) {
                float stripe = fract(sunGrad * 14.0 - u_Time * 0.28);
                float gap = mix(0.42, 0.05, sunGrad / 0.58);
                slice = smoothstep(gap, gap + 0.06, stripe);
            }
            float edgeAA = smoothstep(sunR, sunR - 0.004, r);
            col = mix(col, sunCol * 1.28, edgeAA * slice);
        }
    }

    if (u_Quality >= 1 && uv.y > horizon - 0.02 && uv.y < horizon + 0.22) {
        float x = uv.x * 6.5 + u_ParallaxX * 0.08;
        float ridge = 0.048 * abs(sin(x * 1.3 + 0.4)) + 0.030 * abs(sin(x * 2.9 - 1.1));
        float mH = horizon + ridge;
        if (uv.y < mH) {
            float rimDist = mH - uv.y;
            vec3 mtnCol = u_SkyFog * 0.52;
            float neonRim = smoothstep(0.009, 0.0, rimDist);
            col = mtnCol + u_GridColor * neonRim * 0.92;
        }
    }

    if (u_Quality >= 2 && uv.y > horizon + 0.12) {
        vec2 starCell = floor(uv * vec2(38.0, 64.0));
        float rnd = hash21(starCell);
        if (rnd > 0.966) {
            vec2 local = fract(uv * vec2(38.0, 64.0)) - 0.5;
            float d = length(local);
            float twinkle = 0.55 + 0.45 * sin(u_Time * (2.5 + rnd * 4.0) + rnd * 60.0);
            float star = smoothstep(0.28, 0.02, d) * twinkle * smoothstep(horizon + 0.12, 0.85, uv.y);
            col += vec3(0.85, 0.95, 1.0) * star;
        }
    }

    if (uv.y < horizon) {
        float belowT = clamp((horizon - uv.y) / horizon, 0.0, 1.0);
        col = mix(col, u_SkyFog, smoothstep(0.0, 0.25, belowT));
    }

    fragColor = vec4(col, 1.0);
}
)";

inline const char* PARTICLE_VERT_SRC = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;

uniform mat4 u_VP;
uniform vec2 u_Curve;

out vec2 v_UV;
out vec4 v_Color;

void main() {
    vec4 wp = vec4(a_Position, 1.0);
    float dz = max(0.0, wp.z);
    float dz2 = dz * dz;
    wp.x += u_Curve.x * dz2;
    wp.y += u_Curve.y * dz2;

    v_UV = a_TexCoord;
    v_Color = a_Color;
    gl_Position = u_VP * wp;
}
)";

inline const char* PARTICLE_FRAG_SRC = R"(#version 300 es
precision mediump float;

in vec2 v_UV;
in vec4 v_Color;

uniform int u_Mode; // 0 = Radial Glow, 1 = Soft Trail Ribbon, 2 = Contact Shadow

out vec4 fragColor;

void main() {
    if (u_Mode == 1) {
        float edge = 1.0 - abs(v_UV.x - 0.5) * 2.0;
        float core = smoothstep(0.0, 0.65, edge);
        float alpha = core * v_Color.a;
        vec3 col = v_Color.rgb * (0.95 + 0.55 * core);
        fragColor = vec4(col, alpha);
        return;
    }

    vec2 p = v_UV - vec2(0.5);
    float d = length(p);

    if (u_Mode == 2) {
        float a = smoothstep(0.5, 0.08, d) * v_Color.a;
        fragColor = vec4(v_Color.rgb, a);
        return;
    }

    float glow = smoothstep(0.5, 0.02, d);
    fragColor = vec4(v_Color.rgb, v_Color.a * glow * glow);
}
)";

inline const char* UI_VERT_SRC = R"(#version 300 es
layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_MVP;

out vec2 v_UV;

void main() {
    v_UV = a_TexCoord;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
)";

inline const char* UI_FRAG_SRC = R"(#version 300 es
precision mediump float;

in vec2 v_UV;

uniform vec4  u_Color;
uniform vec4  u_TopColor;
uniform vec4  u_BorderColor;
uniform vec2  u_RectSize;
uniform float u_Radius;
uniform float u_BorderWidth;
uniform int   u_Mode; // 0 = Smooth Vector Font, 1 = SDF Glass Panel, 2 = Solid Quad
uniform sampler2D u_Texture;

out vec4 fragColor;

void main() {
    if (u_Mode == 0) {
        float alpha = texture(u_Texture, v_UV).r;
        // Crisp SDF-like anti-aliased edge enhancement
        float crisp = smoothstep(0.28, 0.62, alpha);
        fragColor = vec4(u_Color.rgb, u_Color.a * crisp);
        return;
    }

    if (u_Mode == 2) {
        fragColor = u_Color;
        return;
    }

    vec2 halfSize = u_RectSize * 0.5;
    vec2 p = (v_UV - vec2(0.5)) * u_RectSize;
    float r = min(u_Radius, min(halfSize.x, halfSize.y));
    vec2 q = abs(p) - (halfSize - vec2(r));
    float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;

    float outerAA = 1.0 - smoothstep(-1.0, 0.8, dist);
    if (outerAA <= 0.001) discard;

    vec4 fill = mix(u_TopColor, u_Color, v_UV.y);
    float sheen = smoothstep(0.30, 0.0, v_UV.y) * 0.10;
    fill.rgb += vec3(sheen);

    float borderMask = 0.0;
    if (u_BorderWidth > 0.01) {
        borderMask = smoothstep(-u_BorderWidth - 0.8, -u_BorderWidth + 0.6, dist);
    }

    vec4 finalCol = mix(fill, u_BorderColor, borderMask * u_BorderColor.a);
    finalCol.a *= outerAA;
    fragColor = finalCol;
}
)";

inline bool SceneShader::build() {
    program_id = link_gl_program(SCENE_VERT_SRC, SCENE_FRAG_SRC);
    if (!program_id) return false;
    loc_vp           = glGetUniformLocation(program_id, "u_VP");
    loc_model        = glGetUniformLocation(program_id, "u_Model");
    loc_inv_scale    = glGetUniformLocation(program_id, "u_InvScale");
    loc_curve        = glGetUniformLocation(program_id, "u_Curve");
    loc_color        = glGetUniformLocation(program_id, "u_Color");
    loc_energy_color = glGetUniformLocation(program_id, "u_EnergyColor");
    loc_sky_color    = glGetUniformLocation(program_id, "u_SkyColor");
    loc_light_pos    = glGetUniformLocation(program_id, "u_LightPos");
    loc_view_pos     = glGetUniformLocation(program_id, "u_ViewPos");
    loc_emission     = glGetUniformLocation(program_id, "u_Emission");
    loc_time         = glGetUniformLocation(program_id, "u_Time");
    loc_shading_mode = glGetUniformLocation(program_id, "u_ShadingMode");
    return true;
}

inline bool SkyShader::build() {
    program_id = link_gl_program(SKY_VERT_SRC, SKY_FRAG_SRC);
    if (!program_id) return false;
    loc_sky_fog    = glGetUniformLocation(program_id, "u_SkyFog");
    loc_sun_top    = glGetUniformLocation(program_id, "u_SunTop");
    loc_sun_bottom = glGetUniformLocation(program_id, "u_SunBottom");
    loc_grid_color = glGetUniformLocation(program_id, "u_GridColor");
    loc_time       = glGetUniformLocation(program_id, "u_Time");
    loc_parallax_x = glGetUniformLocation(program_id, "u_ParallaxX");
    loc_fever      = glGetUniformLocation(program_id, "u_Fever");
    loc_quality    = glGetUniformLocation(program_id, "u_Quality");
    return true;
}

inline bool ParticleShader::build() {
    program_id = link_gl_program(PARTICLE_VERT_SRC, PARTICLE_FRAG_SRC);
    if (!program_id) return false;
    loc_vp    = glGetUniformLocation(program_id, "u_VP");
    loc_curve = glGetUniformLocation(program_id, "u_Curve");
    loc_mode  = glGetUniformLocation(program_id, "u_Mode");
    return true;
}

inline bool UiShader::build() {
    program_id = link_gl_program(UI_VERT_SRC, UI_FRAG_SRC);
    if (!program_id) return false;
    loc_mvp          = glGetUniformLocation(program_id, "u_MVP");
    loc_color        = glGetUniformLocation(program_id, "u_Color");
    loc_top_color    = glGetUniformLocation(program_id, "u_TopColor");
    loc_border_color = glGetUniformLocation(program_id, "u_BorderColor");
    loc_rect_size    = glGetUniformLocation(program_id, "u_RectSize");
    loc_radius       = glGetUniformLocation(program_id, "u_Radius");
    loc_border_width = glGetUniformLocation(program_id, "u_BorderWidth");
    loc_mode         = glGetUniformLocation(program_id, "u_Mode");
    loc_texture      = glGetUniformLocation(program_id, "u_Texture");
    return true;
}

} // namespace velo
