/**
 * VeloSphere 3.0 — Overhauled 3D Synthwave Arcade Engine
 * OpenGL ES 3.0 | GameActivity 4.4.2 | C++17 | Zero-Allocation Render Loop
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
#include <cmath>

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

// SFX IDs synchronized with MainActivity.kt
enum SfxId : int {
    SFX_HIT             = 0,
    SFX_ORB             = 1,
    SFX_POWERUP         = 2,
    SFX_SHIELD_SMASH    = 3,
    SFX_JUMP            = 4,
    SFX_SLAM            = 5,
    SFX_NEAR_MISS       = 6,
    SFX_FEVER           = 7,
    SFX_UI_CLICK        = 8,
    SFX_UNLOCK          = 9,
    SFX_MISSION_DONE    = 10,
    SFX_COUNTDOWN       = 11
};

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

        // Enable vsync pacing (1 swap interval) for jitter-free frame delivery
        eglSwapInterval(display, 1);

        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);

        LOGI("OpenGL ES 3.0 Initialized: %dx%d", width, height);

        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glCullFace(GL_BACK);

        is_ready = true;
        return true;
    }

    void update_dimensions() {
        if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
            eglQuerySurface(display, surface, EGL_WIDTH, &width);
            eglQuerySurface(display, surface, EGL_HEIGHT, &height);
            glViewport(0, 0, width, height);
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
    }
};

// =============================================================================
// FRAME TIMER & FPS MONITOR
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
        if (dt > 0.05f) dt = 0.05f;
        if (dt > 0.0001f) {
            float inst_fps = 1.0f / dt;
            smoothed_fps_ = smoothed_fps_ * 0.90f + inst_fps * 0.10f;
        }
        return dt;
    }

    float get_fps() const { return smoothed_fps_; }

private:
    TimePoint last_time_;
    float smoothed_fps_{60.0f};
};

// =============================================================================
// RESPONSIVE UI LAYOUT (SHARED BY RENDERER & TOUCH HIT-TESTING)
// =============================================================================

static float hud_base_scale(int view_width) {
    float s = static_cast<float>(view_width) / 420.0f;
    return std::clamp(s, 0.9f, 2.6f);
}

struct UiLayout {
    float vw{1080.0f};
    float vh{1920.0f};
    float s{2.0f};

    // Shared header / back
    Rect2D back_btn{};
    Rect2D pause_btn{};

    // HOME screen
    Rect2D home_mode_btn{};
    Rect2D home_play_btn{};
    Rect2D home_garage_btn{};
    Rect2D home_missions_btn{};
    Rect2D home_settings_btn{};

    // GARAGE screen
    Rect2D garage_tab_skins{};
    Rect2D garage_tab_upgrades{};
    Rect2D garage_prev_skin{};
    Rect2D garage_next_skin{};
    Rect2D garage_action_skin{};
    Rect2D garage_upgrade_btns[4]{};

    // SETTINGS screen (7 rows)
    Rect2D settings_rows[7]{};

    // PAUSE screen
    Rect2D pause_resume_btn{};
    Rect2D pause_restart_btn{};
    Rect2D pause_audio_btn{};
    Rect2D pause_home_btn{};

    // GAME OVER screen
    Rect2D over_revive_btn{};
    Rect2D over_retry_btn{};
    Rect2D over_home_btn{};

    static UiLayout compute(int width, int height) {
        UiLayout L{};
        L.vw = static_cast<float>(width);
        L.vh = static_cast<float>(height);
        L.s  = hud_base_scale(width);
        const float s = L.s;
        const float cx = L.vw * 0.5f;
        const float pad = 16.0f * s;

        // Shared Top Back & Pause buttons
        L.back_btn  = {pad, 14.0f * s, 94.0f * s, 38.0f * s};
        L.pause_btn = {cx - 26.0f * s, 10.0f * s, 52.0f * s, 40.0f * s};

        // HOME Screen (leaves center 3D showcase clear for the glowing sphere!)
        const float card_w = std::min(L.vw - pad * 2.0f, 360.0f * s);
        const float card_x = cx - card_w * 0.5f;
        const float dock_h = 52.0f * s;
        const float dock_y = L.vh - dock_h - 18.0f * s;
        const float gap = 8.0f * s;
        const float dock_w = (card_w - gap * 2.0f) / 3.0f;

        L.home_garage_btn   = {card_x, dock_y, dock_w, dock_h};
        L.home_missions_btn = {card_x + dock_w + gap, dock_y, dock_w, dock_h};
        L.home_settings_btn = {card_x + (dock_w + gap) * 2.0f, dock_y, dock_w, dock_h};

        L.home_play_btn = {card_x, dock_y - 68.0f * s, card_w, 56.0f * s};
        L.home_mode_btn = {card_x, L.home_play_btn.y - 48.0f * s, card_w, 38.0f * s};

        // GARAGE Screen
        const float tab_y = 66.0f * s;
        const float tab_w = (card_w - gap) * 0.5f;
        L.garage_tab_skins    = {card_x, tab_y, tab_w, 38.0f * s};
        L.garage_tab_upgrades = {card_x + tab_w + gap, tab_y, tab_w, 38.0f * s};

        const float skin_ctrl_y = L.vh - 150.0f * s;
        L.garage_prev_skin   = {card_x, skin_ctrl_y, 58.0f * s, 48.0f * s};
        L.garage_next_skin   = {card_x + card_w - 58.0f * s, skin_ctrl_y, 58.0f * s, 48.0f * s};
        L.garage_action_skin = {card_x, skin_ctrl_y + 62.0f * s, card_w, 50.0f * s};

        const float upg_start_y = tab_y + 58.0f * s;
        const float upg_h = 74.0f * s;
        for (int i = 0; i < 4; ++i) {
            float ry = upg_start_y + static_cast<float>(i) * (upg_h + 12.0f * s);
            L.garage_upgrade_btns[i] = {card_x, ry, card_w, upg_h};
        }

        // SETTINGS Screen (7 stacked rows)
        const float set_start_y = 84.0f * s;
        const float set_h = 50.0f * s;
        for (int i = 0; i < 7; ++i) {
            float ry = set_start_y + static_cast<float>(i) * (set_h + 10.0f * s);
            L.settings_rows[i] = {card_x, ry, card_w, set_h};
        }

        // PAUSE Overlay
        const float p_w = std::min(L.vw - pad * 2.0f, 330.0f * s);
        const float p_x = cx - p_w * 0.5f;
        const float p_y = L.vh * 0.28f;
        L.pause_resume_btn  = {p_x + 20.0f * s, p_y + 66.0f * s,  p_w - 40.0f * s, 44.0f * s};
        L.pause_restart_btn = {p_x + 20.0f * s, p_y + 120.0f * s, p_w - 40.0f * s, 40.0f * s};
        L.pause_audio_btn   = {p_x + 20.0f * s, p_y + 170.0f * s, p_w - 40.0f * s, 40.0f * s};
        L.pause_home_btn    = {p_x + 20.0f * s, p_y + 220.0f * s, p_w - 40.0f * s, 40.0f * s};

        // GAME OVER Overlay
        const float go_w = std::min(L.vw - pad * 2.0f, 350.0f * s);
        const float go_x = cx - go_w * 0.5f;
        const float go_y = L.vh * 0.21f;
        L.over_revive_btn = {go_x + 20.0f * s, go_y + 208.0f * s, go_w - 40.0f * s, 44.0f * s};
        L.over_retry_btn  = {go_x + 20.0f * s, go_y + 262.0f * s, go_w - 40.0f * s, 48.0f * s};
        L.over_home_btn   = {go_x + 20.0f * s, go_y + 320.0f * s, go_w - 40.0f * s, 40.0f * s};

        return L;
    }
};

// =============================================================================
// GRAPHICS RENDERER (MULTI-SHADER ZERO-LAG PIPELINE)
// =============================================================================

class GameRenderer {
public:
    SceneShader    scene_shader;
    SkyShader      sky_shader;
    ParticleShader particle_shader;
    UiShader       ui_shader;

    SphereMesh     sphere_mesh;
    CrystalMesh    crystal_mesh;
    BoxMesh        box_mesh;
    BoxMesh        rail_mesh;
    TrackMesh      track_mesh;
    QuadMesh       sky_quad;
    TrailRibbon    trail;
    HudRenderer    hud;
    ParticleSystem particles;

    float          elapsed_time{0.0f};

    bool init() {
        if (!scene_shader.build()) {
            LOGE("Failed to build SceneShader");
            return false;
        }
        if (!sky_shader.build()) {
            LOGE("Failed to build SkyShader");
            return false;
        }
        if (!particle_shader.build()) {
            LOGE("Failed to build ParticleShader");
            return false;
        }
        if (!ui_shader.build()) {
            LOGE("Failed to build UiShader");
            return false;
        }

        sphere_mesh.init(PlayerBall::RADIUS, 28, 18);
        crystal_mesh.init(0.36f, 1.30f);
        box_mesh.init(1.0f, 1.0f, 1.0f);
        rail_mesh.init(0.24f, 0.40f, TreadmillSystem::SEGMENT_SPACING);
        track_mesh.init(PlayerBall::TRACK_HALF_WIDTH * 2.0f, TreadmillSystem::SEGMENT_SPACING);
        sky_quad.init(2.0f, 2.0f);
        trail.init();
        hud.init();
        particles.init();

        LOGI("VeloSphere 3.0 GameRenderer initialized (4 specialized GLES 3.0 shaders).");
        return true;
    }

    void render_frame(const PlayerBall& player,
                      const TreadmillSystem& treadmill,
                      const SaveData& save,
                      GameState state,
                      GarageTab garage_tab,
                      int garage_skin_idx,
                      int view_width,
                      int view_height,
                      float death_shake_timer,
                      float countdown_timer,
                      int countdown_value,
                      float fps) {
        glViewport(0, 0, view_width, view_height);

        const BiomePalette biome = treadmill.get_interpolated_biome();
        const int quality = static_cast<int>(save.graphics_quality);
        particles.set_quality_limit(quality);

        Color3 sky_fog = biome.sky_fog;
        if (state == GameState::GAME_OVER) {
            sky_fog = {0.11f, 0.02f, 0.04f};
        }

        glClearColor(sky_fog.r, sky_fog.g, sky_fog.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---------------------------------------------------------------------
        // 0. PROCEDURAL SYNTHWAVE HORIZON SKYBOX (1 DRAW CALL)
        // ---------------------------------------------------------------------
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        sky_shader.use();
        sky_shader.set_params(
            sky_fog,
            biome.sun_top,
            biome.sun_bottom,
            biome.grid_color,
            elapsed_time,
            player.pos.x,
            treadmill.is_fever_active() ? 1.0f : 0.0f,
            quality
        );
        sky_quad.draw();
        glDepthMask(GL_TRUE);

        // ---------------------------------------------------------------------
        // CAMERA SETUP (DYNAMIC CHASE vs HOME SHOWCASE vs GARAGE TURNTABLE)
        // ---------------------------------------------------------------------
        float aspect = (view_height > 0) ? (static_cast<float>(view_width) / static_cast<float>(view_height)) : 1.0f;
        float speed = treadmill.get_global_speed();
        float speed_frac = (speed - TreadmillSystem::BASE_SPEED) / (TreadmillSystem::MAX_SPEED - TreadmillSystem::BASE_SPEED);
        float extra_fov = (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) ? 7.0f
                        : (treadmill.is_fever_active() ? 4.0f : 0.0f);
        float fov = 60.0f + std::clamp(speed_frac, 0.0f, 1.2f) * 10.0f + extra_fov;
        Mat4 projection = Mat4::perspective(to_radians(fov), aspect, 0.1f, 160.0f);

        float shake_x = 0.0f;
        float shake_y = 0.0f;
        if (death_shake_timer > 0.0f) {
            shake_x = std::sin(death_shake_timer * 52.0f) * 0.24f * death_shake_timer;
            shake_y = std::cos(death_shake_timer * 42.0f) * 0.18f * death_shake_timer;
        }

        Vec3 eye{};
        Vec3 target{};
        if (state == GameState::GARAGE && garage_tab == GarageTab::SKINS) {
            eye    = {std::sin(elapsed_time * 0.45f) * 0.6f, 2.05f, -3.1f};
            target = {0.0f, 0.95f, 0.0f};
        } else if (state == GameState::HOME) {
            eye    = {std::sin(elapsed_time * 0.35f) * 0.85f, 3.2f, -4.6f};
            target = {0.0f, 1.15f, 12.0f};
        } else {
            eye = {
                player.pos.x * 0.32f + shake_x,
                3.75f + player.jump_y * 0.25f + shake_y,
                -5.5f
            };
            target = {
                player.pos.x * 0.18f,
                1.20f + player.jump_y * 0.15f,
                19.0f
            };
        }
        Mat4 view = Mat4::look_at(eye, target, {0.0f, 1.0f, 0.0f});
        Mat4 vp = projection * view;

        // ---------------------------------------------------------------------
        // 1. RENDER 3D TRACK, RAILS, CRYSTALS & OBSTACLES
        // ---------------------------------------------------------------------
        scene_shader.use();
        scene_shader.set_sky_color(sky_fog.r, sky_fog.g, sky_fog.b);
        scene_shader.set_light_pos({2.5f, 11.0f, -2.5f});
        scene_shader.set_view_pos(eye);
        scene_shader.set_time(elapsed_time);
        scene_shader.set_inv_scale(1.0f, 1.0f, 1.0f);

        const auto& segments = treadmill.get_segments();
        const float spin = treadmill.get_orb_spin_angle();
        const bool show_obstacles = (state == GameState::PLAYING || state == GameState::PAUSED ||
                                     state == GameState::COUNTDOWN || state == GameState::GAME_OVER);

        // Runway floor (single-sided plane -> disable culling)
        glDisable(GL_CULL_FACE);
        scene_shader.set_shading_mode(1);
        scene_shader.set_color(biome.floor_base.r, biome.floor_base.g, biome.floor_base.b, 1.0f);
        scene_shader.set_energy_color(biome.grid_color.r, biome.grid_color.g, biome.grid_color.b);
        scene_shader.set_emission(0.55f);
        for (const auto& seg : segments) {
            Mat4 model_floor = Mat4::translate(0.0f, 0.0f, seg.z_position);
            scene_shader.set_model(model_floor);
            scene_shader.set_mvp(vp * model_floor);
            track_mesh.draw();
        }

        // Closed 3D meshes -> enable back-face culling for ~50% fill-rate savings!
        glEnable(GL_CULL_FACE);

        // Side rails & lane dividers
        scene_shader.set_shading_mode(0);
        for (const auto& seg : segments) {
            scene_shader.set_color(biome.rail_color.r, biome.rail_color.g, biome.rail_color.b, 1.0f);
            scene_shader.set_emission(0.85f);

            Mat4 lr = Mat4::translate(-PlayerBall::TRACK_HALF_WIDTH, 0.20f, seg.z_position);
            scene_shader.set_model(lr);
            scene_shader.set_mvp(vp * lr);
            rail_mesh.draw();

            Mat4 rr = Mat4::translate(PlayerBall::TRACK_HALF_WIDTH, 0.20f, seg.z_position);
            scene_shader.set_model(rr);
            scene_shader.set_mvp(vp * rr);
            rail_mesh.draw();

            const float div_x = TreadmillSystem::LANE_WIDTH * 0.5f;
            scene_shader.set_color(biome.grid_color.r * 0.48f, biome.grid_color.g * 0.48f, biome.grid_color.b * 0.58f, 1.0f);
            scene_shader.set_emission(0.35f);

            Mat4 ld = Mat4::translate(-div_x, 0.018f, seg.z_position) *
                      Mat4::scale(0.045f, 0.025f, TreadmillSystem::SEGMENT_SPACING * 0.90f);
            scene_shader.set_model(ld);
            scene_shader.set_mvp(vp * ld);
            box_mesh.draw();

            Mat4 rd = Mat4::translate(div_x, 0.018f, seg.z_position) *
                      Mat4::scale(0.045f, 0.025f, TreadmillSystem::SEGMENT_SPACING * 0.90f);
            scene_shader.set_model(rd);
            scene_shader.set_mvp(vp * rd);
            box_mesh.draw();
        }

        // Collectibles & Obstacles (only during active run / pause / countdown / gameover)
        if (show_obstacles) {
            scene_shader.set_shading_mode(2);
            for (const auto& seg : segments) {
                // Collectible Data Orb Crystal
                if (seg.has_orb && !seg.orb_collected) {
                    float bob = std::sin(spin * 2.0f + seg.z_position * 0.2f) * 0.14f;
                    Mat4 m = Mat4::translate(seg.orb_pos.x, seg.orb_pos.y + bob, seg.orb_pos.z) *
                             Mat4::rotate_y(spin * 1.4f);
                    scene_shader.set_model(m);
                    scene_shader.set_mvp(vp * m);
                    scene_shader.set_color(1.0f, 0.86f, 0.12f, 1.0f);
                    scene_shader.set_emission(1.15f);
                    crystal_mesh.draw();
                }

                // Collectible Power-Up Crystal (larger & color-coded)
                if (seg.powerup_type != PowerUpType::NONE && !seg.powerup_collected) {
                    float bob = std::cos(spin * 2.2f + seg.z_position * 0.3f) * 0.16f;
                    Mat4 m = Mat4::translate(seg.powerup_pos.x, seg.powerup_pos.y + bob, seg.powerup_pos.z) *
                             Mat4::rotate_y(spin * 1.8f) *
                             Mat4::rotate_x(spin * 0.7f) *
                             Mat4::scale(1.45f, 1.45f, 1.45f);
                    scene_shader.set_model(m);
                    scene_shader.set_mvp(vp * m);
                    switch (seg.powerup_type) {
                        case PowerUpType::SHIELD:    scene_shader.set_color(0.05f, 0.95f, 1.00f, 1.0f); break;
                        case PowerUpType::OVERDRIVE: scene_shader.set_color(1.00f, 0.22f, 0.85f, 1.0f); break;
                        case PowerUpType::SLOW_MO:   scene_shader.set_color(0.60f, 0.32f, 1.00f, 1.0f); break;
                        case PowerUpType::MAGNET:
                        default:                     scene_shader.set_color(0.15f, 1.00f, 0.45f, 1.0f); break;
                    }
                    scene_shader.set_emission(1.30f);
                    crystal_mesh.draw();
                }

                // Obstacles (Primary + Optional Dual-Lane Second Blocker)
                if (seg.has_obstacle) {
                    auto draw_block = [&](float ox, float oy, float oz, float w, float h, float d) {
                        Mat4 m = Mat4::translate(ox, oy, oz) * Mat4::scale(w, h, d);
                        scene_shader.set_model(m);
                        scene_shader.set_mvp(vp * m);
                        box_mesh.draw();
                    };

                    if (seg.is_moving) {
                        if (seg.motion_warning_time > 0.0f) {
                            float wp = 1.0f - (seg.motion_warning_time / TreadmillSystem::MOVING_WARNING_TIME);
                            float pulse = 0.75f + 0.25f * std::sin(wp * 18.0f);
                            scene_shader.set_color(1.0f, 0.82f, 0.10f, 1.0f);
                            scene_shader.set_emission(1.05f + 0.65f * pulse);
                        } else {
                            scene_shader.set_color(1.0f, 0.62f, 0.05f, 1.0f);
                            scene_shader.set_emission(0.88f);
                        }
                        draw_block(seg.obstacle_x_offset, seg.hover_y + seg.obstacle_height * 0.5f,
                                   seg.z_position, seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth);
                    } else if (seg.obstacle_type == ObstacleType::LOW_BARRIER) {
                        scene_shader.set_color(0.88f, 0.15f, 1.0f, 1.0f);
                        scene_shader.set_emission(0.95f);
                        draw_block(seg.obstacle_x_offset, seg.hover_y + seg.obstacle_height * 0.5f,
                                   seg.z_position, seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth);
                    } else if (seg.obstacle_type == ObstacleType::OVERHEAD_GATE) {
                        scene_shader.set_color(1.0f, 0.28f, 0.14f, 1.0f);
                        scene_shader.set_emission(0.85f);
                        draw_block(seg.obstacle_x_offset, seg.hover_y + seg.obstacle_height * 0.5f,
                                   seg.z_position, seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth);

                        float ph = seg.hover_y;
                        float px = seg.obstacle_width * 0.44f;
                        draw_block(seg.obstacle_x_offset - px, ph * 0.5f, seg.z_position, 0.16f, ph, seg.obstacle_depth * 0.5f);
                        draw_block(seg.obstacle_x_offset + px, ph * 0.5f, seg.z_position, 0.16f, ph, seg.obstacle_depth * 0.5f);
                    } else {
                        scene_shader.set_color(1.0f, 0.12f, 0.38f, 1.0f);
                        scene_shader.set_emission(0.78f);
                        draw_block(seg.obstacle_x_offset, seg.hover_y + seg.obstacle_height * 0.5f,
                                   seg.z_position, seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth);
                        if (seg.has_second_obstacle) {
                            draw_block(seg.second_obstacle_x, seg.hover_y + seg.obstacle_height * 0.5f,
                                       seg.z_position, seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth);
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------------------
        // 2. GROUND CONTACT SHADOW & 3D NEON TRAIL RIBBON
        // ---------------------------------------------------------------------
        glDisable(GL_CULL_FACE);
        int display_skin_idx = (state == GameState::GARAGE)
            ? std::clamp(garage_skin_idx, 0, static_cast<int>(SKIN_PALETTES.size()) - 1)
            : std::clamp(save.selected_skin, 0, static_cast<int>(SKIN_PALETTES.size()) - 1);
        const auto& skin = SKIN_PALETTES[static_cast<size_t>(display_skin_idx)];

        if (!player.is_tumbling) {
            trail.draw_contact_shadow(particle_shader, vp, player.pos, player.jump_y);
            if (state == GameState::PLAYING || state == GameState::HOME) {
                Color3 trail_col = skin.energy_color;
                float trail_int = (quality == 0) ? 0.65f : 1.0f;
                if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
                    trail_col = {1.0f, 0.25f, 0.90f};
                    trail_int = 1.35f;
                } else if (treadmill.is_fever_active()) {
                    float h = elapsed_time * 4.0f;
                    trail_col = {0.55f + 0.45f * std::sin(h),
                                 0.55f + 0.45f * std::sin(h + 2.09f),
                                 0.55f + 0.45f * std::sin(h + 4.18f)};
                    trail_int = 1.30f;
                }
                trail.draw_trail(particle_shader, vp, trail_col, trail_int);
            }
        }

        // ---------------------------------------------------------------------
        // 3. PLAYER SPHERE (WITH ELASTIC SQUASH & STRETCH) & HOLOGRAPHIC SHIELD
        // ---------------------------------------------------------------------
        glEnable(GL_CULL_FACE);
        scene_shader.use();
        Mat4 model_ball;
        if (player.is_tumbling) {
            model_ball =
                Mat4::translate(player.tumble_pos) *
                Mat4::rotate_z(player.tumble_rot.z) *
                Mat4::rotate_x(player.tumble_rot.x) *
                Mat4::rotate_y(player.tumble_rot.y);
            scene_shader.set_inv_scale(1.0f, 1.0f, 1.0f);
        } else if (state == GameState::GARAGE && garage_tab == GarageTab::SKINS) {
            model_ball =
                Mat4::translate(0.0f, 1.05f + std::sin(elapsed_time * 2.2f) * 0.08f, 0.0f) *
                Mat4::rotate_y(elapsed_time * 1.15f) *
                Mat4::rotate_x(0.35f) *
                Mat4::scale(1.25f, 1.25f, 1.25f);
            scene_shader.set_inv_scale(1.0f, 1.0f, 1.0f);
        } else {
            model_ball =
                Mat4::translate(player.pos.x, player.pos.y, player.pos.z) *
                Mat4::scale(player.scale_xz, player.scale_y, player.scale_xz) *
                Mat4::rotate_z(player.tilt_angle + player.lateral_roll_angle) *
                Mat4::rotate_x(player.roll_angle);
            scene_shader.set_inv_scale(1.0f / player.scale_xz, 1.0f / player.scale_y, 1.0f / player.scale_xz);
        }

        scene_shader.set_model(model_ball);
        scene_shader.set_mvp(vp * model_ball);
        scene_shader.set_shading_mode(3);
        scene_shader.set_energy_color(skin.energy_color.r, skin.energy_color.g, skin.energy_color.b);

        if (state == GameState::GAME_OVER) {
            scene_shader.set_color(1.0f, 0.25f, 0.25f, 1.0f);
            scene_shader.set_emission(0.95f);
        } else if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
            scene_shader.set_color(1.0f, 0.92f, 0.35f, 1.0f);
            scene_shader.set_emission(0.85f);
        } else {
            scene_shader.set_color(skin.core_color.r, skin.core_color.g, skin.core_color.b, 1.0f);
            scene_shader.set_emission(treadmill.is_fever_active() ? 0.65f : 0.35f);
        }
        sphere_mesh.draw();
        scene_shader.set_inv_scale(1.0f, 1.0f, 1.0f);

        // Fresnel Holographic Shield / Overdrive Shell
        if (!player.is_tumbling && show_obstacles &&
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
            scene_shader.set_model(model_shield);
            scene_shader.set_mvp(vp * model_shield);
            scene_shader.set_shading_mode(4);
            if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
                scene_shader.set_color(1.0f, 0.25f, 0.85f, 0.88f);
            } else {
                scene_shader.set_color(0.05f, 0.95f, 1.0f, 0.82f);
            }
            scene_shader.set_emission(1.45f);
            sphere_mesh.draw();

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // ---------------------------------------------------------------------
        // 4. BATCHED PARTICLES (1 SINGLE DRAW CALL!)
        // ---------------------------------------------------------------------
        glDisable(GL_CULL_FACE);
        particles.draw(particle_shader, vp);

        // ---------------------------------------------------------------------
        // 5. GLASSMORPHIC UI SCREENS & ARCADE HUD
        // ---------------------------------------------------------------------
        const UiLayout L = UiLayout::compute(view_width, view_height);
        const Mat4 ortho = Mat4::ortho(0.0f, L.vw, L.vh, 0.0f);
        const float s = L.s;
        const float cw = 9.5f * s;
        const float ch = 15.0f * s;
        const float pad = 16.0f * s;

        HudRenderer::begin_hud(ui_shader);

        // Impact red flash
        if (state == GameState::GAME_OVER && death_shake_timer > 0.0f) {
            float flash_a = 0.42f * std::clamp(death_shake_timer / 0.65f, 0.0f, 1.0f);
            hud.draw_box(ui_shader, ortho, 0.0f, 0.0f, L.vw, L.vh, 1.0f, 0.08f, 0.14f, flash_a);
        }

        // Route to active screen renderer
        switch (state) {
            case GameState::HOME:
                render_home_screen(L, ortho, treadmill, save, biome, cw, ch, pad);
                break;
            case GameState::GARAGE:
                render_garage_screen(L, ortho, save, garage_tab, garage_skin_idx, cw, ch, pad);
                break;
            case GameState::MISSIONS:
                render_missions_screen(L, ortho, save, cw, ch, pad);
                break;
            case GameState::SETTINGS:
                render_settings_screen(L, ortho, save, cw, ch, pad);
                break;
            case GameState::COUNTDOWN:
            case GameState::PLAYING:
            case GameState::PAUSED:
            case GameState::GAME_OVER:
                render_gameplay_hud(L, ortho, treadmill, save, biome, state, countdown_timer, countdown_value, cw, ch, pad);
                break;
        }

        // Optional Live FPS Readout
        if (save.show_fps != 0) {
            char fps_buf[24];
            std::snprintf(fps_buf, sizeof(fps_buf), "%d FPS", static_cast<int>(fps + 0.5f));
            float fw = HudRenderer::measure_text_width(fps_buf, cw * 0.72f);
            hud.draw_text(ui_shader, ortho, L.vw - fw - pad, L.vh - 22.0f * s,
                          cw * 0.72f, ch * 0.72f, fps_buf, 0.35f, 1.0f, 0.55f, 0.85f);
        }

        HudRenderer::end_hud();
    }

    void destroy() {
        particles.destroy();
        trail.destroy();
        hud.destroy();
        sky_quad.destroy();
        sphere_mesh.destroy();
        crystal_mesh.destroy();
        box_mesh.destroy();
        rail_mesh.destroy();
        track_mesh.destroy();
        scene_shader.destroy();
        sky_shader.destroy();
        particle_shader.destroy();
        ui_shader.destroy();
    }

private:
    void render_home_screen(const UiLayout& L, const Mat4& ortho,
                            const TreadmillSystem& treadmill, const SaveData& save,
                            const BiomePalette& biome, float cw, float ch, float pad) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;

        // Top Profile Glass Bar (Pilot Rank | Best | Orbs)
        Rect2D top_bar = {pad, 12.0f * s, L.vw - pad * 2.0f, 46.0f * s};
        hud.draw_panel(ui_shader, ortho, top_bar, 12.0f * s, 1.4f,
                       0.05f, 0.09f, 0.20f, 0.84f,
                       0.02f, 0.04f, 0.10f, 0.90f,
                       0.05f, 0.88f, 1.00f, 0.65f);

        char rank_buf[32];
        std::snprintf(rank_buf, sizeof(rank_buf), "LV.%d %s", save.get_pilot_rank(), save.get_pilot_title());
        hud.draw_text(ui_shader, ortho, top_bar.x + 12.0f * s, top_bar.y + 15.0f * s,
                      cw * 0.78f, ch * 0.78f, rank_buf, 0.20f, 0.96f, 1.0f, 1.0f);

        char orb_buf[32];
        std::snprintf(orb_buf, sizeof(orb_buf), "ORBS %d", save.total_orbs);
        float ow = HudRenderer::measure_text_width(orb_buf, cw * 0.80f);
        hud.draw_text(ui_shader, ortho, top_bar.x + top_bar.w - ow - 12.0f * s, top_bar.y + 15.0f * s,
                      cw * 0.80f, ch * 0.80f, orb_buf, 1.0f, 0.86f, 0.18f, 1.0f);

        // Hero Neon Title Block
        hud.draw_text_shadow(ui_shader, ortho, cx, 82.0f * s, cw * 1.95f, ch * 1.95f,
                             "VELOSPHERE", 0.05f, 0.96f, 1.0f, 1.0f);
        hud.draw_text_shadow(ui_shader, ortho, cx, 118.0f * s, cw * 0.82f, ch * 0.82f,
                             "HYPERFLOW EDITION", 1.0f, 0.30f, 0.86f, 0.95f);

        // Best Score Pill under title
        char best_buf[40];
        std::snprintf(best_buf, sizeof(best_buf), "BEST: %06d", save.get_best_for_mode(treadmill.get_game_mode()));
        hud.draw_text_centered(ui_shader, ortho, cx, 146.0f * s, cw * 0.88f, ch * 0.88f,
                               best_buf, 1.0f, 0.88f, 0.22f, 0.96f);

        // Active Mission Mini Ticker above Mode button
        char m_title[48];
        save.missions[0].format_title(m_title, sizeof(m_title));
        char m_line[64];
        std::snprintf(m_line, sizeof(m_line), "MISSION: %s (%d/%d)",
                      m_title, save.missions[0].progress, save.missions[0].target);
        hud.draw_text_centered(ui_shader, ortho, cx, L.home_mode_btn.y - 24.0f * s,
                               cw * 0.66f, ch * 0.66f, m_line, 0.78f, 0.92f, 1.0f, 0.88f);

        // Mode Selector Pill
        const char* mode_lbl =
            (treadmill.get_game_mode() == GameMode::TIME_ATTACK) ? "< MODE: TIME ATTACK 90S >" :
            (treadmill.get_game_mode() == GameMode::HARDCORE)    ? "< MODE: HARDCORE (2X) >" :
                                                                   "< MODE: ENDLESS ARCADE >";
        hud.draw_button(ui_shader, ortho, L.home_mode_btn, mode_lbl,
                        cw * 0.80f, ch * 0.80f, 12.0f * s, biome.grid_color, false);

        // Glowing Primary PLAY CTA Button
        float pulse = 0.88f + 0.12f * std::sin(elapsed_time * 4.5f);
        hud.draw_button(ui_shader, ortho, L.home_play_btn, "LAUNCH RUN",
                        cw * 1.22f, ch * 1.22f, 16.0f * s,
                        {0.05f * pulse, 0.95f * pulse, 1.0f * pulse}, true);

        // Bottom Navigation Dock (GARAGE | MISSIONS | SETTINGS)
        hud.draw_button(ui_shader, ortho, L.home_garage_btn,   "GARAGE",   cw * 0.78f, ch * 0.78f, 12.0f * s, {1.0f, 0.75f, 0.15f}, false);
        hud.draw_button(ui_shader, ortho, L.home_missions_btn, "MISSIONS", cw * 0.78f, ch * 0.78f, 12.0f * s, {0.25f, 1.0f, 0.55f}, false);
        hud.draw_button(ui_shader, ortho, L.home_settings_btn, "SETTINGS", cw * 0.78f, ch * 0.78f, 12.0f * s, {0.75f, 0.50f, 1.0f}, false);
    }

    void render_garage_screen(const UiLayout& L, const Mat4& ortho,
                              const SaveData& save, GarageTab tab, int preview_skin,
                              float cw, float ch, float pad) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;

        // Top Header + Back + Orbs
        hud.draw_button(ui_shader, ortho, L.back_btn, "< BACK", cw * 0.78f, ch * 0.78f, 10.0f * s, {0.1f, 0.92f, 1.0f}, false);
        hud.draw_text_shadow(ui_shader, ortho, cx, 22.0f * s, cw * 1.15f, ch * 1.15f, "GARAGE", 1.0f, 0.85f, 0.2f, 1.0f);

        char orb_buf[24];
        std::snprintf(orb_buf, sizeof(orb_buf), "%d ORBS", save.total_orbs);
        float ow = HudRenderer::measure_text_width(orb_buf, cw * 0.80f);
        hud.draw_text(ui_shader, ortho, L.vw - ow - pad, 24.0f * s, cw * 0.80f, ch * 0.80f, orb_buf, 1.0f, 0.86f, 0.18f, 1.0f);

        // Tabs: SKINS | UPGRADES
        hud.draw_button(ui_shader, ortho, L.garage_tab_skins, "SKINS",
                        cw * 0.85f, ch * 0.85f, 10.0f * s, {0.05f, 0.95f, 1.0f}, tab == GarageTab::SKINS);
        hud.draw_button(ui_shader, ortho, L.garage_tab_upgrades, "TECH UPGRADES",
                        cw * 0.80f, ch * 0.80f, 10.0f * s, {1.0f, 0.30f, 0.85f}, tab == GarageTab::UPGRADES);

        if (tab == GarageTab::SKINS) {
            int idx = std::clamp(preview_skin, 0, static_cast<int>(SKIN_PALETTES.size()) - 1);
            const auto& sk = SKIN_PALETTES[static_cast<size_t>(idx)];
            bool unlocked = save.is_skin_unlocked(idx);
            bool equipped = (save.selected_skin == idx);

            hud.draw_button(ui_shader, ortho, L.garage_prev_skin, "<", cw * 1.2f, ch * 1.2f, 12.0f * s, sk.core_color, false);
            hud.draw_button(ui_shader, ortho, L.garage_next_skin, ">", cw * 1.2f, ch * 1.2f, 12.0f * s, sk.core_color, false);

            // Skin name + index pill between arrows
            Rect2D name_box = {L.garage_prev_skin.x + L.garage_prev_skin.w + 8.0f * s,
                               L.garage_prev_skin.y,
                               L.garage_next_skin.x - (L.garage_prev_skin.x + L.garage_prev_skin.w) - 16.0f * s,
                               L.garage_prev_skin.h};
            hud.draw_panel(ui_shader, ortho, name_box, 12.0f * s, 1.5f,
                           0.05f, 0.08f, 0.18f, 0.88f,
                           0.02f, 0.03f, 0.09f, 0.92f,
                           sk.core_color.r, sk.core_color.g, sk.core_color.b, 0.90f);
            hud.draw_text_centered(ui_shader, ortho, cx, name_box.y + 15.0f * s,
                                   cw * 0.86f, ch * 0.86f, sk.name,
                                   sk.core_color.r, sk.core_color.g, sk.core_color.b, 1.0f);

            // Action CTA: EQUIPPED / EQUIP / UNLOCK (COST ORBS)
            char act_lbl[40];
            Color3 act_col = {0.1f, 0.95f, 0.6f};
            bool filled = true;
            if (equipped) {
                std::snprintf(act_lbl, sizeof(act_lbl), "EQUIPPED");
                act_col = {0.35f, 0.95f, 0.65f};
                filled = false;
            } else if (unlocked) {
                std::snprintf(act_lbl, sizeof(act_lbl), "EQUIP SKIN");
                act_col = {0.05f, 0.95f, 1.0f};
            } else if (save.total_orbs >= sk.unlock_cost) {
                std::snprintf(act_lbl, sizeof(act_lbl), "UNLOCK - %d ORBS", sk.unlock_cost);
                act_col = {1.0f, 0.82f, 0.15f};
            } else {
                std::snprintf(act_lbl, sizeof(act_lbl), "NEED %d ORBS", sk.unlock_cost);
                act_col = {0.55f, 0.55f, 0.68f};
                filled = false;
            }
            hud.draw_button(ui_shader, ortho, L.garage_action_skin, act_lbl,
                            cw * 0.95f, ch * 0.95f, 14.0f * s, act_col, filled);
        } else {
            // 4 Permanent Tech Upgrade Cards
            for (int i = 0; i < 4; ++i) {
                UpgradeType ut = static_cast<UpgradeType>(i);
                const auto& info = UPGRADE_CATALOG[static_cast<size_t>(i)];
                int lv = save.get_upgrade_level(ut);
                int cost = save.get_next_upgrade_cost(ut);
                const Rect2D& r = L.garage_upgrade_btns[i];

                hud.draw_panel(ui_shader, ortho, r, 12.0f * s, 1.6f,
                               0.05f, 0.08f, 0.18f, 0.90f,
                               0.02f, 0.03f, 0.09f, 0.94f,
                               info.accent.r, info.accent.g, info.accent.b, 0.80f);

                hud.draw_text(ui_shader, ortho, r.x + 12.0f * s, r.y + 10.0f * s,
                              cw * 0.82f, ch * 0.82f, info.name, info.accent.r, info.accent.g, info.accent.b, 1.0f);
                hud.draw_text(ui_shader, ortho, r.x + 12.0f * s, r.y + 30.0f * s,
                              cw * 0.66f, ch * 0.66f, info.short_desc, 0.80f, 0.88f, 1.0f, 0.85f);

                char cost_lbl[24];
                Color3 cc = {1.0f, 0.86f, 0.18f};
                if (lv >= MAX_UPGRADE_LEVEL) {
                    std::snprintf(cost_lbl, sizeof(cost_lbl), "MAXED");
                    cc = {0.25f, 1.0f, 0.55f};
                } else {
                    std::snprintf(cost_lbl, sizeof(cost_lbl), "LV%d -> %d ORBS", lv, cost);
                    if (save.total_orbs < cost) cc = {0.65f, 0.65f, 0.75f};
                }
                float cw_txt = HudRenderer::measure_text_width(cost_lbl, cw * 0.70f);
                hud.draw_text(ui_shader, ortho, r.x + r.w - cw_txt - 12.0f * s, r.y + 12.0f * s,
                              cw * 0.70f, ch * 0.70f, cost_lbl, cc.r, cc.g, cc.b, 1.0f);

                Rect2D bar = {r.x + 12.0f * s, r.y + r.h - 18.0f * s, r.w - 24.0f * s, 9.0f * s};
                hud.draw_progress_bar(ui_shader, ortho, bar,
                                      static_cast<float>(lv) / static_cast<float>(MAX_UPGRADE_LEVEL),
                                      4.0f * s, info.accent);
            }
        }
    }

    void render_missions_screen(const UiLayout& L, const Mat4& ortho,
                                const SaveData& save, float cw, float ch, float pad) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;
        const float card_w = std::min(L.vw - pad * 2.0f, 360.0f * s);
        const float card_x = cx - card_w * 0.5f;

        hud.draw_button(ui_shader, ortho, L.back_btn, "< BACK", cw * 0.78f, ch * 0.78f, 10.0f * s, {0.1f, 0.92f, 1.0f}, false);
        hud.draw_text_shadow(ui_shader, ortho, cx, 22.0f * s, cw * 1.10f, ch * 1.10f, "MISSIONS", 0.25f, 1.0f, 0.55f, 1.0f);

        // 3 Active Mission Cards
        float my = 72.0f * s;
        const float mh = 72.0f * s;
        for (int i = 0; i < 3; ++i) {
            const MissionSlot& m = save.missions[i];
            Rect2D r = {card_x, my + static_cast<float>(i) * (mh + 12.0f * s), card_w, mh};
            hud.draw_panel(ui_shader, ortho, r, 12.0f * s, 1.5f,
                           0.05f, 0.09f, 0.19f, 0.90f,
                           0.02f, 0.04f, 0.10f, 0.94f,
                           0.20f, 0.95f, 0.60f, 0.78f);

            char title[48];
            m.format_title(title, sizeof(title));
            hud.draw_text(ui_shader, ortho, r.x + 12.0f * s, r.y + 10.0f * s,
                          cw * 0.76f, ch * 0.76f, title, 0.95f, 0.98f, 1.0f, 1.0f);

            char rew[24];
            std::snprintf(rew, sizeof(rew), "+%d ORBS", static_cast<int>(m.reward_orbs));
            float rw = HudRenderer::measure_text_width(rew, cw * 0.72f);
            hud.draw_text(ui_shader, ortho, r.x + r.w - rw - 12.0f * s, r.y + 10.0f * s,
                          cw * 0.72f, ch * 0.72f, rew, 1.0f, 0.86f, 0.18f, 1.0f);

            char prog[32];
            std::snprintf(prog, sizeof(prog), "%d / %d", m.progress, m.target);
            hud.draw_text(ui_shader, ortho, r.x + 12.0f * s, r.y + 30.0f * s,
                          cw * 0.66f, ch * 0.66f, prog, 0.35f, 1.0f, 0.70f, 0.92f);

            Rect2D bar = {r.x + 12.0f * s, r.y + r.h - 17.0f * s, r.w - 24.0f * s, 8.0f * s};
            hud.draw_progress_bar(ui_shader, ortho, bar, m.progress_ratio(), 4.0f * s, {0.20f, 1.0f, 0.55f});
        }

        // Career Stats Glass Card
        float sy = my + 3.0f * (mh + 12.0f * s) + 10.0f * s;
        Rect2D stats_r = {card_x, sy, card_w, 168.0f * s};
        hud.draw_panel(ui_shader, ortho, stats_r, 14.0f * s, 1.5f,
                       0.04f, 0.07f, 0.16f, 0.90f,
                       0.01f, 0.03f, 0.08f, 0.94f,
                       0.05f, 0.85f, 1.00f, 0.70f);

        hud.draw_text_centered(ui_shader, ortho, cx, stats_r.y + 12.0f * s,
                               cw * 0.88f, ch * 0.88f, "PILOT CAREER STATS", 0.05f, 0.95f, 1.0f, 1.0f);

        char line[64];
        std::snprintf(line, sizeof(line), "RANK: LV.%d %s  (DONE: %d)",
                      save.get_pilot_rank(), save.get_pilot_title(), save.missions_completed);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 42.0f * s, cw * 0.70f, ch * 0.70f, line, 1.0f, 0.86f, 0.20f, 1.0f);

        std::snprintf(line, sizeof(line), "RUNS: %d   DODGES: %d", save.total_runs, save.total_dodges);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 66.0f * s, cw * 0.70f, ch * 0.70f, line, 0.85f, 0.94f, 1.0f, 0.95f);

        std::snprintf(line, sizeof(line), "NEAR MISSES: %d   MAX COMBO: X%d", save.total_near_misses, save.best_combo);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 90.0f * s, cw * 0.70f, ch * 0.70f, line, 0.85f, 0.94f, 1.0f, 0.95f);

        std::snprintf(line, sizeof(line), "DISTANCE: %dM   ORBS: %d", save.total_distance_m, save.total_orbs);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 114.0f * s, cw * 0.70f, ch * 0.70f, line, 0.35f, 1.0f, 0.70f, 0.95f);

        std::snprintf(line, sizeof(line), "BEST ARC:%d TA:%d HC:%d",
                      save.high_score, save.best_score_time_attack, save.best_score_hardcore);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 138.0f * s, cw * 0.66f, ch * 0.66f, line, 1.0f, 0.45f, 0.85f, 0.95f);
    }

    void render_settings_screen(const UiLayout& L, const Mat4& ortho,
                                const SaveData& save, float cw, float ch, float /*pad*/) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;

        hud.draw_button(ui_shader, ortho, L.back_btn, "< BACK", cw * 0.78f, ch * 0.78f, 10.0f * s, {0.1f, 0.92f, 1.0f}, false);
        hud.draw_text_shadow(ui_shader, ortho, cx, 22.0f * s, cw * 1.10f, ch * 1.10f, "SETTINGS", 0.78f, 0.52f, 1.0f, 1.0f);

        const char* qual_str = (save.graphics_quality == 0) ? "LOW (FAST)"
                             : (save.graphics_quality == 1) ? "MEDIUM" : "HIGH (ULTRA)";
        const char* ctrl_str = (save.control_scheme == 0) ? "SMOOTH DRAG" : "SWIPE LANES";
        const char* sens_str = (save.sensitivity == 0) ? "LOW"
                             : (save.sensitivity == 1) ? "MEDIUM" : "HIGH";

        const char* labels[7] = {
            "MUSIC",
            "SOUND FX",
            "HAPTICS",
            "CONTROLS",
            "SENSITIVITY",
            "GRAPHICS",
            "SHOW FPS"
        };
        const char* values[7] = {
            save.music_enabled   ? "ON" : "OFF",
            save.sfx_enabled     ? "ON" : "OFF",
            save.haptics_enabled ? "ON" : "OFF",
            ctrl_str,
            sens_str,
            qual_str,
            save.show_fps        ? "ON" : "OFF"
        };

        for (int i = 0; i < 7; ++i) {
            const Rect2D& r = L.settings_rows[i];
            bool active = (i == 0 && save.music_enabled) ||
                          (i == 1 && save.sfx_enabled) ||
                          (i == 2 && save.haptics_enabled) ||
                          (i == 6 && save.show_fps) ||
                          (i >= 3 && i <= 5);
            Color3 brd = active ? Color3{0.05f, 0.92f, 1.0f} : Color3{0.45f, 0.45f, 0.58f};

            hud.draw_panel(ui_shader, ortho, r, 12.0f * s, 1.5f,
                           0.05f, 0.08f, 0.18f, 0.90f,
                           0.02f, 0.04f, 0.10f, 0.94f,
                           brd.r, brd.g, brd.b, 0.78f);

            hud.draw_text(ui_shader, ortho, r.x + 16.0f * s, r.y + (r.h - ch * 0.84f) * 0.5f,
                          cw * 0.84f, ch * 0.84f, labels[i], 0.90f, 0.95f, 1.0f, 1.0f);

            float vw_txt = HudRenderer::measure_text_width(values[i], cw * 0.84f);
            hud.draw_text(ui_shader, ortho, r.x + r.w - vw_txt - 16.0f * s,
                          r.y + (r.h - ch * 0.84f) * 0.5f,
                          cw * 0.84f, ch * 0.84f, values[i], brd.r, brd.g, brd.b, 1.0f);
        }
    }

    void render_gameplay_hud(const UiLayout& L, const Mat4& ortho,
                             const TreadmillSystem& treadmill, const SaveData& save,
                             const BiomePalette& biome, GameState state,
                             float countdown_timer, int countdown_value,
                             float cw, float ch, float pad) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;

        // Sleek Top HUD Bar
        hud.draw_box(ui_shader, ortho, 0.0f, 0.0f, L.vw, 62.0f * s, 0.02f, 0.03f, 0.08f, 0.80f);
        hud.draw_box(ui_shader, ortho, 0.0f, 60.0f * s, L.vw, 2.0f * s,
                     biome.grid_color.r, biome.grid_color.g, biome.grid_color.b, 0.88f);

        // Top-Center Pause Button
        if (state == GameState::PLAYING || state == GameState::COUNTDOWN || state == GameState::PAUSED) {
            hud.draw_button(ui_shader, ortho, L.pause_btn,
                            (state == GameState::PAUSED) ? ">" : "II",
                            cw * 1.05f, ch * 1.05f, 10.0f * s, {0.05f, 0.95f, 1.0f}, false);
        }

        // Top-Left: Score & Multiplier/Time
        char score_str[32];
        std::snprintf(score_str, sizeof(score_str), "%06d", treadmill.get_score());
        hud.draw_text(ui_shader, ortho, pad, 10.0f * s, cw * 1.08f, ch * 1.08f, score_str, 0.05f, 0.96f, 1.0f, 1.0f);

        char sub_left[36];
        int total_mult = treadmill.get_combo() * treadmill.get_effective_multiplier();
        if (treadmill.get_game_mode() == GameMode::TIME_ATTACK) {
            std::snprintf(sub_left, sizeof(sub_left), "T:%04.1FS  X%d", treadmill.get_time_remaining(), total_mult);
        } else {
            std::snprintf(sub_left, sizeof(sub_left), "X%d  ORBS %d", total_mult, save.total_orbs + treadmill.get_run_orbs());
        }
        Color3 sub_col = treadmill.is_fever_active() ? Color3{1.0f, 0.35f, 0.92f} : Color3{0.35f, 1.0f, 0.65f};
        hud.draw_text(ui_shader, ortho, pad, 36.0f * s, cw * 0.78f, ch * 0.78f, sub_left, sub_col.r, sub_col.g, sub_col.b, 0.98f);

        // Top-Right: Best Score & Sector Name
        int mode_best = std::max(save.get_best_for_mode(treadmill.get_game_mode()), treadmill.get_score());
        char best_str[28];
        std::snprintf(best_str, sizeof(best_str), "BEST %06d", mode_best);
        float bw = HudRenderer::measure_text_width(best_str, cw * 0.88f);
        hud.draw_text(ui_shader, ortho, L.vw - bw - pad, 12.0f * s, cw * 0.88f, ch * 0.88f, best_str, 1.0f, 0.86f, 0.18f, 1.0f);

        float biome_w = HudRenderer::measure_text_width(biome.name, cw * 0.70f);
        hud.draw_text(ui_shader, ortho, L.vw - biome_w - pad, 36.0f * s, cw * 0.70f, ch * 0.70f,
                      biome.name, biome.grid_color.r, biome.grid_color.g, biome.grid_color.b, 0.92f);

        // Hyperflow / Fever Gauge Bar directly beneath HUD
        const float fbar_w = 150.0f * s;
        Rect2D fbar = {cx - fbar_w * 0.5f, 68.0f * s, fbar_w, 9.0f * s};
        Color3 fcol = treadmill.is_fever_active() ? Color3{1.0f, 0.25f, 0.92f} : Color3{0.10f, 0.92f, 1.0f};
        hud.draw_progress_bar(ui_shader, ortho, fbar, treadmill.get_fever_meter(), 4.5f * s, fcol);
        const char* flbl = treadmill.is_fever_active() ? "HYPERFLOW 2X" : "FLOW";
        hud.draw_text_centered(ui_shader, ortho, cx, fbar.y + 12.0f * s, cw * 0.62f, ch * 0.62f, flbl, fcol.r, fcol.g, fcol.b, 0.90f);

        // Active Power-Up Bar
        if (treadmill.get_active_powerup() != PowerUpType::NONE) {
            const char* pu_name =
                (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) ? "OVERDRIVE" :
                (treadmill.get_active_powerup() == PowerUpType::SLOW_MO)   ? "CHRONO SLOW-MO" : "ORB MAGNET";
            Rect2D pbar = {cx - 70.0f * s, 96.0f * s, 140.0f * s, 8.0f * s};
            hud.draw_progress_bar(ui_shader, ortho, pbar, treadmill.get_powerup_progress(), 4.0f * s, {1.0f, 0.35f, 0.90f});
            hud.draw_text_centered(ui_shader, ortho, cx, pbar.y + 11.0f * s, cw * 0.68f, ch * 0.68f, pu_name, 1.0f, 0.86f, 0.20f, 1.0f);
        }

        // Floating Alert Banner (Near Miss, Shield Smash, Hyperflow, Mission Complete)
        if (treadmill.get_banner_timer() > 0.0f) {
            float alpha = std::min(1.0f, treadmill.get_banner_timer() * 1.5f);
            hud.draw_text_shadow(ui_shader, ortho, cx, 130.0f * s, cw * 1.05f, ch * 1.05f,
                                 treadmill.get_banner_text(), 1.0f, 0.90f, 0.18f, alpha);
        } else if (treadmill.get_dodge_milestone_timer() > 0.0f) {
            char msg[32];
            std::snprintf(msg, sizeof(msg), "%d+ DODGES!", treadmill.get_dodge_milestone());
            float a = std::min(1.0f, treadmill.get_dodge_milestone_timer());
            hud.draw_text_shadow(ui_shader, ortho, cx, 130.0f * s, cw * 1.05f, ch * 1.05f, msg, 0.15f, 1.0f, 0.60f, a);
        }

        // Bottom-Left: Speedometer & Distance
        int mph = static_cast<int>(treadmill.get_global_speed() * 1.8f);
        char spd_str[36];
        std::snprintf(spd_str, sizeof(spd_str), "%d MPH  %dM", mph, static_cast<int>(treadmill.get_distance_traveled()));
        hud.draw_text(ui_shader, ortho, pad, L.vh - 38.0f * s, cw * 0.82f, ch * 0.82f, spd_str, 0.25f, 1.0f, 0.55f, 0.92f);

        // Bottom-Right: Shield Ready badge
        if (treadmill.is_shield_active()) {
            const char* sh = "[SHIELD]";
            float sw = HudRenderer::measure_text_width(sh, cw * 0.82f);
            hud.draw_text(ui_shader, ortho, L.vw - sw - pad, L.vh - 38.0f * s,
                          cw * 0.82f, ch * 0.82f, sh, 0.05f, 0.95f, 1.0f, 1.0f);
        }

        // State Overlays
        if (state == GameState::COUNTDOWN) {
            float pulse = 0.92f + 0.08f * std::sin((1.0f - std::max(0.0f, countdown_timer)) * 12.0f);
            char ctxt[8];
            if (countdown_value == 0) std::snprintf(ctxt, sizeof(ctxt), "GO!");
            else std::snprintf(ctxt, sizeof(ctxt), "%d", countdown_value);

            hud.draw_box(ui_shader, ortho, 0.0f, 0.0f, L.vw, L.vh, 0.005f, 0.01f, 0.03f, 0.28f);
            hud.draw_text_shadow(ui_shader, ortho, cx, L.vh * 0.38f, cw * 3.0f, ch * 3.0f, ctxt, 0.05f, 0.96f, 1.0f, pulse);
        } else if (state == GameState::PAUSED) {
            hud.draw_box(ui_shader, ortho, 0.0f, 0.0f, L.vw, L.vh, 0.005f, 0.01f, 0.03f, 0.55f);
            const float p_w = std::min(L.vw - pad * 2.0f, 330.0f * s);
            Rect2D panel = {cx - p_w * 0.5f, L.vh * 0.28f, p_w, 280.0f * s};
            hud.draw_panel(ui_shader, ortho, panel, 18.0f * s, 2.0f,
                           0.05f, 0.09f, 0.20f, 0.94f,
                           0.02f, 0.03f, 0.09f, 0.96f,
                           0.05f, 0.92f, 1.00f, 0.92f);

            hud.draw_text_shadow(ui_shader, ortho, cx, panel.y + 20.0f * s, cw * 1.30f, ch * 1.30f, "PAUSED", 0.05f, 0.95f, 1.0f, 1.0f);
            hud.draw_button(ui_shader, ortho, L.pause_resume_btn, "RESUME", cw * 0.95f, ch * 0.95f, 12.0f * s, {0.05f, 0.95f, 1.0f}, true);
            hud.draw_button(ui_shader, ortho, L.pause_restart_btn, "RESTART RUN", cw * 0.85f, ch * 0.85f, 12.0f * s, {1.0f, 0.80f, 0.20f}, false);

            char aud_lbl[32];
            std::snprintf(aud_lbl, sizeof(aud_lbl), "AUDIO: %s", (save.sfx_enabled || save.music_enabled) ? "ON" : "MUTED");
            hud.draw_button(ui_shader, ortho, L.pause_audio_btn, aud_lbl, cw * 0.82f, ch * 0.82f, 12.0f * s, {0.65f, 0.55f, 1.0f}, false);
            hud.draw_button(ui_shader, ortho, L.pause_home_btn, "HOME MENU", cw * 0.85f, ch * 0.85f, 12.0f * s, {1.0f, 0.40f, 0.65f}, false);
        } else if (state == GameState::GAME_OVER) {
            hud.draw_box(ui_shader, ortho, 0.0f, 0.0f, L.vw, L.vh, 0.04f, 0.005f, 0.015f, 0.52f);
            const float go_w = std::min(L.vw - pad * 2.0f, 350.0f * s);
            Rect2D panel = {cx - go_w * 0.5f, L.vh * 0.21f, go_w, 378.0f * s};
            hud.draw_panel(ui_shader, ortho, panel, 18.0f * s, 2.0f,
                           0.10f, 0.03f, 0.07f, 0.94f,
                           0.03f, 0.01f, 0.04f, 0.96f,
                           1.00f, 0.24f, 0.42f, 0.92f);

            const char* hdr = treadmill.is_time_expired() ? "TIME UP!" : "RUN COMPLETE";
            hud.draw_text_shadow(ui_shader, ortho, cx, panel.y + 18.0f * s, cw * 1.28f, ch * 1.28f, hdr, 1.0f, 0.30f, 0.45f, 1.0f);

            if (treadmill.has_new_best()) {
                hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 52.0f * s, cw * 0.96f, ch * 0.96f, "NEW RECORD!", 1.0f, 0.88f, 0.15f, 1.0f);
            }

            char buf[48];
            std::snprintf(buf, sizeof(buf), "SCORE   %06d", treadmill.get_score());
            hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 82.0f * s, cw * 1.02f, ch * 1.02f, buf, 1.0f, 1.0f, 1.0f, 1.0f);

            std::snprintf(buf, sizeof(buf), "BEST    %06d", mode_best);
            hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 112.0f * s, cw * 0.92f, ch * 0.92f, buf, 1.0f, 0.86f, 0.20f, 1.0f);

            std::snprintf(buf, sizeof(buf), "+%d ORBS   DIST %dM",
                          treadmill.get_run_orbs(), static_cast<int>(treadmill.get_distance_traveled()));
            hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 144.0f * s, cw * 0.78f, ch * 0.78f, buf, 0.30f, 1.0f, 0.68f, 0.95f);

            std::snprintf(buf, sizeof(buf), "NEAR MISS %d   MAX X%d",
                          treadmill.get_near_miss_count(), treadmill.get_best_run_combo());
            hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 170.0f * s, cw * 0.74f, ch * 0.74f, buf, 0.82f, 0.88f, 1.0f, 0.92f);

            // Optional Revive Button (once per run if player has >= 25 Orbs)
            if (treadmill.can_revive() && save.total_orbs >= SaveData::REVIVE_ORB_COST) {
                char rev_lbl[36];
                std::snprintf(rev_lbl, sizeof(rev_lbl), "REVIVE (%d ORBS)", SaveData::REVIVE_ORB_COST);
                hud.draw_button(ui_shader, ortho, L.over_revive_btn, rev_lbl, cw * 0.88f, ch * 0.88f, 12.0f * s, {1.0f, 0.82f, 0.15f}, true);
            } else {
                hud.draw_button(ui_shader, ortho, L.over_revive_btn, "REVIVE UNAVAILABLE", cw * 0.74f, ch * 0.74f, 12.0f * s, {0.42f, 0.42f, 0.52f}, false);
            }

            hud.draw_button(ui_shader, ortho, L.over_retry_btn, "PLAY AGAIN", cw * 1.02f, ch * 1.02f, 14.0f * s, {0.05f, 0.95f, 1.0f}, true);
            hud.draw_button(ui_shader, ortho, L.over_home_btn,  "HOME MENU",  cw * 0.84f, ch * 0.84f, 12.0f * s, {0.88f, 0.75f, 1.0f}, false);
        }
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

    GameState           game_state{GameState::HOME};
    GarageTab           garage_tab{GarageTab::SKINS};
    int                 garage_skin_idx{0};

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
    bool                horizontal_swipe_consumed{false};
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
            // Try v3 first
            SaveData loaded_v3{};
            if (fread(&loaded_v3, sizeof(SaveData), 1, f) == 1 && loaded_v3.is_valid()) {
                fclose(f);
                LOGI("Loaded verified v3 SaveData (Best: %d, Orbs: %d)", loaded_v3.high_score, loaded_v3.total_orbs);
                return loaded_v3;
            }
            // Fallback: migrate v2 SaveData seamlessly
            fseek(f, 0, SEEK_SET);
            SaveDataV2 loaded_v2{};
            if (fread(&loaded_v2, sizeof(SaveDataV2), 1, f) == 1 && loaded_v2.is_valid()) {
                fclose(f);
                LOGI("Migrated verified v2 SaveData to v3 (Best: %d, Orbs: %d)", loaded_v2.high_score, loaded_v2.total_orbs);
                return SaveData::migrate_from_v2(loaded_v2);
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
            }
            fclose(lf);
        }
    }
    return data;
}

// =============================================================================
// JNI AUDIO & HAPTICS HELPERS
// =============================================================================

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
        if (mid) env->CallStaticVoidMethod(cls, mid, sfx_type);
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
        if (mid) env->CallStaticVoidMethod(cls, mid, duration_ms, amplitude);
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

static void start_new_run(AppState* state, bool with_countdown) {
    state->treadmill.apply_upgrades(state->save_data);
    state->treadmill.reset();
    state->player.reset();
    state->renderer.trail.reset(state->player.pos);
    state->death_shake_timer = 0.0f;
    if (with_countdown) {
        state->countdown_timer = 0.75f;
        state->countdown_value = 3;
        state->game_state = GameState::COUNTDOWN;
        call_java_sfx(state->app, SFX_COUNTDOWN);
    } else {
        state->game_state = GameState::PLAYING;
    }
    state->timer.reset();
}

static void finalize_run(AppState* state) {
    int final_score = state->treadmill.get_score();
    state->save_data.total_orbs += state->treadmill.get_run_orbs();
    state->save_data.total_runs += 1;
    state->save_data.total_dodges += state->treadmill.get_dodge_count();
    state->save_data.total_near_misses += state->treadmill.get_near_miss_count();
    state->save_data.total_distance_m += static_cast<int>(state->treadmill.get_distance_traveled());
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
            if (app->window != nullptr) {
                if (state->egl.initialize(app->window)) {
                    state->renderer.init();
                    state->renderer.trail.reset(state->player.pos);
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
            if (state->game_state == GameState::PLAYING || state->game_state == GameState::COUNTDOWN) {
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
    const UiLayout L = UiLayout::compute(vw, vh);

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
                state->horizontal_swipe_consumed = false;

                switch (state->game_state) {
                    case GameState::HOME:
                        if (L.home_play_btn.contains(x, y)) {
                            call_java_sfx(state->app, SFX_UI_CLICK);
                            trigger_haptic(state->app, 25, 90);
                            start_new_run(state, true);
                        } else if (L.home_mode_btn.contains(x, y)) {
                            int next_mode = (static_cast<int>(state->treadmill.get_game_mode()) + 1) % 3;
                            state->save_data.selected_mode = next_mode;
                            state->treadmill.set_game_mode(static_cast<GameMode>(next_mode));
                            state->treadmill.set_best_score(state->save_data.get_best_for_mode(state->treadmill.get_game_mode()));
                            save_profile_data(state->app, state->save_data);
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        } else if (L.home_garage_btn.contains(x, y)) {
                            state->garage_skin_idx = state->save_data.selected_skin;
                            state->garage_tab = GarageTab::SKINS;
                            state->game_state = GameState::GARAGE;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        } else if (L.home_missions_btn.contains(x, y)) {
                            state->game_state = GameState::MISSIONS;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        } else if (L.home_settings_btn.contains(x, y)) {
                            state->game_state = GameState::SETTINGS;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        }
                        break;

                    case GameState::GARAGE:
                        if (L.back_btn.contains(x, y)) {
                            state->game_state = GameState::HOME;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        } else if (L.garage_tab_skins.contains(x, y)) {
                            state->garage_tab = GarageTab::SKINS;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        } else if (L.garage_tab_upgrades.contains(x, y)) {
                            state->garage_tab = GarageTab::UPGRADES;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        } else if (state->garage_tab == GarageTab::SKINS) {
                            const int count = static_cast<int>(SphereSkin::COUNT);
                            if (L.garage_prev_skin.contains(x, y)) {
                                state->garage_skin_idx = (state->garage_skin_idx + count - 1) % count;
                                call_java_sfx(state->app, SFX_UI_CLICK);
                            } else if (L.garage_next_skin.contains(x, y)) {
                                state->garage_skin_idx = (state->garage_skin_idx + 1) % count;
                                call_java_sfx(state->app, SFX_UI_CLICK);
                            } else if (L.garage_action_skin.contains(x, y)) {
                                if (state->save_data.is_skin_unlocked(state->garage_skin_idx)) {
                                    state->save_data.selected_skin = state->garage_skin_idx;
                                    save_profile_data(state->app, state->save_data);
                                    call_java_sfx(state->app, SFX_UI_CLICK);
                                } else if (state->save_data.try_unlock_skin(state->garage_skin_idx)) {
                                    state->save_data.selected_skin = state->garage_skin_idx;
                                    save_profile_data(state->app, state->save_data);
                                    call_java_sfx(state->app, SFX_UNLOCK);
                                    trigger_haptic(state->app, 55, 160);
                                }
                            }
                        } else {
                            for (int u = 0; u < 4; ++u) {
                                if (L.garage_upgrade_btns[u].contains(x, y)) {
                                    if (state->save_data.try_buy_upgrade(static_cast<UpgradeType>(u))) {
                                        state->treadmill.apply_upgrades(state->save_data);
                                        save_profile_data(state->app, state->save_data);
                                        call_java_sfx(state->app, SFX_UNLOCK);
                                        trigger_haptic(state->app, 55, 160);
                                    }
                                    break;
                                }
                            }
                        }
                        break;

                    case GameState::MISSIONS:
                        if (L.back_btn.contains(x, y)) {
                            state->game_state = GameState::HOME;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        }
                        break;

                    case GameState::SETTINGS:
                        if (L.back_btn.contains(x, y)) {
                            state->game_state = GameState::HOME;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        } else {
                            bool changed = false;
                            if (L.settings_rows[0].contains(x, y)) {
                                state->save_data.music_enabled = state->save_data.music_enabled ? 0 : 1;
                                changed = true;
                            } else if (L.settings_rows[1].contains(x, y)) {
                                state->save_data.sfx_enabled = state->save_data.sfx_enabled ? 0 : 1;
                                changed = true;
                            } else if (L.settings_rows[2].contains(x, y)) {
                                state->save_data.haptics_enabled = state->save_data.haptics_enabled ? 0 : 1;
                                changed = true;
                            } else if (L.settings_rows[3].contains(x, y)) {
                                state->save_data.control_scheme = (state->save_data.control_scheme + 1) % 2;
                                changed = true;
                            } else if (L.settings_rows[4].contains(x, y)) {
                                state->save_data.sensitivity = (state->save_data.sensitivity + 1) % 3;
                                changed = true;
                            } else if (L.settings_rows[5].contains(x, y)) {
                                state->save_data.graphics_quality = (state->save_data.graphics_quality + 1) % 3;
                                changed = true;
                            } else if (L.settings_rows[6].contains(x, y)) {
                                state->save_data.show_fps = state->save_data.show_fps ? 0 : 1;
                                changed = true;
                            }
                            if (changed) {
                                save_profile_data(state->app, state->save_data);
                                sync_java_settings(state->app, state->save_data);
                                call_java_sfx(state->app, SFX_UI_CLICK);
                            }
                        }
                        break;

                    case GameState::PLAYING:
                    case GameState::COUNTDOWN:
                        if (L.pause_btn.contains(x, y)) {
                            state->game_state = GameState::PAUSED;
                            state->timer.reset();
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        }
                        break;

                    case GameState::PAUSED:
                        if (L.pause_restart_btn.contains(x, y)) {
                            call_java_sfx(state->app, SFX_UI_CLICK);
                            start_new_run(state, false);
                        } else if (L.pause_audio_btn.contains(x, y)) {
                            uint8_t next = (state->save_data.sfx_enabled || state->save_data.music_enabled) ? 0 : 1;
                            state->save_data.sfx_enabled = next;
                            state->save_data.music_enabled = next;
                            save_profile_data(state->app, state->save_data);
                            sync_java_settings(state->app, state->save_data);
                        } else if (L.pause_home_btn.contains(x, y)) {
                            state->treadmill.reset();
                            state->player.reset();
                            state->game_state = GameState::HOME;
                            state->timer.reset();
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        } else if (L.pause_resume_btn.contains(x, y) || L.pause_btn.contains(x, y)) {
                            state->game_state = (state->countdown_value >= 0 && state->countdown_timer > 0.0f)
                                ? GameState::COUNTDOWN : GameState::PLAYING;
                            state->timer.reset();
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        }
                        break;

                    case GameState::GAME_OVER:
                        if (L.over_revive_btn.contains(x, y) &&
                            state->treadmill.can_revive() &&
                            state->save_data.try_spend_revive()) {
                            save_profile_data(state->app, state->save_data);
                            state->player.revive();
                            state->treadmill.clear_nearby_for_revive();
                            state->renderer.trail.reset(state->player.pos);
                            state->death_shake_timer = 0.0f;
                            state->game_state = GameState::PLAYING;
                            state->timer.reset();
                            call_java_sfx(state->app, SFX_POWERUP);
                            trigger_haptic(state->app, 75, 180);
                        } else if (L.over_retry_btn.contains(x, y)) {
                            call_java_sfx(state->app, SFX_UI_CLICK);
                            start_new_run(state, false);
                        } else if (L.over_home_btn.contains(x, y)) {
                            state->treadmill.reset();
                            state->player.reset();
                            state->death_shake_timer = 0.0f;
                            state->game_state = GameState::HOME;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        }
                        break;
                }
            } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                if (state->touch_active && vw > 0 && state->game_state == GameState::PLAYING) {
                    float dx = (x - state->last_touch_x) / static_cast<float>(vw);
                    float total_dx = x - state->touch_down_x;
                    float total_dy = y - state->touch_down_y;
                    float scale = hud_base_scale(vw);

                    if (static_cast<ControlScheme>(state->save_data.control_scheme) == ControlScheme::DRAG) {
                        state->player.apply_touch_delta(dx, state->save_data.get_touch_sensitivity_value());
                    } else if (!state->horizontal_swipe_consumed) {
                        if (total_dx > 34.0f * scale) {
                            state->player.steer_to_lane(1);
                            state->horizontal_swipe_consumed = true;
                            trigger_haptic(state->app, 12, 55);
                        } else if (total_dx < -34.0f * scale) {
                            state->player.steer_to_lane(-1);
                            state->horizontal_swipe_consumed = true;
                            trigger_haptic(state->app, 12, 55);
                        }
                    }

                    // Vertical swipe detection for Jump (with buffer!) / Fast-Drop Slam
                    if (!state->vertical_gesture_consumed) {
                        if (total_dy < -32.0f * scale) {
                            if (state->player.jump()) {
                                call_java_sfx(state->app, SFX_JUMP);
                                trigger_haptic(state->app, 22, 80);
                            }
                            state->vertical_gesture_consumed = true;
                        } else if (total_dy > 32.0f * scale) {
                            if (state->player.slam()) {
                                call_java_sfx(state->app, SFX_SLAM);
                                trigger_haptic(state->app, 35, 130);
                            }
                            state->vertical_gesture_consumed = true;
                        }
                    }

                    state->last_touch_x = x;
                    state->last_touch_y = y;
                }
            } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
                state->touch_active = false;
                state->vertical_gesture_consumed = false;
                state->horizontal_swipe_consumed = false;
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
    LOGI("   VeloSphere 3.0 - Hyperflow 3D Arcade Engine   ");
    LOGI("=================================================");

    AppState state{};
    state.app = app;
    app->userData = &state;
    app->onAppCmd = on_app_cmd;

    state.save_data = load_profile_data(app);
    state.garage_skin_idx = state.save_data.selected_skin;
    state.treadmill.set_game_mode(static_cast<GameMode>(std::clamp(state.save_data.selected_mode, 0, 2)));
    state.treadmill.apply_upgrades(state.save_data);
    state.treadmill.set_best_score(state.save_data.get_best_for_mode(state.treadmill.get_game_mode()));
    sync_java_settings(app, state.save_data);

    while (!app->destroyRequested) {
        int events = 0;
        struct android_poll_source* source = nullptr;

        while (ALooper_pollOnce(state.animating ? 0 : -1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested) break;
        }

        process_input_events(&state);

        if (state.animating && state.egl.is_ready) {
            const float dt = state.timer.tick();
            state.renderer.elapsed_time += dt;

            if (state.game_state == GameState::COUNTDOWN) {
                state.countdown_timer -= dt;
                if (state.countdown_timer <= 0.0f) {
                    if (state.countdown_value > 0) {
                        --state.countdown_value;
                        state.countdown_timer = (state.countdown_value == 0) ? 0.45f : 0.70f;
                        call_java_sfx(state.app, SFX_COUNTDOWN);
                    } else {
                        state.game_state = GameState::PLAYING;
                        state.countdown_timer = 0.0f;
                    }
                }
            } else if (state.game_state == GameState::PLAYING) {
                const float prev_milestone = state.treadmill.get_dodge_milestone_timer();
                const FrameEvents fev = state.treadmill.update(dt, state.player.pos);

                if (state.treadmill.get_dodge_milestone_timer() > prev_milestone) {
                    trigger_haptic(state.app, 60, 110);
                }
                if (fev.near_miss_triggered) {
                    call_java_sfx(state.app, SFX_NEAR_MISS);
                    trigger_haptic(state.app, 28, 100);
                }
                if (fev.fever_started) {
                    call_java_sfx(state.app, SFX_FEVER);
                    trigger_haptic(state.app, 90, 200);
                }

                state.player.update(dt, state.treadmill.get_global_speed());
                state.renderer.trail.update(state.player.pos, state.treadmill.get_global_speed(), dt);

                if (state.player.just_landed) {
                    state.renderer.particles.spawn_orb_burst(
                        {state.player.pos.x, 0.12f, state.player.pos.z},
                        {0.15f, 0.92f, 1.0f});
                }

                if (std::abs(state.player.lateral_velocity) > 4.0f && !state.player.is_airborne) {
                    state.renderer.particles.spawn_trail_sparks(state.player.pos, state.player.lateral_velocity);
                }

                InteractionOutcome outcome = CollisionSystem::resolve_interactions(state.player, state.treadmill);

                int orbs_delta = outcome.orb_picked_up ? state.treadmill.get_mode_multiplier() : 0;
                int fevers_delta = (fev.fever_started || outcome.fever_started) ? 1 : 0;

                if (outcome.orb_picked_up) {
                    call_java_sfx(state.app, SFX_ORB);
                    trigger_haptic(state.app, 16, 60);
                    state.renderer.particles.spawn_orb_burst(state.player.pos);
                }
                if (outcome.fever_started) {
                    call_java_sfx(state.app, SFX_FEVER);
                    trigger_haptic(state.app, 90, 200);
                }
                if (outcome.powerup_picked_up) {
                    call_java_sfx(state.app, SFX_POWERUP);
                    trigger_haptic(state.app, 45, 145);
                    state.renderer.particles.spawn_orb_burst(state.player.pos, {0.1f, 0.95f, 1.0f});
                }
                if (outcome.obstacle_smashed || outcome.shield_broken) {
                    call_java_sfx(state.app, SFX_SHIELD_SMASH);
                    trigger_haptic(state.app, 90, 195);
                    state.death_shake_timer = 0.28f;
                    state.renderer.particles.spawn_shield_shatter(outcome.impact_pos);
                }

                // Live Mission Progress Check
                int mission_orbs = state.save_data.advance_missions(
                    orbs_delta,
                    fev.dodges_this_frame,
                    fev.near_miss_triggered ? 1 : 0,
                    fev.distance_delta_m,
                    state.treadmill.get_combo(),
                    fevers_delta
                );
                if (mission_orbs > 0) {
                    char mbuf[32];
                    std::snprintf(mbuf, sizeof(mbuf), "MISSION +%d ORBS!", mission_orbs);
                    state.treadmill.trigger_banner(mbuf, 1.6f);
                    call_java_sfx(state.app, SFX_MISSION_DONE);
                    trigger_haptic(state.app, 70, 170);
                    save_profile_data(state.app, state.save_data);
                }

                if (outcome.lethal_collision) {
                    call_java_sfx(state.app, SFX_HIT);
                    trigger_haptic(state.app, 140, 235);

                    if (state.treadmill.get_game_mode() == GameMode::TIME_ATTACK) {
                        state.treadmill.reset_combo();
                        state.treadmill.apply_time_penalty(10.0f);
                        state.treadmill.trigger_banner("-10S PENALTY!", 1.1f);
                        state.player.invulnerable_timer = 1.2f;
                        state.death_shake_timer = 0.42f;
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
                    call_java_sfx(state.app, SFX_HIT);
                    trigger_haptic(state.app, 120, 180);
                    state.game_state = GameState::GAME_OVER;
                    finalize_run(&state);
                }

                if (state.death_shake_timer > 0.0f) {
                    state.death_shake_timer = std::max(0.0f, state.death_shake_timer - dt);
                }
            } else if (state.game_state == GameState::HOME ||
                       state.game_state == GameState::GARAGE ||
                       state.game_state == GameState::MISSIONS ||
                       state.game_state == GameState::SETTINGS) {
                state.player.update(dt, 7.5f);
                state.renderer.trail.update(state.player.pos, 7.5f, dt);
            } else if (state.game_state == GameState::GAME_OVER) {
                state.player.update(dt, 0.0f);
                if (state.death_shake_timer > 0.0f) {
                    state.death_shake_timer = std::max(0.0f, state.death_shake_timer - dt);
                }
            }

            state.renderer.particles.update(dt);

            state.renderer.render_frame(
                state.player,
                state.treadmill,
                state.save_data,
                state.game_state,
                state.garage_tab,
                state.garage_skin_idx,
                state.egl.width,
                state.egl.height,
                state.death_shake_timer,
                state.countdown_timer,
                state.countdown_value,
                state.timer.get_fps()
            );

            state.egl.swap_buffers();
        }
    }

    save_profile_data(state.app, state.save_data);
    state.renderer.destroy();
    state.egl.terminate();
    LOGI("VeloSphere 3.0 Engine Terminated Cleanly.");
}