/**
 * VeloSphere - Native 3D Endless Runner
 * Complete Game Engine Implementation (OpenGL ES 3.0, GameActivity, C++17)
 */

#include <jni.h>
#include <game-activity/GameActivity.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

// Android Native App Glue implementation
#include <game-activity/native_app_glue/android_native_app_glue.c>

// Register GameActivity native methods
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

// Cleanly undefine glue logging macros to eliminate compiler warnings
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
        if (dt > 0.1f) dt = 0.1f; // Clamp to avoid physics explosion
        return dt;
    }

private:
    TimePoint last_time_;
};

// =============================================================================
// GRAPHICS RENDERER (SHADERS, GEOMETRY & CAMERA PIPELINE)
// =============================================================================

class GameRenderer {
public:
    ShaderProgram  shader;
    SphereMesh     sphere_mesh;
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

        sphere_mesh.init(PlayerBall::RADIUS, 24, 16);
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
                      GameState state,
                      int view_width,
                      int view_height,
                      float death_shake_timer,
                      int high_score) {
        glViewport(0, 0, view_width, view_height);

        // Background color shifts on Game Over
        if (state == GameState::GAME_OVER) {
            glClearColor(0.12f, 0.02f, 0.04f, 1.0f);
        } else {
            glClearColor(0.02f, 0.03f, 0.06f, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        // Camera calculations: dynamic speed warp FOV (widens with speed)
        float aspect = (view_height > 0) ? (static_cast<float>(view_width) / static_cast<float>(view_height)) : 1.0f;
        float speed = treadmill.get_global_speed();
        float speed_frac = (speed - TreadmillSystem::BASE_SPEED) / (TreadmillSystem::MAX_SPEED - TreadmillSystem::BASE_SPEED);
        float fov = 60.0f + std::clamp(speed_frac, 0.0f, 1.0f) * 10.0f;
        Mat4 projection = Mat4::perspective(to_radians(fov), aspect, 0.1f, 150.0f);

        // Dynamic camera tracking with smooth lag and impact shake
        float shake_offset_x = 0.0f;
        float shake_offset_y = 0.0f;
        if (death_shake_timer > 0.0f) {
            shake_offset_x = std::sin(death_shake_timer * 50.0f) * 0.25f * death_shake_timer;
            shake_offset_y = std::cos(death_shake_timer * 40.0f) * 0.20f * death_shake_timer;
        }

        Vec3 eye = {
            player.pos.x * 0.30f + shake_offset_x,
            3.8f + shake_offset_y,
            -5.5f
        };
        Vec3 target = {
            player.pos.x * 0.18f,
            1.2f,
            18.0f
        };
        Vec3 up = {0.0f, 1.0f, 0.0f};
        Mat4 view = Mat4::look_at(eye, target, up);
        Mat4 vp = projection * view;

        Vec3 light_pos = {2.0f, 10.0f, -2.0f};
        shader.set_light_pos(light_pos);
        shader.set_view_pos(eye);

        // ---------------------------------------------------------------------
        // 1. RENDER TRACK RUNWAY & RAILS (NEON SYNTHWAVE)
        // ---------------------------------------------------------------------
        const auto& segments = treadmill.get_segments();
        for (const auto& seg : segments) {
            // Track runway floor
            Mat4 model_floor = Mat4::translate(0.0f, 0.0f, seg.z_position);
            shader.set_model(model_floor);
            shader.set_mvp(vp * model_floor);
            shader.set_shading_mode(1); // Neon grid mode
            shader.set_color(0.0f, 0.85f, 1.0f, 1.0f);
            shader.set_emission(0.5f);
            track_mesh.draw();

            // Left glowing side rail
            Mat4 model_left_rail = Mat4::translate(-PlayerBall::TRACK_HALF_WIDTH, 0.20f, seg.z_position);
            shader.set_model(model_left_rail);
            shader.set_mvp(vp * model_left_rail);
            shader.set_shading_mode(0); // Lit mode
            shader.set_color(0.0f, 0.9f, 1.0f, 1.0f);
            shader.set_emission(0.85f);
            rail_mesh.draw();

            // Right glowing side rail
            Mat4 model_right_rail = Mat4::translate(PlayerBall::TRACK_HALF_WIDTH, 0.20f, seg.z_position);
            shader.set_model(model_right_rail);
            shader.set_mvp(vp * model_right_rail);
            shader.set_color(0.0f, 0.9f, 1.0f, 1.0f);
            shader.set_emission(0.85f);
            rail_mesh.draw();

            // -----------------------------------------------------------------
            // 2. RENDER RECTANGULAR OBSTACLES WITH HOVER OSCILLATION
            // -----------------------------------------------------------------
            if (seg.has_obstacle) {
                Mat4 model_box =
                    Mat4::translate(seg.obstacle_x_offset, seg.obstacle_height * 0.5f + seg.hover_y, seg.z_position) *
                    Mat4::scale(seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth);

                shader.set_model(model_box);
                shader.set_mvp(vp * model_box);
                shader.set_shading_mode(2); // Glowing hazard mode

                if (seg.is_moving) {
                    // Moving hazards: pulsing electric amber
                    shader.set_color(1.0f, 0.65f, 0.0f, 1.0f);
                    shader.set_emission(0.90f);
                } else {
                    // Stationary hazards: glowing neon ruby
                    shader.set_color(1.0f, 0.12f, 0.38f, 1.0f);
                    shader.set_emission(0.75f);
                }
                box_mesh.draw();
            }
        }

        // ---------------------------------------------------------------------
        // 3. RENDER PLAYER BALL (SPHERE WITH 3D ROLLING BANDS & TUMBLING)
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

        shader.set_model(model_ball);
        shader.set_mvp(vp * model_ball);
        shader.set_shading_mode(3); // High-tech player sphere with visible rolling bands!

        if (state == GameState::GAME_OVER) {
            shader.set_color(1.0f, 0.25f, 0.25f, 1.0f);
            shader.set_emission(0.95f);
        } else {
            shader.set_color(0.12f, 0.95f, 0.88f, 1.0f);
            shader.set_emission(0.35f);
        }
        sphere_mesh.draw();

        // ---------------------------------------------------------------------
        // 4. RENDER SPARK PARTICLES
        // ---------------------------------------------------------------------
        particles.draw(shader, vp);

        // ---------------------------------------------------------------------
        // 5. RENDER ON-SCREEN ARCADE HUD (SCORE, BEST, SPEED, OVERLAYS)
        // ---------------------------------------------------------------------
        Mat4 ortho = Mat4::ortho(0.0f, static_cast<float>(view_width), static_cast<float>(view_height), 0.0f);

        float base_scale = static_cast<float>(view_width) / 450.0f;
        if (base_scale < 1.0f) base_scale = 1.0f;
        if (base_scale > 2.5f) base_scale = 2.5f;

        float char_w = 11.0f * base_scale;
        float char_h = 18.0f * base_scale;
        float pad = 16.0f * base_scale;

        // Top HUD status bar backdrop
        hud.draw_box(shader, ortho, 0.0f, 0.0f, static_cast<float>(view_width), 55.0f * base_scale, 0.02f, 0.03f, 0.08f, 0.75f);
        hud.draw_box(shader, ortho, 0.0f, 53.0f * base_scale, static_cast<float>(view_width), 2.0f * base_scale, 0.0f, 0.85f, 1.0f, 0.9f);

        char score_str[32];
        snprintf(score_str, sizeof(score_str), "SCORE %06d", treadmill.get_score());
        hud.draw_text(shader, ortho, pad, pad, char_w, char_h, score_str, 0.0f, 0.95f, 1.0f, 1.0f);

        char best_str[32];
        snprintf(best_str, sizeof(best_str), "BEST %06d", high_score);
        float best_x = static_cast<float>(view_width) - (11.0f * char_w * 1.12f) - pad;
        if (best_x < pad + 12.0f * char_w * 1.12f) best_x = static_cast<float>(view_width) * 0.52f;
        hud.draw_text(shader, ortho, best_x, pad, char_w, char_h, best_str, 1.0f, 0.85f, 0.15f, 1.0f);

        // Speedometer at bottom-left
        int speed_mph = static_cast<int>(treadmill.get_global_speed() * 1.6f);
        char spd_str[32];
        snprintf(spd_str, sizeof(spd_str), "%d MPH", speed_mph);
        hud.draw_text(shader, ortho, pad, static_cast<float>(view_height) - 40.0f * base_scale, char_w * 0.9f, char_h * 0.9f, spd_str, 0.2f, 1.0f, 0.4f, 0.9f);

        // Dodge bonus indicator
        if (treadmill.get_bonus_timer() > 0.0f) {
            float bonus_alpha = std::min(1.0f, treadmill.get_bonus_timer());
            float bonus_x = static_cast<float>(view_width) * 0.5f - 60.0f * base_scale;
            float bonus_y = 70.0f * base_scale + (1.2f - treadmill.get_bonus_timer()) * 15.0f * base_scale;
            hud.draw_text(shader, ortho, bonus_x, bonus_y, char_w * 1.05f, char_h * 1.05f, "+100 DODGE!", 1.0f, 0.2f, 0.8f, bonus_alpha);
        }

        // Center state overlays
        if (state == GameState::READY) {
            float box_w = 320.0f * base_scale;
            float box_h = 140.0f * base_scale;
            float bx = (static_cast<float>(view_width) - box_w) * 0.5f;
            float by = (static_cast<float>(view_height) - box_h) * 0.42f;

            hud.draw_box(shader, ortho, bx, by, box_w, box_h, 0.01f, 0.03f, 0.08f, 0.85f);
            hud.draw_box(shader, ortho, bx, by, box_w, 2.0f * base_scale, 0.0f, 0.95f, 1.0f, 1.0f);
            hud.draw_box(shader, ortho, bx, by + box_h - 2.0f * base_scale, box_w, 2.0f * base_scale, 0.0f, 0.95f, 1.0f, 1.0f);

            float t_w = char_w * 1.35f;
            float t_h = char_h * 1.35f;
            float tx = (static_cast<float>(view_width) - 10.0f * t_w * 1.12f) * 0.5f;
            hud.draw_text(shader, ortho, tx, by + 20.0f * base_scale, t_w, t_h, "VELOSPHERE", 0.0f, 0.95f, 1.0f, 1.0f);

            float sx = (static_cast<float>(view_width) - 14.0f * char_w * 0.9f * 1.12f) * 0.5f;
            hud.draw_text(shader, ortho, sx, by + 65.0f * base_scale, char_w * 0.9f, char_h * 0.9f, "SWIPE TO STEER", 0.8f, 0.9f, 1.0f, 0.85f);

            float px = (static_cast<float>(view_width) - 17.0f * char_w * 0.95f * 1.12f) * 0.5f;
            hud.draw_text(shader, ortho, px, by + 95.0f * base_scale, char_w * 0.95f, char_h * 0.95f, "TAP SCREEN TO RUN", 1.0f, 0.85f, 0.2f, 1.0f);
        } else if (state == GameState::GAME_OVER) {
            float box_w = 340.0f * base_scale;
            float box_h = 185.0f * base_scale;
            float bx = (static_cast<float>(view_width) - box_w) * 0.5f;
            float by = (static_cast<float>(view_height) - box_h) * 0.40f;

            hud.draw_box(shader, ortho, bx, by, box_w, box_h, 0.07f, 0.01f, 0.03f, 0.88f);
            hud.draw_box(shader, ortho, bx, by, box_w, 2.0f * base_scale, 1.0f, 0.2f, 0.35f, 1.0f);
            hud.draw_box(shader, ortho, bx, by + box_h - 2.0f * base_scale, box_w, 2.0f * base_scale, 1.0f, 0.2f, 0.35f, 1.0f);

            float go_w = char_w * 1.4f;
            float go_h = char_h * 1.4f;
            float gx = (static_cast<float>(view_width) - 9.0f * go_w * 1.12f) * 0.5f;
            hud.draw_text(shader, ortho, gx, by + 18.0f * base_scale, go_w, go_h, "GAME OVER", 1.0f, 0.25f, 0.35f, 1.0f);

            char fin_str[32];
            snprintf(fin_str, sizeof(fin_str), "SCORE  %d", treadmill.get_score());
            float fx = (static_cast<float>(view_width) - static_cast<float>(strlen(fin_str)) * char_w * 1.12f) * 0.5f;
            hud.draw_text(shader, ortho, fx, by + 65.0f * base_scale, char_w, char_h, fin_str, 1.0f, 1.0f, 1.0f, 1.0f);

            char bst_res[32];
            snprintf(bst_res, sizeof(bst_res), "BEST   %d", high_score);
            float bx_pos = (static_cast<float>(view_width) - static_cast<float>(strlen(bst_res)) * char_w * 1.12f) * 0.5f;
            hud.draw_text(shader, ortho, bx_pos, by + 95.0f * base_scale, char_w, char_h, bst_res, 1.0f, 0.85f, 0.2f, 1.0f);

            float rx = (static_cast<float>(view_width) - 12.0f * char_w * 1.12f) * 0.5f;
            hud.draw_text(shader, ortho, rx, by + 140.0f * base_scale, char_w, char_h, "TAP TO RETRY", 0.0f, 0.95f, 1.0f, 1.0f);
        }
    }

    void destroy() {
        particles.destroy();
        hud.destroy();
        sphere_mesh.destroy();
        box_mesh.destroy();
        rail_mesh.destroy();
        track_mesh.destroy();
        shader.destroy();
    }
};

// =============================================================================
// APP STATE & ENGINE CONTROLLER
// =============================================================================

struct AppState {
    struct android_app* app{nullptr};
    EglContextManager   egl{};
    FrameTimer          timer{};
    GameRenderer        renderer{};

    GameState           game_state{GameState::READY};
    PlayerBall          player{};
    TreadmillSystem     treadmill{};

    bool                animating{false};
    float               death_shake_timer{0.0f};
    int                 high_score{0};

    // Touch input state
    bool                touch_active{false};
    float               last_touch_x{0.0f};

    // Logging throttle
    float               log_throttle{0.0f};
};

// Persistent high score helpers
static std::string get_save_file_path(struct android_app* app) {
    if (app && app->activity && app->activity->internalDataPath) {
        return std::string(app->activity->internalDataPath) + "/highscore.dat";
    }
    return "";
}

static int load_saved_high_score(struct android_app* app) {
    std::string path = get_save_file_path(app);
    if (path.empty()) return 0;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return 0;
    int hs = 0;
    if (fread(&hs, sizeof(int), 1, f) == 1) {
        LOGI("VeloSphere: Loaded persistent high score: %d from %s", hs, path.c_str());
    } else {
        hs = 0;
    }
    fclose(f);
    return hs;
}

static void save_high_score(struct android_app* app, int score) {
    std::string path = get_save_file_path(app);
    if (path.empty()) return;
    FILE* f = fopen(path.c_str(), "wb");
    if (f) {
        fwrite(&score, sizeof(int), 1, f);
        fclose(f);
        LOGI("VeloSphere: Persisted high score: %d to %s", score, path.c_str());
    }
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
            LOGI("Lifecycle: APP_CMD_WINDOW_RESIZED");
            state->egl.update_dimensions();
            break;

        case APP_CMD_CONFIG_CHANGED:
            LOGI("Lifecycle: APP_CMD_CONFIG_CHANGED");
            state->egl.update_dimensions();
            break;

        case APP_CMD_TERM_WINDOW:
            LOGI("Lifecycle: APP_CMD_TERM_WINDOW");
            state->animating = false;
            save_high_score(state->app, state->high_score);
            state->renderer.destroy();
            state->egl.terminate();
            break;

        case APP_CMD_GAINED_FOCUS:
            LOGI("Lifecycle: APP_CMD_GAINED_FOCUS");
            state->animating = true;
            state->timer.reset();
            break;

        case APP_CMD_LOST_FOCUS:
            LOGI("Lifecycle: APP_CMD_LOST_FOCUS");
            state->animating = false;
            break;

        case APP_CMD_RESUME:
            LOGI("Lifecycle: APP_CMD_RESUME");
            state->animating = true;
            state->timer.reset();
            break;

        case APP_CMD_PAUSE:
            LOGI("Lifecycle: APP_CMD_PAUSE");
            state->animating = false;
            save_high_score(state->app, state->high_score);
            break;

        case APP_CMD_DESTROY:
            LOGI("Lifecycle: APP_CMD_DESTROY");
            save_high_score(state->app, state->high_score);
            state->renderer.destroy();
            state->egl.terminate();
            break;

        default:
            break;
    }
}

// Process touch/drag motion events from GameActivity
static void process_input_events(AppState* state) {
    android_input_buffer* input_buffer = android_app_swap_input_buffers(state->app);
    if (!input_buffer) return;

    for (size_t i = 0; i < input_buffer->motionEventsCount; ++i) {
        GameActivityMotionEvent& event = input_buffer->motionEvents[i];
        int action = event.action & AMOTION_EVENT_ACTION_MASK;

        if (event.pointerCount > 0) {
            float x = GameActivityPointerAxes_getAxisValue(&event.pointers[0], AMOTION_EVENT_AXIS_X);
            float y = GameActivityPointerAxes_getAxisValue(&event.pointers[0], AMOTION_EVENT_AXIS_Y);
            (void)y; // Silence unused warning

            if (action == AMOTION_EVENT_ACTION_DOWN) {
                state->touch_active = true;
                state->last_touch_x = x;

                if (state->game_state == GameState::READY) {
                    state->game_state = GameState::PLAYING;
                    LOGI("VeloSphere: Game Started!");
                } else if (state->game_state == GameState::GAME_OVER) {
                    // Tap to restart
                    state->treadmill.reset();
                    state->player.reset();
                    state->death_shake_timer = 0.0f;
                    state->game_state = GameState::PLAYING;
                    LOGI("VeloSphere: Game Restarted!");
                }
            } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                if (state->touch_active && state->egl.width > 0) {
                    float dx = (x - state->last_touch_x) / static_cast<float>(state->egl.width);
                    if (state->game_state == GameState::PLAYING) {
                        state->player.apply_touch_delta(dx);
                    }
                    state->last_touch_x = x;
                }
            } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
                state->touch_active = false;
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
    LOGI("   VeloSphere Engine - Modern 3D Arcade Runner   ");
    LOGI("=================================================");

    AppState state{};
    state.app = app;
    app->userData = &state;
    app->onAppCmd = on_app_cmd;

    // Load persistent high score from internal storage
    state.high_score = load_saved_high_score(app);
    state.treadmill.set_best_score(state.high_score);

    while (!app->destroyRequested) {
        int events = 0;
        struct android_poll_source* source = nullptr;

        // Non-blocking poll while animating, blocking poll when paused/minimized
        while (ALooper_pollOnce(state.animating ? 0 : -1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested) {
                break;
            }
        }

        // Process touch controls
        process_input_events(&state);

        // Active rendering & physics update loop
        if (state.animating && state.egl.is_ready) {
            const float dt = state.timer.tick();

            // State-specific gameplay logic
            if (state.game_state == GameState::PLAYING) {
                // 1. Advance treadmill coordinate system and global speed
                state.treadmill.update(dt);

                // 2. Advance player lateral movement and rolling rotation
                state.player.update(dt, state.treadmill.get_global_speed());

                // Spawn friction sparks on sharp high-speed steering
                if (std::abs(state.player.lateral_velocity) > 4.0f) {
                    state.renderer.particles.spawn_trail_sparks(state.player.pos, state.player.lateral_velocity);
                }

                // 3. Collision detection: Player Sphere vs Hazard Obstacles
                if (CollisionSystem::check_collision(state.player, state.treadmill)) {
                    state.game_state = GameState::GAME_OVER;
                    state.death_shake_timer = 0.65f;

                    // Trigger realistic collision impact tumble and explosion sparks
                    state.player.trigger_collision();
                    state.renderer.particles.spawn_explosion(state.player.pos);

                    int final_score = state.treadmill.get_score();
                    if (final_score > state.high_score) {
                        state.high_score = final_score;
                        state.treadmill.set_best_score(final_score);
                        save_high_score(state.app, final_score);
                        LOGI("VeloSphere: [NEW RECORD!] Score: %d persisted!", final_score);
                    }
                    LOGI("VeloSphere: [COLLISION] GAME OVER! Final Score: %d | Best: %d",
                         final_score, state.high_score);
                }
            } else if (state.game_state == GameState::READY) {
                // Slow idle drift while waiting for start
                state.player.update(dt, 4.0f);
            } else if (state.game_state == GameState::GAME_OVER) {
                state.player.update(dt, 0.0f);
                if (state.death_shake_timer > 0.0f) {
                    state.death_shake_timer -= dt;
                }
            }

            // Update spark particle physics
            state.renderer.particles.update(dt);

            // Periodic Logcat statistics (~2 per second)
            state.log_throttle += dt;
            if (state.log_throttle >= 0.5f) {
                state.log_throttle = 0.0f;
                const char* state_str =
                    (state.game_state == GameState::PLAYING) ? "PLAYING" :
                    (state.game_state == GameState::READY)   ? "READY (Tap to start)" : "GAME OVER (Tap to retry)";

                LOGI("[VeloSphere] State: %s | Score: %d | Best: %d | Speed: %.1f u/s | Ball X: %.2f",
                     state_str,
                     state.treadmill.get_score(),
                     state.high_score,
                     state.treadmill.get_global_speed(),
                     state.player.pos.x);
            }

            // 4. Render 3D scene & Arcade HUD
            state.renderer.render_frame(
                state.player,
                state.treadmill,
                state.game_state,
                state.egl.width,
                state.egl.height,
                state.death_shake_timer,
                state.high_score
            );

            // 5. Swap front and back display buffers
            state.egl.swap_buffers();
        }
    }

    save_high_score(state.app, state.high_score);
    state.renderer.destroy();
    state.egl.terminate();
    LOGI("VeloSphere Engine Terminated Cleanly.");
}
