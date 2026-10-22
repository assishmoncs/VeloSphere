/**
 * VeloSphere - Native 3D Endless Runner (Production Release 2.0)
 * Complete Game Engine Implementation (OpenGL ES 3.0, GameActivity, C++17)
 */

#include <jni.h>
#include <game-activity/GameActivity.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

extern "C" {
    void GameActivity_register(JNIEnv* env);
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    GameActivity_register(env);
    return JNI_VERSION_1_6;
}

#include <android/log.h>
#include <android/native_window.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <algorithm>

#include "Math3D.h"
#include "Shader.h"
#include "Mesh.h"
#include "GameLogic.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#ifdef LOGI
#undef LOGI
#endif
#ifdef LOGW
#undef LOGW
#endif
#ifdef LOGE
#undef LOGE
#endif

#define LOG_TAG "VeloSphere"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

namespace velo {

// =============================================================================
// EGL CONTEXT MANAGER
// =============================================================================

struct EglContextManager {
    EGLDisplay display{EGL_NO_DISPLAY};
    EGLSurface surface{EGL_NO_SURFACE};
    EGLContext context{EGL_NO_CONTEXT};
    int32_t    width{0};
    int32_t    height{0};
    bool       is_ready{false};

    bool initialize(ANativeWindow* window) {
        if (!window) {
            LOGE("EGL Init Failed: window is null");
            return false;
        }

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) {
            LOGE("eglGetDisplay failed");
            return false;
        }

        if (!eglInitialize(display, nullptr, nullptr)) {
            LOGE("eglInitialize failed");
            return false;
        }

        const EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
            EGL_BLUE_SIZE,       8,
            EGL_GREEN_SIZE,      8,
            EGL_RED_SIZE,        8,
            EGL_ALPHA_SIZE,      8,
            EGL_DEPTH_SIZE,      24,
            EGL_NONE
        };

        EGLConfig config;
        EGLint num_configs = 0;
        if (!eglChooseConfig(display, attribs, &config, 1, &num_configs) || num_configs <= 0) {
            LOGE("eglChooseConfig failed for OpenGL ES 3.0");
            return false;
        }

        EGLint format = 0;
        eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
        ANativeWindow_setBuffersGeometry(window, 0, 0, format);

        surface = eglCreateWindowSurface(display, config, window, nullptr);
        if (surface == EGL_NO_SURFACE) {
            LOGE("eglCreateWindowSurface failed");
            return false;
        }

        const EGLint context_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };

        context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
        if (context == EGL_NO_CONTEXT) {
            LOGE("eglCreateContext failed for GLES 3.0");
            return false;
        }

        if (!eglMakeCurrent(display, surface, surface, context)) {
            LOGE("eglMakeCurrent failed");
            return false;
        }

        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);

        LOGI("OpenGL ES 3.0 Initialized: %dx%d", width, height);
        LOGI("Renderer: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        LOGI("Version:  %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_CULL_FACE);

        is_ready = true;
        return true;
    }

    void update_dimensions() {
        if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
            eglQuerySurface(display, surface, EGL_WIDTH, &width);
            eglQuerySurface(display, surface, EGL_HEIGHT, &height);
            glViewport(0, 0, width, height);
            LOGI("EGL Surface dimensions updated: %dx%d", width, height);
        }
    }

    void swap_buffers() const {
        if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
            eglSwapBuffers(display, surface);
        }
    }

    void terminate() {
        if (display != EGL_NO_DISPLAY) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT) {
                eglDestroyContext(display, context);
                context = EGL_NO_CONTEXT;
            }
            if (surface != EGL_NO_SURFACE) {
                eglDestroySurface(display, surface);
                surface = EGL_NO_SURFACE;
            }
            eglTerminate(display);
            display = EGL_NO_DISPLAY;
        }
        is_ready = false;
        LOGI("EGL Context Terminated.");
    }
};

// =============================================================================
// FRAME TIMER
// =============================================================================

class FrameTimer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    FrameTimer() : last_time_(Clock::now()) {}

    void reset() {
        last_time_ = Clock::now();
    }

    float tick() {
        const auto now = Clock::now();
        const std::chrono::duration<float> elapsed = now - last_time_;
        last_time_ = now;

        float dt = elapsed.count();
        if (dt > 0.1f) dt = 0.1f;
        return dt;
    }

private:
    TimePoint last_time_;
};

// =============================================================================
// UI LAYOUT & HIT-TEST HELPERS
// =============================================================================

static float hud_base_scale(int view_width) {
    float base_scale = static_cast<float>(view_width) / 450.0f;
    if (base_scale < 1.0f) base_scale = 1.0f;
    if (base_scale > 2.5f) base_scale = 2.5f;
    return base_scale;
}

struct Rect2D {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};

    bool contains(float px, float py) const {
        return px >= x && px <= (x + w) && py >= y && py <= (y + h);
    }
};

static Rect2D get_pause_button_rect(int view_width) {
    const float scale = hud_base_scale(view_width);
    const float w = 52.0f * scale;
    const float h = 42.0f * scale;
    return {(static_cast<float>(view_width) - w) * 0.5f, 6.0f * scale, w, h};
}

static Rect2D get_ready_mode_button_rect(int view_width, int view_height) {
    const float scale = hud_base_scale(view_width);
    const float box_w = 360.0f * scale;
    const float box_h = 260.0f * scale;
    const float bx = (static_cast<float>(view_width) - box_w) * 0.5f;
    const float by = (static_cast<float>(view_height) - box_h) * 0.42f;
    return {bx + 20.0f * scale, by + 92.0f * scale, box_w - 40.0f * scale, 34.0f * scale};
}

static Rect2D get_ready_skin_button_rect(int view_width, int view_height) {
    const float scale = hud_base_scale(view_width);
    const float box_w = 360.0f * scale;
    const float box_h = 260.0f * scale;
    const float bx = (static_cast<float>(view_width) - box_w) * 0.5f;
    const float by = (static_cast<float>(view_height) - box_h) * 0.42f;
    return {bx + 20.0f * scale, by + 134.0f * scale, box_w - 40.0f * scale, 34.0f * scale};
}

static Rect2D get_ready_audio_button_rect(int view_width, int view_height) {
    const float scale = hud_base_scale(view_width);
    const float box_w = 360.0f * scale;
    const float box_h = 260.0f * scale;
    const float bx = (static_cast<float>(view_width) - box_w) * 0.5f;
    const float by = (static_cast<float>(view_height) - box_h) * 0.42f;
    return {bx + 20.0f * scale, by + 176.0f * scale, box_w - 40.0f * scale, 32.0f * scale};
}

static Rect2D get_pause_restart_button_rect(int view_width, int view_height) {
    const float scale = hud_base_scale(view_width);
    const float box_w = 340.0f * scale;
    const float box_h = 210.0f * scale;
    const float bx = (static_cast<float>(view_width) - box_w) * 0.5f;
    const float by = (static_cast<float>(view_height) - box_h) * 0.40f;
    return {bx + 30.0f * scale, by + 112.0f * scale, box_w - 60.0f * scale, 34.0f * scale};
}

static Rect2D get_pause_menu_button_rect(int view_width, int view_height) {
    const float scale = hud_base_scale(view_width);
    const float box_w = 340.0f * scale;
    const float box_h = 210.0f * scale;
    const float bx = (static_cast<float>(view_width) - box_w) * 0.5f;
    const float by = (static_cast<float>(view_height) - box_h) * 0.40f;
    return {bx + 30.0f * scale, by + 156.0f * scale, box_w - 60.0f * scale, 34.0f * scale};
}

static Rect2D get_gameover_menu_button_rect(int view_width, int view_height) {
    const float scale = hud_base_scale(view_width);
    const float box_w = 350.0f * scale;
    const float box_h = 250.0f * scale;
    const float bx = (static_cast<float>(view_width) - box_w) * 0.5f;
    const float by = (static_cast<float>(view_height) - box_h) * 0.40f;
    return {bx + 35.0f * scale, by + 202.0f * scale, box_w - 70.0f * scale, 34.0f * scale};
}

// =============================================================================
// GRAPHICS RENDERER (SHADERS, GEOMETRY & CAMERA PIPELINE)
// =============================================================================

class GameRenderer {
public:
    ShaderProgram  shader;
    SphereMesh     sphere_mesh;
    SphereMesh     orb_mesh;
    BoxMesh        box_mesh;
    BoxMesh        rail_mesh;
    TrackMesh      track_mesh;
    HudRenderer    hud;
    ParticleSystem particles;

    bool init() {
        if (!shader.build(GLES_VERTEX_SHADER, GLES_FRAGMENT_SHADER)) {
            LOGE("Failed to build main shader program");
            return false;
        }

        sphere_mesh.init(PlayerBall::RADIUS, 28, 18);
        orb_mesh.init(0.32f, 16, 12);
        box_mesh.init(1.0f, 1.0f, 1.0f);
        rail_mesh.init(0.25f, 0.40f, TreadmillSystem::SEGMENT_SPACING);
        track_mesh.init(PlayerBall::TRACK_HALF_WIDTH * 2.0f, TreadmillSystem::SEGMENT_SPACING);
        hud.init();
        particles.init();

        LOGI("GameRenderer: Shaders, HUD font, particles, and procedural meshes loaded.");
        return true;
    }

    void render_frame(const PlayerBall& player,
                      const TreadmillSystem& treadmill,
                      const SaveData& save,
                      GameState state,
                      int view_width,
                      int view_height,
                      float death_shake_timer,
                      float countdown_timer,
                      int countdown_value) {
        glViewport(0, 0, view_width, view_height);

        const auto& biome = BIOME_PALETTES[treadmill.get_biome_index()];

        if (state == GameState::GAME_OVER) {
            glClearColor(0.12f, 0.02f, 0.04f, 1.0f);
            shader.use();
            shader.set_sky_color(0.12f, 0.02f, 0.04f);
        } else {
            glClearColor(biome.sky_fog.r, biome.sky_fog.g, biome.sky_fog.b, 1.0f);
            shader.use();
            shader.set_sky_color(biome.sky_fog.r, biome.sky_fog.g, biome.sky_fog.b);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (view_height > 0) ? (static_cast<float>(view_width) / static_cast<float>(view_height)) : 1.0f;
        float speed = treadmill.get_global_speed();
        float speed_frac = (speed - TreadmillSystem::BASE_SPEED) / (TreadmillSystem::MAX_SPEED - TreadmillSystem::BASE_SPEED);
        float extra_fov = (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) ? 6.0f : 0.0f;
        float fov = 60.0f + std::clamp(speed_frac, 0.0f, 1.2f) * 10.0f + extra_fov;
        Mat4 projection = Mat4::perspective(to_radians(fov), aspect, 0.1f, 150.0f);

        float shake_offset_x = 0.0f;
        float shake_offset_y = 0.0f;
        if (death_shake_timer > 0.0f) {
            shake_offset_x = std::sin(death_shake_timer * 50.0f) * 0.25f * death_shake_timer;
            shake_offset_y = std::cos(death_shake_timer * 40.0f) * 0.20f * death_shake_timer;
        }

        Vec3 eye = {
            player.pos.x * 0.30f + shake_offset_x,
            3.8f + player.jump_y * 0.25f + shake_offset_y,
            -5.5f
        };
        Vec3 target = {
            player.pos.x * 0.18f,
            1.2f + player.jump_y * 0.15f,
            18.0f
        };
        Vec3 up = {0.0f, 1.0f, 0.0f};
        Mat4 view = Mat4::look_at(eye, target, up);
        Mat4 vp = projection * view;

        Vec3 light_pos = {2.0f, 10.0f, -2.0f};
        shader.set_light_pos(light_pos);
        shader.set_view_pos(eye);

        // ---------------------------------------------------------------------
        // 1. RENDER TRACK RUNWAY, RAILS, OBSTACLES, ORBS & POWER-UPS
        // ---------------------------------------------------------------------
        const auto& segments = treadmill.get_segments();
        const float spin = treadmill.get_orb_spin_angle();

        for (const auto& seg : segments) {
            Mat4 model_floor = Mat4::translate(0.0f, 0.0f, seg.z_position);
            shader.set_model(model_floor);
            shader.set_mvp(vp * model_floor);
            shader.set_shading_mode(1);
            shader.set_color(biome.floor_base.r, biome.floor_base.g, biome.floor_base.b, 1.0f);
            shader.set_energy_color(biome.grid_color.r, biome.grid_color.g, biome.grid_color.b);
            shader.set_emission(0.5f);
            track_mesh.draw();

            shader.set_shading_mode(0);
            shader.set_color(biome.rail_color.r, biome.rail_color.g, biome.rail_color.b, 1.0f);
            shader.set_emission(0.85f);

            Mat4 model_left_rail = Mat4::translate(-PlayerBall::TRACK_HALF_WIDTH, 0.20f, seg.z_position);
            shader.set_model(model_left_rail);
            shader.set_mvp(vp * model_left_rail);
            rail_mesh.draw();

            Mat4 model_right_rail = Mat4::translate(PlayerBall::TRACK_HALF_WIDTH, 0.20f, seg.z_position);
            shader.set_model(model_right_rail);
            shader.set_mvp(vp * model_right_rail);
            rail_mesh.draw();

            const float lane_divider_x = TreadmillSystem::LANE_WIDTH * 0.5f;
            const float lane_divider_y = 0.018f;
            shader.set_color(biome.grid_color.r * 0.45f, biome.grid_color.g * 0.45f, biome.grid_color.b * 0.55f, 1.0f);
            shader.set_emission(0.32f);

            Mat4 model_left_divider =
                Mat4::translate(-lane_divider_x, lane_divider_y, seg.z_position) *
                Mat4::scale(0.045f, 0.025f, TreadmillSystem::SEGMENT_SPACING * 0.90f);
            shader.set_model(model_left_divider);
            shader.set_mvp(vp * model_left_divider);
            box_mesh.draw();

            Mat4 model_right_divider =
                Mat4::translate(lane_divider_x, lane_divider_y, seg.z_position) *
                Mat4::scale(0.045f, 0.025f, TreadmillSystem::SEGMENT_SPACING * 0.90f);
            shader.set_model(model_right_divider);
            shader.set_mvp(vp * model_right_divider);
            box_mesh.draw();

            // Render Collectible Data Orb
            if (seg.has_orb && !seg.orb_collected) {
                float bob = std::sin(spin * 2.0f + seg.z_position * 0.2f) * 0.12f;
                Mat4 model_orb =
                    Mat4::translate(seg.orb_pos.x, seg.orb_pos.y + bob, seg.orb_pos.z) *
                    Mat4::rotate_y(spin);
                shader.set_model(model_orb);
                shader.set_mvp(vp * model_orb);
                shader.set_shading_mode(2);
                shader.set_color(1.0f, 0.88f, 0.12f, 1.0f);
                shader.set_emission(1.15f);
                orb_mesh.draw();
            }

            // Render Collectible Power-Up Cube
            if (seg.powerup_type != PowerUpType::NONE && !seg.powerup_collected) {
                float bob = std::cos(spin * 2.2f + seg.z_position * 0.3f) * 0.15f;
                Mat4 model_pu =
                    Mat4::translate(seg.powerup_pos.x, seg.powerup_pos.y + bob, seg.powerup_pos.z) *
                    Mat4::rotate_y(spin * 1.5f) *
                    Mat4::rotate_x(spin * 0.8f) *
                    Mat4::scale(0.62f, 0.62f, 0.62f);
                shader.set_model(model_pu);
                shader.set_mvp(vp * model_pu);
                shader.set_shading_mode(2);

                switch (seg.powerup_type) {
                    case PowerUpType::SHIELD:
                        shader.set_color(0.05f, 0.95f, 1.0f, 1.0f);
                        break;
                    case PowerUpType::OVERDRIVE:
                        shader.set_color(1.0f, 0.20f, 0.85f, 1.0f);
                        break;
                    case PowerUpType::SLOW_MO:
                        shader.set_color(0.55f, 0.30f, 1.0f, 1.0f);
                        break;
                    case PowerUpType::MAGNET:
                    default:
                        shader.set_color(0.15f, 1.0f, 0.45f, 1.0f);
                        break;
                }
                shader.set_emission(1.25f);
                box_mesh.draw();
            }

            // Render Obstacle by Type
            if (seg.has_obstacle) {
                Mat4 model_box =
                    Mat4::translate(seg.obstacle_x_offset, seg.hover_y + seg.obstacle_height * 0.5f, seg.z_position) *
                    Mat4::scale(seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth);

                shader.set_model(model_box);
                shader.set_mvp(vp * model_box);
                shader.set_shading_mode(2);

                if (seg.is_moving) {
                    if (seg.motion_warning_time > 0.0f) {
                        const float warning_progress =
                            1.0f - (seg.motion_warning_time / TreadmillSystem::MOVING_WARNING_TIME);
                        const float pulse =
                            0.75f + 0.25f * std::sin(warning_progress * 18.0f);
                        shader.set_color(1.0f, 0.80f, 0.08f, 1.0f);
                        shader.set_emission(1.0f + 0.70f * pulse);
                    } else {
                        shader.set_color(1.0f, 0.65f, 0.0f, 1.0f);
                        shader.set_emission(0.90f);
                    }
                    box_mesh.draw();
                } else if (seg.obstacle_type == ObstacleType::LOW_BARRIER) {
                    // Jumpable low laser hurdle in vivid violet-magenta
                    shader.set_color(0.85f, 0.15f, 1.0f, 1.0f);
                    shader.set_emission(0.95f);
                    box_mesh.draw();
                } else if (seg.obstacle_type == ObstacleType::OVERHEAD_GATE) {
                    // Elevated gate with two thin side pillars so player knows to roll under
                    shader.set_color(1.0f, 0.28f, 0.15f, 1.0f);
                    shader.set_emission(0.85f);
                    box_mesh.draw();

                    float pillar_h = seg.hover_y;
                    float pillar_x_off = seg.obstacle_width * 0.44f;
                    Mat4 left_pillar =
                        Mat4::translate(seg.obstacle_x_offset - pillar_x_off, pillar_h * 0.5f, seg.z_position) *
                        Mat4::scale(0.16f, pillar_h, seg.obstacle_depth * 0.5f);
                    shader.set_model(left_pillar);
                    shader.set_mvp(vp * left_pillar);
                    box_mesh.draw();

                    Mat4 right_pillar =
                        Mat4::translate(seg.obstacle_x_offset + pillar_x_off, pillar_h * 0.5f, seg.z_position) *
                        Mat4::scale(0.16f, pillar_h, seg.obstacle_depth * 0.5f);
                    shader.set_model(right_pillar);
                    shader.set_mvp(vp * right_pillar);
                    box_mesh.draw();
                } else {
                    shader.set_color(1.0f, 0.12f, 0.38f, 1.0f);
                    shader.set_emission(0.75f);
                    box_mesh.draw();
                }
            }
        }

        // ---------------------------------------------------------------------
        // 2. RENDER PLAYER SPHERE & ACTIVE SHIELD / OVERDRIVE SHELL
        // ---------------------------------------------------------------------
        Mat4 model_ball;
        if (player.is_tumbling) {
            model_ball =
                Mat4::translate(player.tumble_pos) *
                Mat4::rotate_z(player.tumble_rot.z) *
                Mat4::rotate_x(player.tumble_rot.x) *
                Mat4::rotate_y(player.tumble_rot.y);
        } else {
            model_ball =
                Mat4::translate(player.pos.x, player.pos.y, player.pos.z) *
                Mat4::rotate_z(player.tilt_angle + player.lateral_roll_angle) *
                Mat4::rotate_x(player.roll_angle);
        }

        int skin_idx = std::clamp(save.selected_skin, 0, static_cast<int>(SKIN_PALETTES.size()) - 1);
        const auto& skin = SKIN_PALETTES[static_cast<size_t>(skin_idx)];

        shader.set_model(model_ball);
        shader.set_mvp(vp * model_ball);
        shader.set_shading_mode(3);
        shader.set_energy_color(skin.energy_color.r, skin.energy_color.g, skin.energy_color.b);

        if (state == GameState::GAME_OVER) {
            shader.set_color(1.0f, 0.25f, 0.25f, 1.0f);
            shader.set_emission(0.95f);
        } else if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
            shader.set_color(1.0f, 0.92f, 0.35f, 1.0f);
            shader.set_emission(0.85f);
        } else {
            shader.set_color(skin.core_color.r, skin.core_color.g, skin.core_color.b, 1.0f);
            shader.set_emission(0.35f);
        }
        sphere_mesh.draw();

        // Render Holographic Shield Shell if Shield or Invulnerability or Overdrive is active
        if (!player.is_tumbling &&
            (treadmill.is_shield_active() ||
             player.invulnerable_timer > 0.0f ||
             treadmill.get_active_powerup() == PowerUpType::OVERDRIVE)) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDepthMask(GL_FALSE);

            Mat4 model_shield =
                Mat4::translate(player.pos.x, player.pos.y, player.pos.z) *
                Mat4::rotate_y(spin * 1.4f) *
                Mat4::scale(1.28f, 1.28f, 1.28f);
            shader.set_model(model_shield);
            shader.set_mvp(vp * model_shield);
            shader.set_shading_mode(7);

            if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
                shader.set_color(1.0f, 0.25f, 0.85f, 0.85f);
            } else {
                shader.set_color(0.05f, 0.95f, 1.0f, 0.80f);
            }
            shader.set_emission(1.4f);
            sphere_mesh.draw();

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // ---------------------------------------------------------------------
        // 3. RENDER SPARK PARTICLES
        // ---------------------------------------------------------------------
        particles.draw(shader, vp);

        if (state == GameState::GAME_OVER && death_shake_timer > 0.0f) {
            const float flash_alpha =
                0.45f * std::clamp(death_shake_timer / 0.65f, 0.0f, 1.0f);

            Mat4 flash_ortho =
                Mat4::ortho(0.0f, static_cast<float>(view_width),
                            static_cast<float>(view_height), 0.0f);
            hud.draw_box(shader, flash_ortho,
                         0.0f, 0.0f,
                         static_cast<float>(view_width),
                         static_cast<float>(view_height),
                         1.0f, 0.08f, 0.12f, flash_alpha);
        }

        // ---------------------------------------------------------------------
        // 4. RENDER ON-SCREEN ARCADE HUD & INTERACTIVE MENUS
        // ---------------------------------------------------------------------
        const float vw = static_cast<float>(view_width);
        const float vh = static_cast<float>(view_height);
        Mat4 ortho = Mat4::ortho(0.0f, vw, vh, 0.0f);

        float base_scale = hud_base_scale(view_width);
        float char_w = 10.5f * base_scale;
        float char_h = 17.0f * base_scale;
        float pad = 14.0f * base_scale;

        // Top HUD status bar backdrop
        hud.draw_box(shader, ortho, 0.0f, 0.0f, vw, 58.0f * base_scale, 0.02f, 0.03f, 0.08f, 0.82f);
        hud.draw_box(shader, ortho, 0.0f, 56.0f * base_scale, vw, 2.0f * base_scale, 0.0f, 0.85f, 1.0f, 0.9f);

        // Top-Center Pause Button
        if (state == GameState::PLAYING ||
            state == GameState::COUNTDOWN ||
            state == GameState::PAUSED) {
            Rect2D pb = get_pause_button_rect(view_width);
            hud.draw_box(shader, ortho, pb.x, pb.y, pb.w, pb.h, 0.04f, 0.10f, 0.18f, 0.95f);
            hud.draw_box(shader, ortho, pb.x, pb.y, pb.w, 2.0f * base_scale, 0.0f, 0.95f, 1.0f, 1.0f);

            const char* pause_icon = (state == GameState::PAUSED) ? ">" : "II";
            hud.draw_text_centered(shader, ortho,
                                   pb.x + pb.w * 0.5f,
                                   pb.y + (pb.h - char_h * 1.1f) * 0.5f,
                                   char_w * 1.1f, char_h * 1.1f,
                                   pause_icon, 0.0f, 0.95f, 1.0f, 1.0f);
        }

        // Top-Left: Score & Combo
        char score_str[32];
        std::snprintf(score_str, sizeof(score_str), "SCORE %06d", treadmill.get_score());
        hud.draw_text(shader, ortho, pad, 10.0f * base_scale, char_w, char_h, score_str, 0.0f, 0.95f, 1.0f, 1.0f);

        char sub_left[32];
        if (treadmill.get_game_mode() == GameMode::TIME_ATTACK) {
            std::snprintf(sub_left, sizeof(sub_left), "TIME %04.1FS  X%d", treadmill.get_time_remaining(), treadmill.get_combo());
        } else {
            std::snprintf(sub_left, sizeof(sub_left), "COMBO X%d  ORBS %d", treadmill.get_combo(), save.total_orbs + treadmill.get_run_orbs());
        }
        hud.draw_text(shader, ortho, pad, 33.0f * base_scale, char_w * 0.78f, char_h * 0.78f, sub_left, 0.35f, 1.0f, 0.65f, 0.95f);

        // Top-Right: Best Score & Biome Name
        int mode_best = std::max(save.get_best_for_mode(treadmill.get_game_mode()), treadmill.get_score());
        char best_str[32];
        std::snprintf(best_str, sizeof(best_str), "BEST %06d", mode_best);
        float best_w = HudRenderer::measure_text_width(best_str, char_w);
        hud.draw_text(shader, ortho, vw - best_w - pad, 10.0f * base_scale, char_w, char_h, best_str, 1.0f, 0.85f, 0.15f, 1.0f);

        float biome_w = HudRenderer::measure_text_width(biome.name, char_w * 0.75f);
        hud.draw_text(shader, ortho, vw - biome_w - pad, 33.0f * base_scale, char_w * 0.75f, char_h * 0.75f,
                      biome.name, biome.grid_color.r, biome.grid_color.g, biome.grid_color.b, 0.9f);

        // Bottom-Left: Speedometer
        int speed_mph = static_cast<int>(treadmill.get_global_speed() * 1.6f);
        char spd_str[32];
        std::snprintf(spd_str, sizeof(spd_str), "%d MPH", speed_mph);
        hud.draw_text(shader, ortho, pad, vh - 36.0f * base_scale, char_w * 0.9f, char_h * 0.9f, spd_str, 0.2f, 1.0f, 0.4f, 0.9f);

        // Bottom-Right: Shield & Active Power-Up Bar
        if (treadmill.is_shield_active()) {
            const char* sh_txt = "[SHIELD READY]";
            float sh_w = HudRenderer::measure_text_width(sh_txt, char_w * 0.8f);
            hud.draw_text(shader, ortho, vw - sh_w - pad, vh - 36.0f * base_scale,
                          char_w * 0.8f, char_h * 0.8f, sh_txt, 0.05f, 0.95f, 1.0f, 1.0f);
        }

        if (treadmill.get_active_powerup() != PowerUpType::NONE) {
            const char* pu_name =
                (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) ? "OVERDRIVE" :
                (treadmill.get_active_powerup() == PowerUpType::SLOW_MO)   ? "SLOW-MO" : "MAGNET";
            float bar_w = 130.0f * base_scale;
            float bar_h = 8.0f * base_scale;
            float bar_x = (vw - bar_w) * 0.5f;
            float bar_y = 66.0f * base_scale;

            hud.draw_box(shader, ortho, bar_x, bar_y, bar_w, bar_h, 0.08f, 0.12f, 0.22f, 0.85f);
            hud.draw_box(shader, ortho, bar_x, bar_y, bar_w * treadmill.get_powerup_progress(), bar_h, 1.0f, 0.30f, 0.90f, 0.95f);
            hud.draw_text_centered(shader, ortho, vw * 0.5f, bar_y + 12.0f * base_scale,
                                   char_w * 0.78f, char_h * 0.78f, pu_name, 1.0f, 0.85f, 0.2f, 1.0f);
        }

        // Floating Alert Banner (Near Miss, Shield Smash, Power-Up, or Dodge Milestone)
        if (treadmill.get_banner_timer() > 0.0f) {
            float alpha = std::min(1.0f, treadmill.get_banner_timer() * 1.5f);
            hud.draw_text_centered(shader, ortho, vw * 0.5f, 92.0f * base_scale,
                                   char_w * 1.05f, char_h * 1.05f,
                                   treadmill.get_banner_text(), 1.0f, 0.90f, 0.15f, alpha);
        } else if (treadmill.get_dodge_milestone_timer() > 0.0f) {
            char dodge_msg[32];
            std::snprintf(dodge_msg, sizeof(dodge_msg), "%d+ DODGES!", treadmill.get_dodge_milestone());
            float milestone_alpha = std::min(1.0f, treadmill.get_dodge_milestone_timer());
            hud.draw_text_centered(shader, ortho, vw * 0.5f, 92.0f * base_scale,
                                   char_w * 1.05f, char_h * 1.05f,
                                   dodge_msg, 0.1f, 1.0f, 0.6f, milestone_alpha);
        }

        // State Overlays
        if (state == GameState::COUNTDOWN) {
            const float pulse = 0.92f + 0.08f *
                std::sin((1.0f - std::max(0.0f, countdown_timer)) * 12.0f);

            char countdown_text[8];
            if (countdown_value == 0) {
                std::snprintf(countdown_text, sizeof(countdown_text), "GO!");
            } else {
                std::snprintf(countdown_text, sizeof(countdown_text), "%d", countdown_value);
            }

            hud.draw_box(shader, ortho, 0.0f, 0.0f, vw, vh, 0.005f, 0.01f, 0.025f, 0.28f);
            hud.draw_text_centered(shader, ortho, vw * 0.5f, vh * 0.38f,
                                   char_w * 2.8f, char_h * 2.8f,
                                   countdown_text, 0.0f, 0.95f, 1.0f, pulse);
        } else if (state == GameState::READY) {
            float box_w = 360.0f * base_scale;
            float box_h = 260.0f * base_scale;
            float bx = (vw - box_w) * 0.5f;
            float by = (vh - box_h) * 0.42f;

            hud.draw_box(shader, ortho, bx, by, box_w, box_h, 0.01f, 0.03f, 0.09f, 0.90f);
            hud.draw_box(shader, ortho, bx, by, box_w, 2.0f * base_scale, 0.0f, 0.95f, 1.0f, 1.0f);
            hud.draw_box(shader, ortho, bx, by + box_h - 2.0f * base_scale, box_w, 2.0f * base_scale, 0.0f, 0.95f, 1.0f, 1.0f);

            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 16.0f * base_scale,
                                   char_w * 1.35f, char_h * 1.35f, "VELOSPHERE 2.0", 0.0f, 0.95f, 1.0f, 1.0f);

            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 54.0f * base_scale,
                                   char_w * 0.78f, char_h * 0.78f, "SWIPE L/R STEER - SWIPE UP JUMP", 0.8f, 0.9f, 1.0f, 0.88f);

            // Mode selector button
            Rect2D mode_btn = get_ready_mode_button_rect(view_width, view_height);
            hud.draw_box(shader, ortho, mode_btn.x, mode_btn.y, mode_btn.w, mode_btn.h, 0.05f, 0.12f, 0.24f, 0.95f);
            const char* mode_label =
                (treadmill.get_game_mode() == GameMode::TIME_ATTACK) ? "MODE: TIME ATTACK (90S)" :
                (treadmill.get_game_mode() == GameMode::HARDCORE)    ? "MODE: HARDCORE (2X PTS)" :
                                                                       "MODE: ENDLESS ARCADE";
            hud.draw_text_centered(shader, ortho, vw * 0.5f, mode_btn.y + 9.0f * base_scale,
                                   char_w * 0.82f, char_h * 0.82f, mode_label, 0.2f, 0.95f, 1.0f, 1.0f);

            // Skin selector button
            Rect2D skin_btn = get_ready_skin_button_rect(view_width, view_height);
            hud.draw_box(shader, ortho, skin_btn.x, skin_btn.y, skin_btn.w, skin_btn.h, 0.05f, 0.12f, 0.24f, 0.95f);
            char skin_label[48];
            std::snprintf(skin_label, sizeof(skin_label), "SKIN: %s", skin.name);
            hud.draw_text_centered(shader, ortho, vw * 0.5f, skin_btn.y + 9.0f * base_scale,
                                   char_w * 0.82f, char_h * 0.82f, skin_label,
                                   skin.core_color.r, skin.core_color.g, skin.core_color.b, 1.0f);

            // Audio & Haptics button
            Rect2D aud_btn = get_ready_audio_button_rect(view_width, view_height);
            hud.draw_box(shader, ortho, aud_btn.x, aud_btn.y, aud_btn.w, aud_btn.h, 0.04f, 0.09f, 0.18f, 0.95f);
            char aud_label[48];
            std::snprintf(aud_label, sizeof(aud_label), "AUDIO: %s  HAPTIC: %s",
                          save.sfx_enabled ? "ON" : "OFF",
                          save.haptics_enabled ? "ON" : "OFF");
            hud.draw_text_centered(shader, ortho, vw * 0.5f, aud_btn.y + 8.0f * base_scale,
                                   char_w * 0.78f, char_h * 0.78f, aud_label, 0.85f, 0.9f, 1.0f, 0.9f);

            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 224.0f * base_scale,
                                   char_w * 0.95f, char_h * 0.95f, "TAP ANYWHERE TO LAUNCH", 1.0f, 0.85f, 0.2f, 1.0f);
        } else if (state == GameState::PAUSED) {
            float box_w = 340.0f * base_scale;
            float box_h = 210.0f * base_scale;
            float bx = (vw - box_w) * 0.5f;
            float by = (vh - box_h) * 0.40f;

            hud.draw_box(shader, ortho, bx, by, box_w, box_h, 0.01f, 0.03f, 0.08f, 0.92f);
            hud.draw_box(shader, ortho, bx, by, box_w, 2.0f * base_scale, 0.0f, 0.95f, 1.0f, 1.0f);
            hud.draw_box(shader, ortho, bx, by + box_h - 2.0f * base_scale, box_w, 2.0f * base_scale, 0.0f, 0.95f, 1.0f, 1.0f);

            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 20.0f * base_scale,
                                   char_w * 1.3f, char_h * 1.3f, "GAME PAUSED", 0.0f, 0.95f, 1.0f, 1.0f);

            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 68.0f * base_scale,
                                   char_w * 0.95f, char_h * 0.95f, "TAP TO RESUME", 1.0f, 0.85f, 0.2f, 1.0f);

            Rect2D rst_btn = get_pause_restart_button_rect(view_width, view_height);
            hud.draw_box(shader, ortho, rst_btn.x, rst_btn.y, rst_btn.w, rst_btn.h, 0.06f, 0.14f, 0.26f, 0.95f);
            hud.draw_text_centered(shader, ortho, vw * 0.5f, rst_btn.y + 9.0f * base_scale,
                                   char_w * 0.85f, char_h * 0.85f, "RESTART RUN", 0.2f, 0.95f, 1.0f, 1.0f);

            Rect2D menu_btn = get_pause_menu_button_rect(view_width, view_height);
            hud.draw_box(shader, ortho, menu_btn.x, menu_btn.y, menu_btn.w, menu_btn.h, 0.08f, 0.06f, 0.16f, 0.95f);
            hud.draw_text_centered(shader, ortho, vw * 0.5f, menu_btn.y + 9.0f * base_scale,
                                   char_w * 0.85f, char_h * 0.85f, "MAIN MENU", 1.0f, 0.55f, 0.75f, 1.0f);
        } else if (state == GameState::GAME_OVER) {
            float box_w = 350.0f * base_scale;
            float box_h = 250.0f * base_scale;
            float bx = (vw - box_w) * 0.5f;
            float by = (vh - box_h) * 0.40f;

            hud.draw_box(shader, ortho, bx, by, box_w, box_h, 0.07f, 0.01f, 0.03f, 0.90f);
            hud.draw_box(shader, ortho, bx, by, box_w, 2.0f * base_scale, 1.0f, 0.2f, 0.35f, 1.0f);
            hud.draw_box(shader, ortho, bx, by + box_h - 2.0f * base_scale, box_w, 2.0f * base_scale, 1.0f, 0.2f, 0.35f, 1.0f);

            const char* header_str = treadmill.is_time_expired() ? "TIME UP!" : "GAME OVER";
            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 16.0f * base_scale,
                                   char_w * 1.35f, char_h * 1.35f, header_str, 1.0f, 0.25f, 0.35f, 1.0f);

            if (treadmill.has_new_best()) {
                hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 46.0f * base_scale,
                                       char_w * 1.0f, char_h * 1.0f, "NEW RECORD!", 1.0f, 0.85f, 0.15f, 1.0f);
            }

            char fin_str[32];
            std::snprintf(fin_str, sizeof(fin_str), "SCORE  %d", treadmill.get_score());
            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 74.0f * base_scale,
                                   char_w, char_h, fin_str, 1.0f, 1.0f, 1.0f, 1.0f);

            char bst_res[32];
            std::snprintf(bst_res, sizeof(bst_res), "BEST   %d", mode_best);
            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 102.0f * base_scale,
                                   char_w, char_h, bst_res, 1.0f, 0.85f, 0.2f, 1.0f);

            char stats_str[48];
            std::snprintf(stats_str, sizeof(stats_str), "+%d ORBS  MAX COMBO X%d",
                          treadmill.get_run_orbs(), treadmill.get_best_run_combo());
            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 132.0f * base_scale,
                                   char_w * 0.82f, char_h * 0.82f, stats_str, 0.3f, 1.0f, 0.65f, 0.95f);

            hud.draw_text_centered(shader, ortho, vw * 0.5f, by + 166.0f * base_scale,
                                   char_w * 0.98f, char_h * 0.98f, "TAP TO RETRY", 0.0f, 0.95f, 1.0f, 1.0f);

            Rect2D menu_btn = get_gameover_menu_button_rect(view_width, view_height);
            hud.draw_box(shader, ortho, menu_btn.x, menu_btn.y, menu_btn.w, menu_btn.h, 0.12f, 0.04f, 0.08f, 0.95f);
            hud.draw_text_centered(shader, ortho, vw * 0.5f, menu_btn.y + 9.0f * base_scale,
                                   char_w * 0.82f, char_h * 0.82f, "MAIN MENU", 0.9f, 0.8f, 0.95f, 1.0f);
        }
    }

    void destroy() {
        particles.destroy();
        hud.destroy();
        sphere_mesh.destroy();
        orb_mesh.destroy();
        box_mesh.destroy();
        rail_mesh.destroy();
        track_mesh.destroy();
        shader.destroy();
    }
};

// =============================================================================
// APP STATE & PERSISTENCE CONTROLLER
// =============================================================================

struct AppState {
    struct android_app* app{nullptr};
    EglContextManager   egl{};
    FrameTimer          timer{};
    GameRenderer        renderer{};

    GameState           game_state{GameState::READY};
    PlayerBall          player{};
    TreadmillSystem     treadmill{};
    SaveData            save_data{};

    float               countdown_timer{0.0f};
    int                 countdown_value{3};

    bool                animating{false};
    float               death_shake_timer{0.0f};

    // Touch & gesture state
    bool                touch_active{false};
    float               touch_down_x{0.0f};
    float               touch_down_y{0.0f};
    float               last_touch_x{0.0f};
    float               last_touch_y{0.0f};
    bool                vertical_gesture_consumed{false};

    float               log_throttle{0.0f};
};

static std::string get_save_file_path(struct android_app* app) {
    if (app && app->activity && app->activity->internalDataPath) {
        return std::string(app->activity->internalDataPath) + "/velosphere_save.dat";
    }
    return "";
}

static std::string get_legacy_save_file_path(struct android_app* app) {
    if (app && app->activity && app->activity->internalDataPath) {
        return std::string(app->activity->internalDataPath) + "/highscore.dat";
    }
    return "";
}

static void save_profile_data(struct android_app* app, SaveData& data) {
    data.seal();
    std::string path = get_save_file_path(app);
    if (path.empty()) return;
    FILE* f = fopen(path.c_str(), "wb");
    if (f) {
        fwrite(&data, sizeof(SaveData), 1, f);
        fclose(f);
    }
    // Also mirror high_score to legacy highscore.dat for backward compatibility
    std::string legacy_path = get_legacy_save_file_path(app);
    if (!legacy_path.empty()) {
        FILE* lf = fopen(legacy_path.c_str(), "wb");
        if (lf) {
            fwrite(&data.high_score, sizeof(int), 1, lf);
            fclose(lf);
        }
    }
}

static SaveData load_profile_data(struct android_app* app) {
    SaveData data{};
    data.seal();

    std::string path = get_save_file_path(app);
    if (!path.empty()) {
        FILE* f = fopen(path.c_str(), "rb");
        if (f) {
            SaveData loaded{};
            if (fread(&loaded, sizeof(SaveData), 1, f) == 1 && loaded.is_valid()) {
                fclose(f);
                LOGI("VeloSphere: Loaded verified v2 SaveData (Best: %d, Orbs: %d)",
                     loaded.high_score, loaded.total_orbs);
                return loaded;
            }
            fclose(f);
        }
    }

    // Fallback migration from legacy v1 highscore.dat
    std::string legacy = get_legacy_save_file_path(app);
    if (!legacy.empty()) {
        FILE* lf = fopen(legacy.c_str(), "rb");
        if (lf) {
            int hs = 0;
            if (fread(&hs, sizeof(int), 1, lf) == 1 && hs > 0) {
                data.high_score = hs;
                data.seal();
                LOGI("VeloSphere: Migrated legacy high score %d to v2 SaveData", hs);
            }
            fclose(lf);
        }
    }
    return data;
}

// JNI Audio & Haptics Helpers
static void call_java_sfx(struct android_app* app, int sfx_type) {
    if (!app || !app->activity || !app->activity->vm || !app->activity->javaGameActivity) return;

    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }

    jclass cls = env->GetObjectClass(app->activity->javaGameActivity);
    if (cls) {
        jmethodID mid = env->GetStaticMethodID(cls, "playSfx", "(I)V");
        if (mid) {
            env->CallStaticVoidMethod(cls, mid, sfx_type);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
    }

    if (attached) vm->DetachCurrentThread();
}

static void trigger_haptic(struct android_app* app, int duration_ms, int amplitude) {
    if (!app || !app->activity || !app->activity->vm || !app->activity->javaGameActivity) return;

    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }

    jclass cls = env->GetObjectClass(app->activity->javaGameActivity);
    if (cls) {
        jmethodID mid = env->GetStaticMethodID(cls, "vibrate", "(II)V");
        if (mid) {
            env->CallStaticVoidMethod(cls, mid, duration_ms, amplitude);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
    }

    if (attached) vm->DetachCurrentThread();
}

static void sync_java_settings(struct android_app* app, const SaveData& save) {
    if (!app || !app->activity || !app->activity->vm || !app->activity->javaGameActivity) return;

    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }

    jclass cls = env->GetObjectClass(app->activity->javaGameActivity);
    if (cls) {
        jmethodID mid = env->GetStaticMethodID(cls, "updateSettings", "(ZZZ)V");
        if (mid) {
            env->CallStaticVoidMethod(cls, mid,
                                      static_cast<jboolean>(save.music_enabled != 0),
                                      static_cast<jboolean>(save.sfx_enabled != 0),
                                      static_cast<jboolean>(save.haptics_enabled != 0));
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
    }

    if (attached) vm->DetachCurrentThread();
}

static void finalize_run(AppState* state) {
    int final_score = state->treadmill.get_score();
    state->save_data.total_orbs += state->treadmill.get_run_orbs();
    state->save_data.total_runs += 1;
    state->save_data.total_dodges += state->treadmill.get_dodge_count();
    if (state->treadmill.get_best_run_combo() > state->save_data.best_combo) {
        state->save_data.best_combo = state->treadmill.get_best_run_combo();
    }
    state->save_data.update_best_for_mode(state->treadmill.get_game_mode(), final_score);
    state->treadmill.set_best_score(state->save_data.get_best_for_mode(state->treadmill.get_game_mode()));
    save_profile_data(state->app, state->save_data);
}

static void on_app_cmd(struct android_app* app, int32_t cmd) {
    auto* state = static_cast<AppState*>(app->userData);
    if (!state) return;

    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("Lifecycle: APP_CMD_INIT_WINDOW");
            if (app->window != nullptr) {
                if (state->egl.initialize(app->window)) {
                    state->renderer.init();
                    state->timer.reset();
                    state->animating = true;
                }
            }
            break;

        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            state->egl.update_dimensions();
            break;

        case APP_CMD_TERM_WINDOW:
            state->animating = false;
            save_profile_data(state->app, state->save_data);
            state->renderer.destroy();
            state->egl.terminate();
            break;

        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_RESUME:
            state->animating = true;
            state->timer.reset();
            sync_java_settings(state->app, state->save_data);
            break;

        case APP_CMD_LOST_FOCUS:
        case APP_CMD_PAUSE:
            if (state->game_state == GameState::PLAYING ||
                state->game_state == GameState::COUNTDOWN) {
                state->game_state = GameState::PAUSED;
            }
            state->animating = false;
            save_profile_data(state->app, state->save_data);
            break;

        case APP_CMD_DESTROY:
            save_profile_data(state->app, state->save_data);
            state->renderer.destroy();
            state->egl.terminate();
            break;

        default:
            break;
    }
}

static void process_input_events(AppState* state) {
    android_input_buffer* input_buffer = android_app_swap_input_buffers(state->app);
    if (!input_buffer) return;

    const int vw = state->egl.width;
    const int vh = state->egl.height;

    for (size_t i = 0; i < input_buffer->motionEventsCount; ++i) {
        GameActivityMotionEvent& event = input_buffer->motionEvents[i];
        int action = event.action & AMOTION_EVENT_ACTION_MASK;

        if (event.pointerCount > 0) {
            float x = GameActivityPointerAxes_getAxisValue(&event.pointers[0], AMOTION_EVENT_AXIS_X);
            float y = GameActivityPointerAxes_getAxisValue(&event.pointers[0], AMOTION_EVENT_AXIS_Y);

            if (action == AMOTION_EVENT_ACTION_DOWN) {
                state->touch_active = true;
                state->touch_down_x = x;
                state->touch_down_y = y;
                state->last_touch_x = x;
                state->last_touch_y = y;
                state->vertical_gesture_consumed = false;

                const bool pause_btn = get_pause_button_rect(vw).contains(x, y);

                if ((state->game_state == GameState::PLAYING ||
                     state->game_state == GameState::COUNTDOWN) && pause_btn) {
                    state->game_state = GameState::PAUSED;
                    state->timer.reset();
                } else if (state->game_state == GameState::READY) {
                    if (get_ready_mode_button_rect(vw, vh).contains(x, y)) {
                        int next_mode = (static_cast<int>(state->treadmill.get_game_mode()) + 1) % 3;
                        state->save_data.selected_mode = next_mode;
                        state->treadmill.set_game_mode(static_cast<GameMode>(next_mode));
                        state->treadmill.set_best_score(state->save_data.get_best_for_mode(state->treadmill.get_game_mode()));
                        save_profile_data(state->app, state->save_data);
                        call_java_sfx(state->app, 1);
                    } else if (get_ready_skin_button_rect(vw, vh).contains(x, y)) {
                        int count = static_cast<int>(SphereSkin::COUNT);
                        int next_skin = (state->save_data.selected_skin + 1) % count;
                        if (!state->save_data.is_skin_unlocked(next_skin)) {
                            state->save_data.try_unlock_skin(next_skin);
                        }
                        if (state->save_data.is_skin_unlocked(next_skin)) {
                            state->save_data.selected_skin = next_skin;
                        } else {
                            // Find next unlocked skin
                            for (int s = 1; s <= count; ++s) {
                                int cand = (state->save_data.selected_skin + s) % count;
                                if (state->save_data.is_skin_unlocked(cand)) {
                                    state->save_data.selected_skin = cand;
                                    break;
                                }
                            }
                        }
                        save_profile_data(state->app, state->save_data);
                        call_java_sfx(state->app, 1);
                    } else if (get_ready_audio_button_rect(vw, vh).contains(x, y)) {
                        uint8_t next_val = state->save_data.sfx_enabled ? 0 : 1;
                        state->save_data.sfx_enabled = next_val;
                        state->save_data.music_enabled = next_val;
                        state->save_data.haptics_enabled = next_val;
                        save_profile_data(state->app, state->save_data);
                        sync_java_settings(state->app, state->save_data);
                    } else {
                        state->treadmill.reset();
                        state->player.reset();
                        state->countdown_timer = 1.0f;
                        state->countdown_value = 3;
                        state->game_state = GameState::COUNTDOWN;
                    }
                } else if (state->game_state == GameState::PAUSED) {
                    if (get_pause_restart_button_rect(vw, vh).contains(x, y)) {
                        state->treadmill.reset();
                        state->player.reset();
                        state->death_shake_timer = 0.0f;
                        state->game_state = GameState::PLAYING;
                        state->timer.reset();
                    } else if (get_pause_menu_button_rect(vw, vh).contains(x, y)) {
                        state->treadmill.reset();
                        state->player.reset();
                        state->game_state = GameState::READY;
                        state->timer.reset();
                    } else {
                        state->game_state =
                            (state->countdown_value >= 0 && state->countdown_timer > 0.0f)
                                ? GameState::COUNTDOWN
                                : GameState::PLAYING;
                        state->timer.reset();
                    }
                } else if (state->game_state == GameState::GAME_OVER) {
                    if (get_gameover_menu_button_rect(vw, vh).contains(x, y)) {
                        state->treadmill.reset();
                        state->player.reset();
                        state->death_shake_timer = 0.0f;
                        state->game_state = GameState::READY;
                    } else {
                        state->treadmill.reset();
                        state->player.reset();
                        state->death_shake_timer = 0.0f;
                        state->game_state = GameState::PLAYING;
                    }
                }
            } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                if (state->touch_active && vw > 0) {
                    float dx = (x - state->last_touch_x) / static_cast<float>(vw);
                    float total_dy = y - state->touch_down_y;
                    float scale = hud_base_scale(vw);

                    if (state->game_state == GameState::PLAYING) {
                        state->player.apply_touch_delta(dx);

                        // Vertical swipe detection for Jump / Slam
                        if (!state->vertical_gesture_consumed) {
                            if (total_dy < -34.0f * scale) {
                                if (state->player.jump()) {
                                    call_java_sfx(state->app, 4);
                                    trigger_haptic(state->app, 25, 80);
                                }
                                state->vertical_gesture_consumed = true;
                            } else if (total_dy > 34.0f * scale) {
                                if (state->player.slam()) {
                                    trigger_haptic(state->app, 35, 120);
                                }
                                state->vertical_gesture_consumed = true;
                            }
                        }
                    }
                    state->last_touch_x = x;
                    state->last_touch_y = y;
                }
            } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
                state->touch_active = false;
                state->vertical_gesture_consumed = false;
            }
        }
    }

    android_app_clear_motion_events(input_buffer);
    android_app_clear_key_events(input_buffer);
}

} // namespace velo

// =============================================================================
// MAIN ENTRY POINT (android_main)
// =============================================================================

void android_main(struct android_app* app) {
    using namespace velo;

    LOGI("=================================================");
    LOGI("   VeloSphere 2.0 - Production 3D Arcade Engine  ");
    LOGI("=================================================");

    AppState state{};
    state.app = app;
    app->userData = &state;
    app->onAppCmd = on_app_cmd;

    state.save_data = load_profile_data(app);
    state.treadmill.set_game_mode(static_cast<GameMode>(std::clamp(state.save_data.selected_mode, 0, 2)));
    state.treadmill.set_best_score(state.save_data.get_best_for_mode(state.treadmill.get_game_mode()));
    sync_java_settings(app, state.save_data);

    while (!app->destroyRequested) {
        int events = 0;
        struct android_poll_source* source = nullptr;

        while (ALooper_pollOnce(state.animating ? 0 : -1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested) {
                break;
            }
        }

        process_input_events(&state);

        if (state.animating && state.egl.is_ready) {
            const float dt = state.timer.tick();

            if (state.game_state == GameState::COUNTDOWN) {
                state.countdown_timer -= dt;
                if (state.countdown_timer <= 0.0f) {
                    if (state.countdown_value > 0) {
                        --state.countdown_value;
                        state.countdown_timer = (state.countdown_value == 0) ? 0.5f : 1.0f;
                    } else {
                        state.game_state = GameState::PLAYING;
                        state.countdown_timer = 0.0f;
                    }
                }
            } else if (state.game_state == GameState::PLAYING) {
                const float prev_milestone = state.treadmill.get_dodge_milestone_timer();
                state.treadmill.update(dt, state.player.pos);

                if (state.treadmill.get_dodge_milestone_timer() > prev_milestone) {
                    trigger_haptic(state.app, 60, 100);
                }

                state.player.update(dt, state.treadmill.get_global_speed());

                if (std::abs(state.player.lateral_velocity) > 4.0f && !state.player.is_airborne) {
                    state.renderer.particles.spawn_trail_sparks(state.player.pos, state.player.lateral_velocity);
                }

                // Resolve pickups, shield smashes, overdrive smashes, and lethal hits
                InteractionOutcome outcome = CollisionSystem::resolve_interactions(state.player, state.treadmill);

                if (outcome.orb_picked_up) {
                    call_java_sfx(state.app, 1);
                    trigger_haptic(state.app, 18, 60);
                    state.renderer.particles.spawn_orb_burst(state.player.pos);
                }
                if (outcome.powerup_picked_up) {
                    call_java_sfx(state.app, 2);
                    trigger_haptic(state.app, 45, 140);
                    state.renderer.particles.spawn_orb_burst(state.player.pos, {0.1f, 0.95f, 1.0f});
                }
                if (outcome.obstacle_smashed || outcome.shield_broken) {
                    call_java_sfx(state.app, 3);
                    trigger_haptic(state.app, 90, 190);
                    state.death_shake_timer = 0.30f;
                    state.renderer.particles.spawn_shield_shatter(outcome.impact_pos);
                }

                if (outcome.lethal_collision) {
                    call_java_sfx(state.app, 0);
                    trigger_haptic(state.app, 140, 230);

                    if (state.treadmill.get_game_mode() == GameMode::TIME_ATTACK) {
                        // In Time Attack, hitting an obstacle deducts 10s & resets combo; ends run if clock hits 0
                        state.treadmill.reset_combo();
                        state.treadmill.apply_time_penalty(10.0f);
                        state.treadmill.trigger_banner("-10S PENALTY!", 1.1f);
                        state.player.invulnerable_timer = 1.2f;
                        state.death_shake_timer = 0.45f;
                        state.renderer.particles.spawn_explosion(state.player.pos);
                    } else {
                        state.game_state = GameState::GAME_OVER;
                        state.death_shake_timer = 0.65f;
                        state.player.trigger_collision();
                        state.renderer.particles.spawn_explosion(state.player.pos);
                        finalize_run(&state);
                    }
                }

                if (state.treadmill.is_time_expired() && state.game_state == GameState::PLAYING) {
                    call_java_sfx(state.app, 0);
                    trigger_haptic(state.app, 120, 180);
                    state.game_state = GameState::GAME_OVER;
                    finalize_run(&state);
                }

                if (state.death_shake_timer > 0.0f) {
                    state.death_shake_timer = std::max(0.0f, state.death_shake_timer - dt);
                }
            } else if (state.game_state == GameState::READY) {
                state.player.update(dt, 4.0f);
            } else if (state.game_state == GameState::GAME_OVER) {
                state.player.update(dt, 0.0f);
                if (state.death_shake_timer > 0.0f) {
                    state.death_shake_timer -= dt;
                }
            }

            state.renderer.particles.update(dt);

            state.log_throttle += dt;
            if (state.log_throttle >= 0.5f) {
                state.log_throttle = 0.0f;
                LOGI("[VeloSphere] Score: %d | Best: %d | Combo: x%d | Orbs: %d | Speed: %.1f u/s",
                     state.treadmill.get_score(),
                     state.save_data.high_score,
                     state.treadmill.get_combo(),
                     state.save_data.total_orbs + state.treadmill.get_run_orbs(),
                     state.treadmill.get_global_speed());
            }

            state.renderer.render_frame(
                state.player,
                state.treadmill,
                state.save_data,
                state.game_state,
                state.egl.width,
                state.egl.height,
                state.death_shake_timer,
                state.countdown_timer,
                state.countdown_value
            );

            state.egl.swap_buffers();
        }
    }

    save_profile_data(state.app, state.save_data);
    state.renderer.destroy();
    state.egl.terminate();
    LOGI("VeloSphere Engine Terminated Cleanly.");
}