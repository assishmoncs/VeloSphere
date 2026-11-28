/**
 * VeloSphere 4.0 — Apex Edition (Curved-World 3D Synthwave Arcade Engine)
 * OpenGL ES 3.0 | GameActivity 4.4.2 | C++17 | Portrait-Locked | Zero-Lag Pipeline
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

enum SfxId : int {
    SFX_HIT          = 0,
    SFX_ORB          = 1,
    SFX_POWERUP      = 2,
    SFX_SHIELD_SMASH = 3,
    SFX_JUMP         = 4,
    SFX_SLAM         = 5,
    SFX_NEAR_MISS    = 6,
    SFX_FEVER        = 7,
    SFX_UI_CLICK     = 8,
    SFX_UNLOCK       = 9,
    SFX_MISSION_DONE = 10,
    SFX_COUNTDOWN    = 11,
    SFX_PHASE_DASH   = 12
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
        if (!window) return false;
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) return false;
        if (!eglInitialize(display, nullptr, nullptr)) return false;

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
        if (!eglChooseConfig(display, attribs, &config, 1, &num_configs) || num_configs <= 0) return false;

        EGLint format = 0;
        eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
        ANativeWindow_setBuffersGeometry(window, 0, 0, format);

        surface = eglCreateWindowSurface(display, config, window, nullptr);
        if (surface == EGL_NO_SURFACE) return false;

        const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
        if (context == EGL_NO_CONTEXT) return false;
        if (!eglMakeCurrent(display, surface, surface, context)) return false;

        eglSwapInterval(display, 1);
        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);

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
            if (context != EGL_NO_CONTEXT) { eglDestroyContext(display, context); context = EGL_NO_CONTEXT; }
            if (surface != EGL_NO_SURFACE) { eglDestroySurface(display, surface); surface = EGL_NO_SURFACE; }
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
    void reset() { last_time_ = Clock::now(); }

    float tick() {
        const auto now = Clock::now();
        const std::chrono::duration<float> elapsed = now - last_time_;
        last_time_ = now;
        float dt = std::min(0.05f, elapsed.count());
        if (dt > 0.0001f) {
            smoothed_fps_ = smoothed_fps_ * 0.90f + (1.0f / dt) * 0.10f;
        }
        return dt;
    }

    float get_fps() const { return smoothed_fps_; }

private:
    TimePoint last_time_;
    float smoothed_fps_{60.0f};
};

// =============================================================================
// PORTRAIT-OPTIMIZED RESPONSIVE UI LAYOUT (SHARED BY RENDERER & TOUCH)
// =============================================================================

static float hud_base_scale(int view_width, int view_height) {
    float min_dim = static_cast<float>(std::min(view_width, view_height));
    float s = min_dim / 420.0f;
    return std::clamp(s, 0.85f, 2.6f);
}

struct UiLayout {
    float vw{1080.0f};
    float vh{1920.0f};
    float s{2.0f};

    Rect2D back_btn{};
    Rect2D pause_btn{};

    // HOME Screen
    Rect2D home_mode_btn{};
    Rect2D home_play_btn{};
    Rect2D home_garage_btn{};
    Rect2D home_missions_btn{};
    Rect2D home_settings_btn{};
    Rect2D home_help_btn{};
    Rect2D home_about_btn{};

    // GARAGE Screen
    Rect2D garage_tab_skins{};
    Rect2D garage_tab_upgrades{};
    Rect2D garage_prev_skin{};
    Rect2D garage_next_skin{};
    Rect2D garage_action_skin{};
    Rect2D garage_upgrade_btns[4]{};

    // SETTINGS Screen (8 independent rows)
    Rect2D settings_rows[8]{};

    // PAUSE Overlay
    Rect2D pause_resume_btn{};
    Rect2D pause_restart_btn{};
    Rect2D pause_audio_btn{};
    Rect2D pause_home_btn{};

    // GAME OVER Overlay
    Rect2D over_revive_btn{};
    Rect2D over_retry_btn{};
    Rect2D over_home_btn{};

    static UiLayout compute(int width, int height) {
        UiLayout L{};
        L.vw = static_cast<float>(width);
        L.vh = static_cast<float>(height);
        L.s  = hud_base_scale(width, height);
        const float s = L.s;
        const float cx = L.vw * 0.5f;
        const float pad = 16.0f * s;

        L.back_btn  = {pad, 14.0f * s, 96.0f * s, 38.0f * s};
        L.pause_btn = {cx - 26.0f * s, 10.0f * s, 52.0f * s, 40.0f * s};

        const float card_w = std::min(L.vw - pad * 2.0f, 368.0f * s);
        const float card_x = cx - card_w * 0.5f;
        const float gap = 8.0f * s;

        // HOME Screen: clean bottom 2-row dock leaving the 3D animated world wide open!
        const float row2_h = 40.0f * s;
        const float row2_y = L.vh - row2_h - 14.0f * s;
        const float half_w = (card_w - gap) * 0.5f;
        L.home_help_btn  = {card_x, row2_y, half_w, row2_h};
        L.home_about_btn = {card_x + half_w + gap, row2_y, half_w, row2_h};

        const float row1_h = 48.0f * s;
        const float row1_y = row2_y - row1_h - gap;
        const float third_w = (card_w - gap * 2.0f) / 3.0f;
        L.home_garage_btn   = {card_x, row1_y, third_w, row1_h};
        L.home_missions_btn = {card_x + third_w + gap, row1_y, third_w, row1_h};
        L.home_settings_btn = {card_x + (third_w + gap) * 2.0f, row1_y, third_w, row1_h};

        L.home_play_btn = {card_x, row1_y - 64.0f * s, card_w, 54.0f * s};
        L.home_mode_btn = {card_x, L.home_play_btn.y - 46.0f * s, card_w, 38.0f * s};

        // GARAGE Screen
        const float tab_y = 64.0f * s;
        const float tab_w = (card_w - gap) * 0.5f;
        L.garage_tab_skins    = {card_x, tab_y, tab_w, 38.0f * s};
        L.garage_tab_upgrades = {card_x + tab_w + gap, tab_y, tab_w, 38.0f * s};

        const float skin_ctrl_y = L.vh - 146.0f * s;
        L.garage_prev_skin   = {card_x, skin_ctrl_y, 58.0f * s, 48.0f * s};
        L.garage_next_skin   = {card_x + card_w - 58.0f * s, skin_ctrl_y, 58.0f * s, 48.0f * s};
        L.garage_action_skin = {card_x, skin_ctrl_y + 60.0f * s, card_w, 50.0f * s};

        const float upg_start_y = tab_y + 56.0f * s;
        const float upg_h = 74.0f * s;
        for (int i = 0; i < 4; ++i) {
            L.garage_upgrade_btns[i] = {card_x, upg_start_y + static_cast<float>(i) * (upg_h + 12.0f * s), card_w, upg_h};
        }

        // SETTINGS Screen (8 stacked rows)
        const float set_start_y = 74.0f * s;
        const float set_h = 46.0f * s;
        for (int i = 0; i < 8; ++i) {
            L.settings_rows[i] = {card_x, set_start_y + static_cast<float>(i) * (set_h + 9.0f * s), card_w, set_h};
        }

        // PAUSE Overlay
        const float p_w = std::min(L.vw - pad * 2.0f, 330.0f * s);
        const float p_x = cx - p_w * 0.5f;
        const float p_y = L.vh * 0.27f;
        L.pause_resume_btn  = {p_x + 20.0f * s, p_y + 66.0f * s,  p_w - 40.0f * s, 44.0f * s};
        L.pause_restart_btn = {p_x + 20.0f * s, p_y + 120.0f * s, p_w - 40.0f * s, 40.0f * s};
        L.pause_audio_btn   = {p_x + 20.0f * s, p_y + 170.0f * s, p_w - 40.0f * s, 40.0f * s};
        L.pause_home_btn    = {p_x + 20.0f * s, p_y + 220.0f * s, p_w - 40.0f * s, 40.0f * s};

        // GAME OVER Overlay
        const float go_w = std::min(L.vw - pad * 2.0f, 350.0f * s);
        const float go_x = cx - go_w * 0.5f;
        const float go_y = L.vh * 0.20f;
        L.over_revive_btn = {go_x + 20.0f * s, go_y + 208.0f * s, go_w - 40.0f * s, 44.0f * s};
        L.over_retry_btn  = {go_x + 20.0f * s, go_y + 262.0f * s, go_w - 40.0f * s, 48.0f * s};
        L.over_home_btn   = {go_x + 20.0f * s, go_y + 320.0f * s, go_w - 40.0f * s, 40.0f * s};

        return L;
    }
};

// =============================================================================
// GRAPHICS RENDERER (CURVED-WORLD 3D ARTICULATED ENGINE + 9 SCREENS)
// =============================================================================

class GameRenderer {
public:
    SceneShader     scene_shader;
    SkyShader       sky_shader;
    ParticleShader  particle_shader;
    UiShader        ui_shader;

    SphereMesh      sphere_mesh;
    TorusMesh       torus_mesh;
    CrystalMesh     crystal_mesh;
    BoxMesh         box_mesh;
    BoxMesh         rail_mesh;
    TrackMesh       track_mesh;
    QuadMesh        sky_quad;
    TrailRibbon     trail;
    ShockwaveSystem shockwaves;
    HudRenderer     hud;
    ParticleSystem  particles;

    float           elapsed_time{0.0f};

    bool init() {
        if (!scene_shader.build() || !sky_shader.build() ||
            !particle_shader.build() || !ui_shader.build()) {
            LOGE("Failed to compile one or more VeloSphere 4.0 shaders");
            return false;
        }

        sphere_mesh.init(PlayerBall::RADIUS, 28, 18);
        torus_mesh.init(1.0f, 0.085f, 28, 10);
        crystal_mesh.init(0.36f, 1.32f);
        box_mesh.init(1.0f, 1.0f, 1.0f);
        rail_mesh.init(0.24f, 0.40f, TreadmillSystem::SEGMENT_SPACING);
        track_mesh.init(PlayerBall::TRACK_HALF_WIDTH * 2.0f, TreadmillSystem::SEGMENT_SPACING, 8);
        sky_quad.init(2.0f, 2.0f);
        trail.init();
        hud.init();
        particles.init();
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

        // Disable curve on Garage turntable so the 3D showcase stays centered & crisp
        const bool curve_on = (save.world_curve_enabled != 0) &&
                              !(state == GameState::GARAGE && garage_tab == GarageTab::SKINS);
        const Vec2 curve = treadmill.get_world_curve(curve_on);

        Color3 sky_fog = (state == GameState::GAME_OVER) ? Color3{0.11f, 0.02f, 0.04f} : biome.sky_fog;
        glClearColor(sky_fog.r, sky_fog.g, sky_fog.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 0. PROCEDURAL SYNTHWAVE HORIZON SKYBOX (1 DRAW CALL)
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        sky_shader.use();
        sky_shader.set_params(sky_fog, biome.sun_top, biome.sun_bottom, biome.grid_color,
                              elapsed_time, player.pos.x,
                              (treadmill.is_fever_active() || player.is_phase_dashing()) ? 1.0f : 0.0f,
                              quality);
        sky_quad.draw();
        glDepthMask(GL_TRUE);

        // CAMERA (DYNAMIC SPRING-ROLL CHASE vs HOME SHOWCASE vs GARAGE TURNTABLE)
        float aspect = (view_height > 0) ? (static_cast<float>(view_width) / static_cast<float>(view_height)) : 0.56f;
        float speed = treadmill.get_global_speed();
        float speed_frac = (speed - TreadmillSystem::BASE_SPEED) / (TreadmillSystem::MAX_SPEED - TreadmillSystem::BASE_SPEED);
        float extra_fov = player.is_phase_dashing() ? 10.0f
                        : (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE ? 7.5f
                        : (treadmill.is_fever_active() ? 4.5f : 0.0f));
        float fov = 60.0f + std::clamp(speed_frac, 0.0f, 1.2f) * 10.0f + extra_fov;
        Mat4 projection = Mat4::perspective(to_radians(fov), aspect, 0.1f, 165.0f);

        float shake_x = 0.0f, shake_y = 0.0f;
        if (death_shake_timer > 0.0f) {
            shake_x = std::sin(death_shake_timer * 54.0f) * 0.24f * death_shake_timer;
            shake_y = std::cos(death_shake_timer * 44.0f) * 0.18f * death_shake_timer;
        }

        Vec3 eye{}, target{};
        Vec3 up{0.0f, 1.0f, 0.0f};
        if (state == GameState::GARAGE && garage_tab == GarageTab::SKINS) {
            eye    = {std::sin(elapsed_time * 0.55f) * 0.75f, 2.05f, -3.25f};
            target = {0.0f, 1.05f, 0.0f};
        } else if (state == GameState::HOME) {
            eye    = {std::sin(elapsed_time * 0.35f) * 0.90f, 3.15f, -4.65f};
            target = {0.0f, 1.25f, 14.0f};
        } else {
            eye    = {player.pos.x * 0.34f + shake_x, 3.75f + player.jump_y * 0.24f + shake_y, -5.5f};
            target = {player.pos.x * 0.18f, 1.20f + player.jump_y * 0.14f, 20.0f};
            up     = {std::sin(player.cam_roll_angle), std::cos(player.cam_roll_angle), 0.0f};
        }
        Mat4 vp = projection * Mat4::look_at(eye, target, up);

        // 1. RENDER CURVED RUNWAY, 3D CYBER-ARCHES, PYLONS & 6 ARTICULATED HAZARDS
        scene_shader.use();
        scene_shader.set_vp(vp);
        scene_shader.set_curve(curve.x, curve.y);
        scene_shader.set_sky_color(sky_fog.r, sky_fog.g, sky_fog.b);
        scene_shader.set_light_pos({2.5f, 12.0f, -2.5f});
        scene_shader.set_view_pos(eye);
        scene_shader.set_time(elapsed_time);
        scene_shader.set_inv_scale(1.0f, 1.0f, 1.0f);

        const auto& segments = treadmill.get_segments();
        const float spin = treadmill.get_orb_spin_angle();
        const bool show_obstacles = (state == GameState::PLAYING || state == GameState::PAUSED ||
                                     state == GameState::COUNTDOWN || state == GameState::GAME_OVER);

        // Subdivided curved runway floor
        glDisable(GL_CULL_FACE);
        scene_shader.set_shading_mode(1);
        scene_shader.set_color(biome.floor_base.r, biome.floor_base.g, biome.floor_base.b, 1.0f);
        scene_shader.set_energy_color(biome.grid_color.r, biome.grid_color.g, biome.grid_color.b);
        scene_shader.set_emission(0.58f);
        for (const auto& seg : segments) {
            scene_shader.set_model(Mat4::translate(0.0f, 0.0f, seg.z_position));
            track_mesh.draw();
        }

        // Closed 3D meshes -> back-face culling ON
        glEnable(GL_CULL_FACE);

        // Rails, lane dividers, 3D Cyber-Arches & Floating Roadside Pylons
        scene_shader.set_shading_mode(0);
        for (const auto& seg : segments) {
            scene_shader.set_color(biome.rail_color.r, biome.rail_color.g, biome.rail_color.b, 1.0f);
            scene_shader.set_emission(0.88f);
            scene_shader.set_model(Mat4::translate(-PlayerBall::TRACK_HALF_WIDTH, 0.20f, seg.z_position));
            rail_mesh.draw();
            scene_shader.set_model(Mat4::translate( PlayerBall::TRACK_HALF_WIDTH, 0.20f, seg.z_position));
            rail_mesh.draw();

            const float div_x = TreadmillSystem::LANE_WIDTH * 0.5f;
            scene_shader.set_color(biome.grid_color.r * 0.50f, biome.grid_color.g * 0.50f, biome.grid_color.b * 0.60f, 1.0f);
            scene_shader.set_emission(0.38f);
            scene_shader.set_model(Mat4::translate(-div_x, 0.018f, seg.z_position) * Mat4::scale(0.045f, 0.025f, TreadmillSystem::SEGMENT_SPACING * 0.90f));
            box_mesh.draw();
            scene_shader.set_model(Mat4::translate( div_x, 0.018f, seg.z_position) * Mat4::scale(0.045f, 0.025f, TreadmillSystem::SEGMENT_SPACING * 0.90f));
            box_mesh.draw();

            // 3D Roadside Cyber-Arches (creates intense 3D tunnel speed perception!)
            if (seg.has_arch && quality >= 1) {
                const float ax = PlayerBall::TRACK_HALF_WIDTH + 0.55f;
                const float ah = 5.4f;
                scene_shader.set_color(biome.grid_color.r, biome.grid_color.g, biome.grid_color.b, 1.0f);
                scene_shader.set_emission(0.75f);
                // Left & right angled cyber-columns
                scene_shader.set_model(Mat4::translate(-ax, ah * 0.5f, seg.z_position) * Mat4::scale(0.22f, ah, 0.26f));
                box_mesh.draw();
                scene_shader.set_model(Mat4::translate( ax, ah * 0.5f, seg.z_position) * Mat4::scale(0.22f, ah, 0.26f));
                box_mesh.draw();
                // Overhead neon arch beam
                scene_shader.set_color(biome.sun_bottom.r, biome.sun_bottom.g, biome.sun_bottom.b, 1.0f);
                scene_shader.set_emission(1.10f);
                scene_shader.set_model(Mat4::translate(0.0f, ah, seg.z_position) * Mat4::scale(ax * 2.05f, 0.22f, 0.28f));
                box_mesh.draw();
            } else if (quality >= 2) {
                // Floating spinning roadside energy pylons
                float py = 2.1f + std::sin(elapsed_time * 2.4f + seg.z_position * 0.15f) * 0.35f;
                scene_shader.set_color(biome.rail_color.r, biome.rail_color.g, biome.rail_color.b, 1.0f);
                scene_shader.set_emission(0.95f);
                scene_shader.set_model(Mat4::translate(-(PlayerBall::TRACK_HALF_WIDTH + 1.6f), py, seg.z_position) *
                                       Mat4::rotate_y(elapsed_time * 1.6f) * Mat4::scale(0.95f, 1.65f, 0.95f));
                crystal_mesh.draw();
                scene_shader.set_model(Mat4::translate( (PlayerBall::TRACK_HALF_WIDTH + 1.6f), py, seg.z_position) *
                                       Mat4::rotate_y(-elapsed_time * 1.6f) * Mat4::scale(0.95f, 1.65f, 0.95f));
                crystal_mesh.draw();
            }
        }

        // Collectibles & 6 Articulated 3D Animated Hazard Machines
        if (show_obstacles) {
            scene_shader.set_shading_mode(2);
            for (const auto& seg : segments) {
                // Data Orb Crystal
                if (seg.has_orb && !seg.orb_collected) {
                    float bob = std::sin(spin * 2.0f + seg.z_position * 0.2f) * 0.14f;
                    scene_shader.set_model(Mat4::translate(seg.orb_pos.x, seg.orb_pos.y + bob, seg.orb_pos.z) *
                                           Mat4::rotate_y(spin * 1.6f));
                    scene_shader.set_color(1.0f, 0.88f, 0.14f, 1.0f);
                    scene_shader.set_emission(1.20f);
                    crystal_mesh.draw();
                }

                // Power-Up Crystal + Orbiting Ring
                if (seg.powerup_type != PowerUpType::NONE && !seg.powerup_collected) {
                    float bob = std::cos(spin * 2.2f + seg.z_position * 0.3f) * 0.16f;
                    Color3 pc = {0.15f, 1.00f, 0.45f};
                    if (seg.powerup_type == PowerUpType::SHIELD)    pc = {0.05f, 0.95f, 1.00f};
                    if (seg.powerup_type == PowerUpType::OVERDRIVE) pc = {1.00f, 0.22f, 0.85f};
                    if (seg.powerup_type == PowerUpType::SLOW_MO)   pc = {0.62f, 0.34f, 1.00f};

                    scene_shader.set_color(pc.r, pc.g, pc.b, 1.0f);
                    scene_shader.set_emission(1.35f);
                    scene_shader.set_model(Mat4::translate(seg.powerup_pos.x, seg.powerup_pos.y + bob, seg.powerup_pos.z) *
                                           Mat4::rotate_y(spin * 1.8f) * Mat4::rotate_x(spin * 0.7f) * Mat4::scale(1.35f, 1.35f, 1.35f));
                    crystal_mesh.draw();
                    scene_shader.set_model(Mat4::translate(seg.powerup_pos.x, seg.powerup_pos.y + bob, seg.powerup_pos.z) *
                                           Mat4::rotate_x(0.75f) * Mat4::rotate_y(-spin * 2.2f) * Mat4::scale(0.68f, 0.68f, 0.68f));
                    torus_mesh.draw();
                }

                // Articulated 3D Hazards
                if (seg.has_obstacle) {
                    auto draw_cyber_pillar = [&](float ox) {
                        // Armored dark ruby pillar
                        scene_shader.set_color(0.95f, 0.10f, 0.34f, 1.0f);
                        scene_shader.set_emission(0.75f);
                        scene_shader.set_model(Mat4::translate(ox, seg.obstacle_height * 0.44f, seg.z_position) *
                                               Mat4::scale(seg.obstacle_width * 0.82f, seg.obstacle_height * 0.88f, seg.obstacle_depth * 0.82f));
                        box_mesh.draw();
                        // Pulsing energy collar ring
                        scene_shader.set_color(1.0f, 0.38f, 0.72f, 1.0f);
                        scene_shader.set_emission(1.35f);
                        scene_shader.set_model(Mat4::translate(ox, seg.obstacle_height * 0.55f, seg.z_position) *
                                               Mat4::rotate_y(seg.anim_angle) * Mat4::scale(seg.obstacle_width * 0.52f, 0.75f, seg.obstacle_width * 0.52f));
                        torus_mesh.draw();
                        // Floating spinning capstone crystal
                        float cap_bob = std::sin(seg.anim_angle * 1.5f) * 0.10f;
                        scene_shader.set_model(Mat4::translate(ox, seg.obstacle_height + 0.32f + cap_bob, seg.z_position) *
                                               Mat4::rotate_y(-seg.anim_angle * 1.4f) * Mat4::scale(1.1f, 1.1f, 1.1f));
                        crystal_mesh.draw();
                    };

                    if (seg.obstacle_type == ObstacleType::MOVING_SWEEPER) {
                        // Articulated Sweeper Drone (Core + Dual Counter-Rotating Rotor Rings!)
                        float pulse = (seg.motion_warning_time > 0.0f) ? (1.25f + 0.55f * std::sin(elapsed_time * 24.0f)) : 0.95f;
                        scene_shader.set_color(1.0f, 0.68f, 0.06f, 1.0f);
                        scene_shader.set_emission(pulse);
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset, seg.obstacle_height * 0.50f, seg.z_position) *
                                               Mat4::scale(seg.obstacle_width * 0.62f, seg.obstacle_height * 0.62f, seg.obstacle_depth * 0.62f));
                        box_mesh.draw();
                        scene_shader.set_color(1.0f, 0.90f, 0.25f, 1.0f);
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset, seg.obstacle_height * 0.50f, seg.z_position) *
                                               Mat4::rotate_y(seg.anim_angle * 2.2f) * Mat4::rotate_x(0.45f) *
                                               Mat4::scale(seg.obstacle_width * 0.54f, 0.85f, seg.obstacle_width * 0.54f));
                        torus_mesh.draw();
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset, seg.obstacle_height * 0.50f, seg.z_position) *
                                               Mat4::rotate_y(-seg.anim_angle * 2.6f) * Mat4::rotate_z(0.65f) *
                                               Mat4::scale(seg.obstacle_width * 0.44f, 0.85f, seg.obstacle_width * 0.44f));
                        torus_mesh.draw();
                    } else if (seg.obstacle_type == ObstacleType::LOW_BARRIER) {
                        // Twin Laser Emitters + Glowing Horizontal Plasma Beam
                        float ex = seg.obstacle_width * 0.45f;
                        scene_shader.set_color(0.55f, 0.12f, 0.95f, 1.0f);
                        scene_shader.set_emission(0.85f);
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset - ex, 0.42f, seg.z_position) * Mat4::scale(0.24f, 0.84f, 0.35f));
                        box_mesh.draw();
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset + ex, 0.42f, seg.z_position) * Mat4::scale(0.24f, 0.84f, 0.35f));
                        box_mesh.draw();
                        scene_shader.set_color(1.0f, 0.22f, 0.98f, 1.0f);
                        scene_shader.set_emission(1.45f);
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset, 0.52f, seg.z_position) *
                                               Mat4::scale(seg.obstacle_width * 0.92f, 0.18f, 0.18f));
                        box_mesh.draw();
                    } else if (seg.obstacle_type == ObstacleType::OVERHEAD_GATE) {
                        // Twin Hydraulic Pillars + Oscillating Overhead Barrier
                        float osc = std::sin(elapsed_time * 5.0f + seg.z_position) * 0.06f;
                        scene_shader.set_color(1.0f, 0.28f, 0.14f, 1.0f);
                        scene_shader.set_emission(0.95f);
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset, seg.hover_y + seg.obstacle_height * 0.5f + osc, seg.z_position) *
                                               Mat4::scale(seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth * 0.85f));
                        box_mesh.draw();
                        float px = seg.obstacle_width * 0.45f;
                        scene_shader.set_color(0.75f, 0.15f, 0.22f, 1.0f);
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset - px, seg.hover_y * 0.5f, seg.z_position) * Mat4::scale(0.18f, seg.hover_y, 0.35f));
                        box_mesh.draw();
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset + px, seg.hover_y * 0.5f, seg.z_position) * Mat4::scale(0.18f, seg.hover_y, 0.35f));
                        box_mesh.draw();
                    } else if (seg.obstacle_type == ObstacleType::LASER_SPINNER) {
                        // Central Armored Hub + Rapidly Spinning 3D Plasma Blade!
                        scene_shader.set_color(0.20f, 0.95f, 0.90f, 1.0f);
                        scene_shader.set_emission(1.10f);
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset, 0.45f, seg.z_position) * Mat4::scale(0.42f, 0.90f, 0.42f));
                        box_mesh.draw();
                        // Spinning cross-blade
                        scene_shader.set_color(1.0f, 0.18f, 0.55f, 1.0f);
                        scene_shader.set_emission(1.50f);
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset, 0.56f, seg.z_position) *
                                               Mat4::rotate_y(seg.anim_angle * 2.4f) *
                                               Mat4::scale(seg.obstacle_width, 0.18f, 0.22f));
                        box_mesh.draw();
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset, 0.56f, seg.z_position) *
                                               Mat4::rotate_y(seg.anim_angle * 2.4f + PI * 0.5f) *
                                               Mat4::scale(seg.obstacle_width * 0.72f, 0.18f, 0.22f));
                        box_mesh.draw();
                    } else if (seg.obstacle_type == ObstacleType::SPLIT_GATE) {
                        // Animated Sliding Twin Cyber-Walls!
                        scene_shader.set_color(0.12f, 0.85f, 1.0f, 1.0f);
                        scene_shader.set_emission(0.95f);
                        scene_shader.set_model(Mat4::translate(seg.obstacle_x_offset, seg.obstacle_height * 0.5f, seg.z_position) *
                                               Mat4::scale(seg.obstacle_width * 0.90f, seg.obstacle_height, seg.obstacle_depth * 0.70f));
                        box_mesh.draw();
                        scene_shader.set_model(Mat4::translate(seg.second_obstacle_x, seg.obstacle_height * 0.5f, seg.z_position) *
                                               Mat4::scale(seg.obstacle_width * 0.90f, seg.obstacle_height, seg.obstacle_depth * 0.70f));
                        box_mesh.draw();
                    } else {
                        draw_cyber_pillar(seg.obstacle_x_offset);
                        if (seg.has_second_obstacle) draw_cyber_pillar(seg.second_obstacle_x);
                    }
                }
            }
        }

        // 2. GROUND CONTACT SHADOW & 3D NEON TRAIL RIBBON
        glDisable(GL_CULL_FACE);
        int display_skin_idx = (state == GameState::GARAGE)
            ? std::clamp(garage_skin_idx, 0, static_cast<int>(SKIN_PALETTES.size()) - 1)
            : std::clamp(save.selected_skin, 0, static_cast<int>(SKIN_PALETTES.size()) - 1);
        const auto& skin = SKIN_PALETTES[static_cast<size_t>(display_skin_idx)];

        if (!player.is_tumbling) {
            trail.draw_contact_shadow(particle_shader, vp, curve, player.pos, player.jump_y);
            if (state == GameState::PLAYING || state == GameState::HOME) {
                Color3 trail_col = skin.energy_color;
                float trail_int = (quality == 0) ? 0.65f : 1.05f;
                if (player.is_phase_dashing()) {
                    trail_col = {0.15f, 1.0f, 0.95f};
                    trail_int = 1.60f;
                } else if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
                    trail_col = {1.0f, 0.25f, 0.90f};
                    trail_int = 1.40f;
                } else if (treadmill.is_fever_active()) {
                    float h = elapsed_time * 4.2f;
                    trail_col = {0.55f + 0.45f * std::sin(h),
                                 0.55f + 0.45f * std::sin(h + 2.09f),
                                 0.55f + 0.45f * std::sin(h + 4.18f)};
                    trail_int = 1.35f;
                }
                trail.draw_trail(particle_shader, vp, curve, trail_col, trail_int);
            }
        }

        // 3. ARTICULATED 3D APEX GYRO-CORE VESSEL (CORE + 2 COUNTER-ROTATING GYRO RINGS!)
        glEnable(GL_CULL_FACE);
        scene_shader.use();
        scene_shader.set_vp(vp);
        scene_shader.set_curve(curve.x, curve.y);

        Vec3 vessel_pos = player.is_tumbling ? player.tumble_pos : player.pos;
        float vessel_scale = 1.0f;
        if (state == GameState::GARAGE && garage_tab == GarageTab::SKINS) {
            vessel_pos = {0.0f, 1.05f + std::sin(elapsed_time * 2.2f) * 0.08f, 0.0f};
            vessel_scale = 1.25f;
        }

        Mat4 model_core;
        if (player.is_tumbling) {
            model_core = Mat4::translate(vessel_pos) *
                         Mat4::rotate_z(player.tumble_rot.z) *
                         Mat4::rotate_x(player.tumble_rot.x) *
                         Mat4::rotate_y(player.tumble_rot.y);
            scene_shader.set_inv_scale(1.0f, 1.0f, 1.0f);
        } else if (state == GameState::GARAGE && garage_tab == GarageTab::SKINS) {
            model_core = Mat4::translate(vessel_pos) *
                         Mat4::rotate_y(elapsed_time * 1.15f) *
                         Mat4::rotate_x(0.32f) *
                         Mat4::scale(vessel_scale, vessel_scale, vessel_scale);
            scene_shader.set_inv_scale(1.0f, 1.0f, 1.0f);
        } else {
            model_core = Mat4::translate(vessel_pos) *
                         Mat4::scale(player.scale_xz, player.scale_y, player.scale_xz) *
                         Mat4::rotate_z(player.tilt_angle + player.lateral_roll_angle) *
                         Mat4::rotate_x(player.roll_angle + player.jump_flip_angle);
            scene_shader.set_inv_scale(1.0f / player.scale_xz, 1.0f / player.scale_y, 1.0f / player.scale_xz);
        }

        scene_shader.set_model(model_core);
        scene_shader.set_shading_mode(3);
        scene_shader.set_energy_color(skin.energy_color.r, skin.energy_color.g, skin.energy_color.b);

        if (state == GameState::GAME_OVER) {
            scene_shader.set_color(1.0f, 0.25f, 0.25f, 1.0f);
            scene_shader.set_emission(0.95f);
        } else if (player.is_phase_dashing()) {
            scene_shader.set_color(0.25f, 1.0f, 0.98f, 1.0f);
            scene_shader.set_emission(1.30f);
        } else if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
            scene_shader.set_color(1.0f, 0.92f, 0.35f, 1.0f);
            scene_shader.set_emission(0.90f);
        } else {
            scene_shader.set_color(skin.core_color.r, skin.core_color.g, skin.core_color.b, 1.0f);
            scene_shader.set_emission(treadmill.is_fever_active() ? 0.72f : 0.38f);
        }
        sphere_mesh.draw();
        scene_shader.set_inv_scale(1.0f, 1.0f, 1.0f);

        // Hierarchical Child Gyroscopic Rings orbiting the Apex Core!
        if (!player.is_tumbling) {
            scene_shader.set_shading_mode(0);
            float r_outer = 0.92f * vessel_scale;
            float r_inner = 0.80f * vessel_scale;

            // Outer banking gyro ring
            scene_shader.set_color(skin.energy_color.r, skin.energy_color.g, skin.energy_color.b, 1.0f);
            scene_shader.set_emission(1.25f);
            Mat4 m_gyro1 = Mat4::translate(vessel_pos) *
                           Mat4::rotate_z(player.tilt_angle * 1.4f) *
                           Mat4::rotate_x(0.35f + player.jump_flip_angle * 0.5f) *
                           Mat4::rotate_y(player.gyro_outer_angle) *
                           Mat4::scale(r_outer, r_outer, r_outer);
            scene_shader.set_model(m_gyro1);
            torus_mesh.draw();

            // Inner counter-rotating stabilizer ring
            scene_shader.set_color(skin.core_color.r, skin.core_color.g, skin.core_color.b, 1.0f);
            scene_shader.set_emission(1.05f);
            Mat4 m_gyro2 = Mat4::translate(vessel_pos) *
                           Mat4::rotate_z(player.tilt_angle + 1.15f) *
                           Mat4::rotate_y(player.gyro_inner_angle) *
                           Mat4::scale(r_inner, r_inner, r_inner);
            scene_shader.set_model(m_gyro2);
            torus_mesh.draw();
        }

        // Fresnel Holographic Shield / Overdrive / Phase-Dash Shell
        if (!player.is_tumbling && show_obstacles &&
            (treadmill.is_shield_active() ||
             player.invulnerable_timer > 0.0f ||
             player.is_phase_dashing() ||
             treadmill.get_active_powerup() == PowerUpType::OVERDRIVE)) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDepthMask(GL_FALSE);

            scene_shader.set_model(Mat4::translate(vessel_pos) * Mat4::rotate_y(spin * 1.5f) * Mat4::scale(1.42f, 1.42f, 1.42f));
            scene_shader.set_shading_mode(4);
            if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
                scene_shader.set_color(1.0f, 0.25f, 0.85f, 0.88f);
            } else if (player.is_phase_dashing()) {
                scene_shader.set_color(0.20f, 1.0f, 0.90f, 0.92f);
            } else {
                scene_shader.set_color(0.05f, 0.95f, 1.0f, 0.82f);
            }
            scene_shader.set_emission(1.50f);
            sphere_mesh.draw();

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // 3D Expanding Shockwave Rings!
        shockwaves.draw(scene_shader, torus_mesh);

        // 4. BATCHED PARTICLES (1 SINGLE DRAW CALL)
        glDisable(GL_CULL_FACE);
        particles.draw(particle_shader, vp, curve);

        // 5. MODERN GLASSMORPHIC UI SCREENS & ARCADE HUD
        const UiLayout L = UiLayout::compute(view_width, view_height);
        const Mat4 ortho = Mat4::ortho(0.0f, L.vw, L.vh, 0.0f);
        const float s = L.s;
        const float cw = 9.5f * s;
        const float ch = 15.0f * s;
        const float pad = 16.0f * s;

        HudRenderer::begin_hud(ui_shader);

        if (state == GameState::GAME_OVER && death_shake_timer > 0.0f) {
            float flash_a = 0.42f * std::clamp(death_shake_timer / 0.65f, 0.0f, 1.0f);
            hud.draw_box(ui_shader, ortho, 0.0f, 0.0f, L.vw, L.vh, 1.0f, 0.08f, 0.14f, flash_a);
        }

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
            case GameState::HELP:
                render_help_screen(L, ortho, cw, ch, pad);
                break;
            case GameState::ABOUT:
                render_about_screen(L, ortho, cw, ch, pad);
                break;
            case GameState::COUNTDOWN:
            case GameState::PLAYING:
            case GameState::PAUSED:
            case GameState::GAME_OVER:
                render_gameplay_hud(L, ortho, player, treadmill, save, biome, state, countdown_timer, countdown_value, cw, ch, pad);
                break;
        }

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
        torus_mesh.destroy();
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

        Rect2D top_bar = {pad, 12.0f * s, L.vw - pad * 2.0f, 44.0f * s};
        hud.draw_panel(ui_shader, ortho, top_bar, 12.0f * s, 1.4f,
                       0.05f, 0.09f, 0.22f, 0.84f, 0.02f, 0.04f, 0.10f, 0.90f,
                       0.05f, 0.90f, 1.00f, 0.65f);

        char rank_buf[32];
        std::snprintf(rank_buf, sizeof(rank_buf), "LV.%d %s", save.get_pilot_rank(), save.get_pilot_title());
        hud.draw_text(ui_shader, ortho, top_bar.x + 12.0f * s, top_bar.y + 14.0f * s,
                      cw * 0.78f, ch * 0.78f, rank_buf, 0.20f, 0.96f, 1.0f, 1.0f);

        char orb_buf[32];
        std::snprintf(orb_buf, sizeof(orb_buf), "ORBS %d", save.total_orbs);
        float ow = HudRenderer::measure_text_width(orb_buf, cw * 0.80f);
        hud.draw_text(ui_shader, ortho, top_bar.x + top_bar.w - ow - 12.0f * s, top_bar.y + 14.0f * s,
                      cw * 0.80f, ch * 0.80f, orb_buf, 1.0f, 0.86f, 0.18f, 1.0f);

        hud.draw_text_shadow(ui_shader, ortho, cx, 78.0f * s, cw * 1.92f, ch * 1.92f, "VELOSPHERE", 0.05f, 0.96f, 1.0f, 1.0f);
        hud.draw_text_shadow(ui_shader, ortho, cx, 114.0f * s, cw * 0.85f, ch * 0.85f, "APEX 4.0", 1.0f, 0.28f, 0.86f, 0.96f);

        char best_buf[40];
        std::snprintf(best_buf, sizeof(best_buf), "BEST: %06d", save.get_best_for_mode(treadmill.get_game_mode()));
        hud.draw_text_centered(ui_shader, ortho, cx, 142.0f * s, cw * 0.86f, ch * 0.86f, best_buf, 1.0f, 0.88f, 0.22f, 0.96f);

        char m_title[48];
        save.missions[0].format_title(m_title, sizeof(m_title));
        char m_line[64];
        std::snprintf(m_line, sizeof(m_line), "MISSION: %s (%d/%d)", m_title, save.missions[0].progress, save.missions[0].target);
        hud.draw_text_centered(ui_shader, ortho, cx, L.home_mode_btn.y - 22.0f * s, cw * 0.64f, ch * 0.64f, m_line, 0.78f, 0.92f, 1.0f, 0.88f);

        const char* mode_lbl =
            (treadmill.get_game_mode() == GameMode::TIME_ATTACK) ? "< TIME ATTACK 90S >" :
            (treadmill.get_game_mode() == GameMode::HARDCORE)    ? "< HARDCORE (2X) >" :
                                                                   "< ENDLESS ARCADE >";
        hud.draw_button(ui_shader, ortho, L.home_mode_btn, mode_lbl, cw * 0.80f, ch * 0.80f, 12.0f * s, biome.grid_color, false);

        float pulse = 0.88f + 0.12f * std::sin(elapsed_time * 4.5f);
        hud.draw_button(ui_shader, ortho, L.home_play_btn, "PLAY NOW", cw * 1.24f, ch * 1.24f, 16.0f * s,
                        {0.05f * pulse, 0.96f * pulse, 1.0f * pulse}, true);

        hud.draw_button(ui_shader, ortho, L.home_garage_btn,   "GARAGE",   cw * 0.76f, ch * 0.76f, 12.0f * s, {1.0f, 0.78f, 0.15f}, false);
        hud.draw_button(ui_shader, ortho, L.home_missions_btn, "MISSIONS", cw * 0.76f, ch * 0.76f, 12.0f * s, {0.25f, 1.0f, 0.55f}, false);
        hud.draw_button(ui_shader, ortho, L.home_settings_btn, "SETTINGS", cw * 0.76f, ch * 0.76f, 12.0f * s, {0.75f, 0.52f, 1.0f}, false);

        hud.draw_button(ui_shader, ortho, L.home_help_btn,  "HOW TO PLAY", cw * 0.72f, ch * 0.72f, 10.0f * s, {0.35f, 0.88f, 1.0f}, false);
        hud.draw_button(ui_shader, ortho, L.home_about_btn, "ABOUT",       cw * 0.72f, ch * 0.72f, 10.0f * s, {1.0f, 0.45f, 0.82f}, false);
    }

    void render_garage_screen(const UiLayout& L, const Mat4& ortho,
                              const SaveData& save, GarageTab tab, int preview_skin,
                              float cw, float ch, float pad) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;

        hud.draw_button(ui_shader, ortho, L.back_btn, "< HOME", cw * 0.76f, ch * 0.76f, 10.0f * s, {0.1f, 0.92f, 1.0f}, false);
        hud.draw_text_shadow(ui_shader, ortho, cx, 22.0f * s, cw * 1.12f, ch * 1.12f, "GARAGE", 1.0f, 0.85f, 0.2f, 1.0f);

        char orb_buf[24];
        std::snprintf(orb_buf, sizeof(orb_buf), "%d ORBS", save.total_orbs);
        float ow = HudRenderer::measure_text_width(orb_buf, cw * 0.78f);
        hud.draw_text(ui_shader, ortho, L.vw - ow - pad, 24.0f * s, cw * 0.78f, ch * 0.78f, orb_buf, 1.0f, 0.86f, 0.18f, 1.0f);

        hud.draw_button(ui_shader, ortho, L.garage_tab_skins, "SKINS", cw * 0.84f, ch * 0.84f, 10.0f * s, {0.05f, 0.95f, 1.0f}, tab == GarageTab::SKINS);
        hud.draw_button(ui_shader, ortho, L.garage_tab_upgrades, "UPGRADES", cw * 0.84f, ch * 0.84f, 10.0f * s, {1.0f, 0.30f, 0.85f}, tab == GarageTab::UPGRADES);

        if (tab == GarageTab::SKINS) {
            int idx = std::clamp(preview_skin, 0, static_cast<int>(SKIN_PALETTES.size()) - 1);
            const auto& sk = SKIN_PALETTES[static_cast<size_t>(idx)];
            bool unlocked = save.is_skin_unlocked(idx);
            bool equipped = (save.selected_skin == idx);

            hud.draw_button(ui_shader, ortho, L.garage_prev_skin, "<", cw * 1.2f, ch * 1.2f, 12.0f * s, sk.core_color, false);
            hud.draw_button(ui_shader, ortho, L.garage_next_skin, ">", cw * 1.2f, ch * 1.2f, 12.0f * s, sk.core_color, false);

            Rect2D name_box = {L.garage_prev_skin.x + L.garage_prev_skin.w + 8.0f * s,
                               L.garage_prev_skin.y,
                               L.garage_next_skin.x - (L.garage_prev_skin.x + L.garage_prev_skin.w) - 16.0f * s,
                               L.garage_prev_skin.h};
            hud.draw_panel(ui_shader, ortho, name_box, 12.0f * s, 1.5f,
                           0.05f, 0.08f, 0.18f, 0.88f, 0.02f, 0.03f, 0.09f, 0.92f,
                           sk.core_color.r, sk.core_color.g, sk.core_color.b, 0.90f);
            hud.draw_text_centered(ui_shader, ortho, cx, name_box.y + 16.0f * s, cw * 0.84f, ch * 0.84f, sk.name,
                                   sk.core_color.r, sk.core_color.g, sk.core_color.b, 1.0f);

            char act_lbl[40];
            Color3 act_col = {0.1f, 0.95f, 0.6f};
            bool filled = true;
            if (equipped) { std::snprintf(act_lbl, sizeof(act_lbl), "EQUIPPED"); act_col = {0.35f, 0.95f, 0.65f}; filled = false; }
            else if (unlocked) { std::snprintf(act_lbl, sizeof(act_lbl), "EQUIP SKIN"); act_col = {0.05f, 0.95f, 1.0f}; }
            else if (save.total_orbs >= sk.unlock_cost) { std::snprintf(act_lbl, sizeof(act_lbl), "UNLOCK - %d ORBS", sk.unlock_cost); act_col = {1.0f, 0.82f, 0.15f}; }
            else { std::snprintf(act_lbl, sizeof(act_lbl), "NEED %d ORBS", sk.unlock_cost); act_col = {0.55f, 0.55f, 0.68f}; filled = false; }
            hud.draw_button(ui_shader, ortho, L.garage_action_skin, act_lbl, cw * 0.92f, ch * 0.92f, 14.0f * s, act_col, filled);
        } else {
            for (int i = 0; i < 4; ++i) {
                UpgradeType ut = static_cast<UpgradeType>(i);
                const auto& info = UPGRADE_CATALOG[static_cast<size_t>(i)];
                int lv = save.get_upgrade_level(ut);
                int cost = save.get_next_upgrade_cost(ut);
                const Rect2D& r = L.garage_upgrade_btns[i];

                hud.draw_panel(ui_shader, ortho, r, 12.0f * s, 1.6f,
                               0.05f, 0.08f, 0.18f, 0.90f, 0.02f, 0.03f, 0.09f, 0.94f,
                               info.accent.r, info.accent.g, info.accent.b, 0.82f);
                hud.draw_text(ui_shader, ortho, r.x + 12.0f * s, r.y + 10.0f * s, cw * 0.80f, ch * 0.80f, info.name, info.accent.r, info.accent.g, info.accent.b, 1.0f);
                hud.draw_text(ui_shader, ortho, r.x + 12.0f * s, r.y + 30.0f * s, cw * 0.64f, ch * 0.64f, info.short_desc, 0.80f, 0.88f, 1.0f, 0.86f);

                char cost_lbl[24];
                Color3 cc = {1.0f, 0.86f, 0.18f};
                if (lv >= MAX_UPGRADE_LEVEL) { std::snprintf(cost_lbl, sizeof(cost_lbl), "MAXED"); cc = {0.25f, 1.0f, 0.55f}; }
                else { std::snprintf(cost_lbl, sizeof(cost_lbl), "LV%d -> %d ORBS", lv, cost); if (save.total_orbs < cost) cc = {0.65f, 0.65f, 0.75f}; }
                float cw_txt = HudRenderer::measure_text_width(cost_lbl, cw * 0.68f);
                hud.draw_text(ui_shader, ortho, r.x + r.w - cw_txt - 12.0f * s, r.y + 12.0f * s, cw * 0.68f, ch * 0.68f, cost_lbl, cc.r, cc.g, cc.b, 1.0f);

                Rect2D bar = {r.x + 12.0f * s, r.y + r.h - 18.0f * s, r.w - 24.0f * s, 9.0f * s};
                hud.draw_progress_bar(ui_shader, ortho, bar, static_cast<float>(lv) / static_cast<float>(MAX_UPGRADE_LEVEL), 4.0f * s, info.accent);
            }
        }
    }

    void render_missions_screen(const UiLayout& L, const Mat4& ortho,
                                const SaveData& save, float cw, float ch, float pad) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;
        const float card_w = std::min(L.vw - pad * 2.0f, 368.0f * s);
        const float card_x = cx - card_w * 0.5f;

        hud.draw_button(ui_shader, ortho, L.back_btn, "< HOME", cw * 0.76f, ch * 0.76f, 10.0f * s, {0.1f, 0.92f, 1.0f}, false);
        hud.draw_text_shadow(ui_shader, ortho, cx, 22.0f * s, cw * 1.08f, ch * 1.08f, "MISSIONS", 0.25f, 1.0f, 0.55f, 1.0f);

        float my = 72.0f * s;
        const float mh = 72.0f * s;
        for (int i = 0; i < 3; ++i) {
            const MissionSlot& m = save.missions[i];
            Rect2D r = {card_x, my + static_cast<float>(i) * (mh + 12.0f * s), card_w, mh};
            hud.draw_panel(ui_shader, ortho, r, 12.0f * s, 1.5f,
                           0.05f, 0.09f, 0.19f, 0.90f, 0.02f, 0.04f, 0.10f, 0.94f,
                           0.20f, 0.95f, 0.60f, 0.78f);

            char title[48];
            m.format_title(title, sizeof(title));
            hud.draw_text(ui_shader, ortho, r.x + 12.0f * s, r.y + 10.0f * s, cw * 0.74f, ch * 0.74f, title, 0.95f, 0.98f, 1.0f, 1.0f);

            char rew[24];
            std::snprintf(rew, sizeof(rew), "+%d ORBS", static_cast<int>(m.reward_orbs));
            float rw = HudRenderer::measure_text_width(rew, cw * 0.70f);
            hud.draw_text(ui_shader, ortho, r.x + r.w - rw - 12.0f * s, r.y + 10.0f * s, cw * 0.70f, ch * 0.70f, rew, 1.0f, 0.86f, 0.18f, 1.0f);

            char prog[32];
            std::snprintf(prog, sizeof(prog), "%d / %d", m.progress, m.target);
            hud.draw_text(ui_shader, ortho, r.x + 12.0f * s, r.y + 30.0f * s, cw * 0.64f, ch * 0.64f, prog, 0.35f, 1.0f, 0.70f, 0.92f);

            Rect2D bar = {r.x + 12.0f * s, r.y + r.h - 17.0f * s, r.w - 24.0f * s, 8.0f * s};
            hud.draw_progress_bar(ui_shader, ortho, bar, m.progress_ratio(), 4.0f * s, {0.20f, 1.0f, 0.55f});
        }

        float sy = my + 3.0f * (mh + 12.0f * s) + 10.0f * s;
        Rect2D stats_r = {card_x, sy, card_w, 168.0f * s};
        hud.draw_panel(ui_shader, ortho, stats_r, 14.0f * s, 1.5f,
                       0.04f, 0.07f, 0.16f, 0.90f, 0.01f, 0.03f, 0.08f, 0.94f,
                       0.05f, 0.85f, 1.00f, 0.72f);

        hud.draw_text_centered(ui_shader, ortho, cx, stats_r.y + 12.0f * s, cw * 0.86f, ch * 0.86f, "PILOT CAREER STATS", 0.05f, 0.95f, 1.0f, 1.0f);

        char line[64];
        std::snprintf(line, sizeof(line), "RANK: LV.%d %s  (DONE: %d)", save.get_pilot_rank(), save.get_pilot_title(), save.missions_completed);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 42.0f * s, cw * 0.68f, ch * 0.68f, line, 1.0f, 0.86f, 0.20f, 1.0f);

        std::snprintf(line, sizeof(line), "RUNS: %d   DODGES: %d", save.total_runs, save.total_dodges);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 66.0f * s, cw * 0.68f, ch * 0.68f, line, 0.85f, 0.94f, 1.0f, 0.95f);

        std::snprintf(line, sizeof(line), "NEAR MISSES: %d   MAX X%d", save.total_near_misses, save.best_combo);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 90.0f * s, cw * 0.68f, ch * 0.68f, line, 0.85f, 0.94f, 1.0f, 0.95f);

        std::snprintf(line, sizeof(line), "DISTANCE: %dM   ORBS: %d", save.total_distance_m, save.total_orbs);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 114.0f * s, cw * 0.68f, ch * 0.68f, line, 0.35f, 1.0f, 0.70f, 0.95f);

        std::snprintf(line, sizeof(line), "BEST ARC:%d TA:%d HC:%d", save.high_score, save.best_score_time_attack, save.best_score_hardcore);
        hud.draw_text(ui_shader, ortho, stats_r.x + 14.0f * s, stats_r.y + 138.0f * s, cw * 0.64f, ch * 0.64f, line, 1.0f, 0.45f, 0.85f, 0.95f);
    }

    void render_settings_screen(const UiLayout& L, const Mat4& ortho,
                                const SaveData& save, float cw, float ch, float /*pad*/) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;

        hud.draw_button(ui_shader, ortho, L.back_btn, "< HOME", cw * 0.76f, ch * 0.76f, 10.0f * s, {0.1f, 0.92f, 1.0f}, false);
        hud.draw_text_shadow(ui_shader, ortho, cx, 22.0f * s, cw * 1.08f, ch * 1.08f, "SETTINGS", 0.78f, 0.52f, 1.0f, 1.0f);

        const char* qual_str = (save.graphics_quality == 0) ? "LOW (FAST)"
                             : (save.graphics_quality == 1) ? "MEDIUM" : "HIGH (ULTRA)";
        const char* ctrl_str = (save.control_scheme == 0) ? "SMOOTH DRAG" : "SWIPE LANES";
        const char* sens_str = (save.sensitivity == 0) ? "LOW"
                             : (save.sensitivity == 1) ? "MEDIUM" : "HIGH";

        const char* labels[8] = {
            "MUSIC", "SOUND FX", "HAPTICS", "CONTROLS",
            "SENSITIVITY", "WORLD CURVE", "GRAPHICS", "SHOW FPS"
        };
        const char* values[8] = {
            save.music_enabled ? "ON" : "OFF",
            save.sfx_enabled ? "ON" : "OFF",
            save.haptics_enabled ? "ON" : "OFF",
            ctrl_str,
            sens_str,
            save.world_curve_enabled ? "ON" : "OFF",
            qual_str,
            save.show_fps ? "ON" : "OFF"
        };

        for (int i = 0; i < 8; ++i) {
            const Rect2D& r = L.settings_rows[i];
            bool active = (i == 0 && save.music_enabled) ||
                          (i == 1 && save.sfx_enabled) ||
                          (i == 2 && save.haptics_enabled) ||
                          (i == 5 && save.world_curve_enabled) ||
                          (i == 7 && save.show_fps) ||
                          (i == 3 || i == 4 || i == 6);
            Color3 brd = active ? Color3{0.05f, 0.92f, 1.0f} : Color3{0.45f, 0.45f, 0.58f};

            hud.draw_panel(ui_shader, ortho, r, 12.0f * s, 1.5f,
                           0.05f, 0.08f, 0.18f, 0.90f, 0.02f, 0.04f, 0.10f, 0.94f,
                           brd.r, brd.g, brd.b, 0.78f);
            hud.draw_text(ui_shader, ortho, r.x + 14.0f * s, r.y + (r.h - ch * 0.80f) * 0.5f,
                          cw * 0.80f, ch * 0.80f, labels[i], 0.90f, 0.95f, 1.0f, 1.0f);

            float vw_txt = HudRenderer::measure_text_width(values[i], cw * 0.80f);
            hud.draw_text(ui_shader, ortho, r.x + r.w - vw_txt - 14.0f * s,
                          r.y + (r.h - ch * 0.80f) * 0.5f,
                          cw * 0.80f, ch * 0.80f, values[i], brd.r, brd.g, brd.b, 1.0f);
        }
    }

    void render_help_screen(const UiLayout& L, const Mat4& ortho, float cw, float ch, float pad) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;
        const float card_w = std::min(L.vw - pad * 2.0f, 368.0f * s);
        const float card_x = cx - card_w * 0.5f;

        hud.draw_button(ui_shader, ortho, L.back_btn, "< HOME", cw * 0.76f, ch * 0.76f, 10.0f * s, {0.1f, 0.92f, 1.0f}, false);
        hud.draw_text_shadow(ui_shader, ortho, cx, 22.0f * s, cw * 1.06f, ch * 1.06f, "HOW TO PLAY", 0.30f, 0.92f, 1.0f, 1.0f);

        // Card 1: Controls
        Rect2D c1 = {card_x, 72.0f * s, card_w, 134.0f * s};
        hud.draw_panel(ui_shader, ortho, c1, 14.0f * s, 1.5f,
                       0.05f, 0.09f, 0.20f, 0.92f, 0.02f, 0.04f, 0.10f, 0.95f,
                       0.05f, 0.92f, 1.00f, 0.80f);
        hud.draw_text(ui_shader, ortho, c1.x + 14.0f * s, c1.y + 12.0f * s, cw * 0.84f, ch * 0.84f, "CONTROLS", 0.05f, 0.96f, 1.0f, 1.0f);
        hud.draw_text(ui_shader, ortho, c1.x + 14.0f * s, c1.y + 38.0f * s, cw * 0.64f, ch * 0.64f,
                      "DRAG / SWIPE L-R : STEER 3 LANES\nSWIPE UP         : 360 JUMP + BUFFER\nSWIPE DOWN (AIR) : FAST-DROP SLAM\nDOUBLE-TAP       : WARP PHASE-DASH",
                      0.88f, 0.95f, 1.0f, 0.95f);

        // Card 2: 6 Articulated Hazards
        Rect2D c2 = {card_x, c1.y + c1.h + 14.0f * s, card_w, 168.0f * s};
        hud.draw_panel(ui_shader, ortho, c2, 14.0f * s, 1.5f,
                       0.08f, 0.04f, 0.14f, 0.92f, 0.03f, 0.02f, 0.08f, 0.95f,
                       1.00f, 0.30f, 0.65f, 0.80f);
        hud.draw_text(ui_shader, ortho, c2.x + 14.0f * s, c2.y + 12.0f * s, cw * 0.84f, ch * 0.84f, "6 HAZARD TYPES", 1.0f, 0.35f, 0.72f, 1.0f);
        hud.draw_text(ui_shader, ortho, c2.x + 14.0f * s, c2.y + 38.0f * s, cw * 0.62f, ch * 0.62f,
                      "RUBY PILLARS   : WEAVE AROUND\nSWEEPER DRONES : WATCH PULSE & DODGE\nLOW LASERS     : JUMP OVER FOR ORBS\nOVERHEAD GATES : STAY GROUNDED UNDER\nLASER SPINNERS : TIME JUMP OR WEAVE\nSPLIT GATES    : AIM FOR CENTER GAP",
                      0.92f, 0.90f, 1.0f, 0.94f);

        // Card 3: Hyperflow & Upgrades
        Rect2D c3 = {card_x, c2.y + c2.h + 14.0f * s, card_w, 134.0f * s};
        hud.draw_panel(ui_shader, ortho, c3, 14.0f * s, 1.5f,
                       0.04f, 0.10f, 0.14f, 0.92f, 0.02f, 0.05f, 0.08f, 0.95f,
                       0.25f, 1.00f, 0.58f, 0.80f);
        hud.draw_text(ui_shader, ortho, c3.x + 14.0f * s, c3.y + 12.0f * s, cw * 0.84f, ch * 0.84f, "HYPERFLOW & PRO TIPS", 0.25f, 1.0f, 0.58f, 1.0f);
        hud.draw_text(ui_shader, ortho, c3.x + 14.0f * s, c3.y + 38.0f * s, cw * 0.62f, ch * 0.62f,
                      "NEAR MISSES CHARGE HYPERFLOW FAST\nHYPERFLOW DOUBLES COMBO UP TO 10X!\nDOUBLE-TAP PHASE-DASH THROUGH WALLS\nSPEND ORBS IN GARAGE FOR UPGRADES",
                      0.86f, 0.98f, 0.92f, 0.95f);
    }

    void render_about_screen(const UiLayout& L, const Mat4& ortho, float cw, float ch, float pad) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;
        const float card_w = std::min(L.vw - pad * 2.0f, 368.0f * s);
        const float card_x = cx - card_w * 0.5f;

        hud.draw_button(ui_shader, ortho, L.back_btn, "< HOME", cw * 0.76f, ch * 0.76f, 10.0f * s, {0.1f, 0.92f, 1.0f}, false);
        hud.draw_text_shadow(ui_shader, ortho, cx, 22.0f * s, cw * 1.08f, ch * 1.08f, "ABOUT", 1.0f, 0.45f, 0.85f, 1.0f);

        Rect2D hero = {card_x, 76.0f * s, card_w, 124.0f * s};
        hud.draw_panel(ui_shader, ortho, hero, 16.0f * s, 1.8f,
                       0.06f, 0.10f, 0.22f, 0.92f, 0.02f, 0.04f, 0.10f, 0.96f,
                       0.05f, 0.95f, 1.00f, 0.85f);
        hud.draw_text_shadow(ui_shader, ortho, cx, hero.y + 20.0f * s, cw * 1.38f, ch * 1.38f, "VELOSPHERE", 0.05f, 0.96f, 1.0f, 1.0f);
        hud.draw_text_centered(ui_shader, ortho, cx, hero.y + 56.0f * s, cw * 0.82f, ch * 0.82f, "VERSION 4.0.0 - APEX EDITION", 1.0f, 0.35f, 0.85f, 1.0f);
        hud.draw_text_centered(ui_shader, ortho, cx, hero.y + 86.0f * s, cw * 0.66f, ch * 0.66f, "HIGH-SPEED 3D SYNTHWAVE RUNNER", 0.82f, 0.92f, 1.0f, 0.92f);

        Rect2D specs = {card_x, hero.y + hero.h + 16.0f * s, card_w, 162.0f * s};
        hud.draw_panel(ui_shader, ortho, specs, 14.0f * s, 1.5f,
                       0.05f, 0.08f, 0.18f, 0.92f, 0.02f, 0.03f, 0.09f, 0.95f,
                       0.75f, 0.48f, 1.00f, 0.78f);
        hud.draw_text(ui_shader, ortho, specs.x + 14.0f * s, specs.y + 14.0f * s, cw * 0.82f, ch * 0.82f, "ENGINE ARCHITECTURE", 0.78f, 0.55f, 1.0f, 1.0f);
        hud.draw_text(ui_shader, ortho, specs.x + 14.0f * s, specs.y + 42.0f * s, cw * 0.64f, ch * 0.64f,
                      "GRAPHICS : OPENGL ES 3.0 CURVED 3D\nCORE     : NATIVE C++17 ZERO-ALLOC\nPLATFORM : ANDROID GAMEACTIVITY\nAUDIO    : 13-CH SYNTHWAVE SFX BANK\nDISPLAY  : PORTRAIT LOCKED 60/120FPS",
                      0.88f, 0.94f, 1.0f, 0.95f);

        Rect2D cred = {card_x, specs.y + specs.h + 16.0f * s, card_w, 108.0f * s};
        hud.draw_panel(ui_shader, ortho, cred, 14.0f * s, 1.5f,
                       0.05f, 0.09f, 0.16f, 0.92f, 0.02f, 0.04f, 0.08f, 0.95f,
                       1.00f, 0.82f, 0.18f, 0.78f);
        hud.draw_text(ui_shader, ortho, cred.x + 14.0f * s, cred.y + 14.0f * s, cw * 0.82f, ch * 0.82f, "CREDITS & LICENSE", 1.0f, 0.86f, 0.20f, 1.0f);
        hud.draw_text(ui_shader, ortho, cred.x + 14.0f * s, cred.y + 42.0f * s, cw * 0.66f, ch * 0.66f,
                      "CREATED BY : ASSISHMON C S\nLICENSE    : MIT OPEN SOURCE\nGITHUB     : ASSISHMONCS/VELOSPHERE",
                      0.90f, 0.95f, 1.0f, 0.95f);
    }

    void render_gameplay_hud(const UiLayout& L, const Mat4& ortho,
                             const PlayerBall& player, const TreadmillSystem& treadmill,
                             const SaveData& save, const BiomePalette& biome,
                             GameState state, float countdown_timer, int countdown_value,
                             float cw, float ch, float pad) {
        const float s = L.s;
        const float cx = L.vw * 0.5f;

        hud.draw_box(ui_shader, ortho, 0.0f, 0.0f, L.vw, 62.0f * s, 0.02f, 0.03f, 0.08f, 0.82f);
        hud.draw_box(ui_shader, ortho, 0.0f, 60.0f * s, L.vw, 2.0f * s, biome.grid_color.r, biome.grid_color.g, biome.grid_color.b, 0.90f);

        if (state == GameState::PLAYING || state == GameState::COUNTDOWN || state == GameState::PAUSED) {
            hud.draw_button(ui_shader, ortho, L.pause_btn, (state == GameState::PAUSED) ? ">" : "II",
                            cw * 1.02f, ch * 1.02f, 10.0f * s, {0.05f, 0.95f, 1.0f}, false);
        }

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
        hud.draw_text(ui_shader, ortho, pad, 36.0f * s, cw * 0.76f, ch * 0.76f, sub_left, sub_col.r, sub_col.g, sub_col.b, 0.98f);

        int mode_best = std::max(save.get_best_for_mode(treadmill.get_game_mode()), treadmill.get_score());
        char best_str[28];
        std::snprintf(best_str, sizeof(best_str), "BEST %06d", mode_best);
        float bw = HudRenderer::measure_text_width(best_str, cw * 0.86f);
        hud.draw_text(ui_shader, ortho, L.vw - bw - pad, 12.0f * s, cw * 0.86f, ch * 0.86f, best_str, 1.0f, 0.86f, 0.18f, 1.0f);

        char wave_buf[32];
        std::snprintf(wave_buf, sizeof(wave_buf), "W%d %s", treadmill.get_intensity_wave(), biome.name);
        float biome_w = HudRenderer::measure_text_width(wave_buf, cw * 0.66f);
        hud.draw_text(ui_shader, ortho, L.vw - biome_w - pad, 36.0f * s, cw * 0.66f, ch * 0.66f,
                      wave_buf, biome.grid_color.r, biome.grid_color.g, biome.grid_color.b, 0.92f);

        // Hyperflow Gauge
        const float fbar_w = 152.0f * s;
        Rect2D fbar = {cx - fbar_w * 0.5f, 68.0f * s, fbar_w, 9.0f * s};
        Color3 fcol = treadmill.is_fever_active() ? Color3{1.0f, 0.25f, 0.92f} : Color3{0.10f, 0.92f, 1.0f};
        hud.draw_progress_bar(ui_shader, ortho, fbar, treadmill.get_fever_meter(), 4.5f * s, fcol);
        hud.draw_text_centered(ui_shader, ortho, cx, fbar.y + 12.0f * s, cw * 0.60f, ch * 0.60f,
                               treadmill.is_fever_active() ? "HYPERFLOW 2X" : "FLOW", fcol.r, fcol.g, fcol.b, 0.92f);

        // Active Power-Up Bar
        if (treadmill.get_active_powerup() != PowerUpType::NONE) {
            const char* pu_name =
                (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) ? "OVERDRIVE" :
                (treadmill.get_active_powerup() == PowerUpType::SLOW_MO)   ? "CHRONO SLOW-MO" : "ORB MAGNET";
            Rect2D pbar = {cx - 72.0f * s, 96.0f * s, 144.0f * s, 8.0f * s};
            hud.draw_progress_bar(ui_shader, ortho, pbar, treadmill.get_powerup_progress(), 4.0f * s, {1.0f, 0.35f, 0.90f});
            hud.draw_text_centered(ui_shader, ortho, cx, pbar.y + 11.0f * s, cw * 0.66f, ch * 0.66f, pu_name, 1.0f, 0.86f, 0.20f, 1.0f);
        }

        // Floating Alert Banner
        if (treadmill.get_banner_timer() > 0.0f) {
            float alpha = std::min(1.0f, treadmill.get_banner_timer() * 1.5f);
            hud.draw_text_shadow(ui_shader, ortho, cx, 130.0f * s, cw * 1.04f, ch * 1.04f,
                                 treadmill.get_banner_text(), 1.0f, 0.90f, 0.18f, alpha);
        } else if (treadmill.get_dodge_milestone_timer() > 0.0f) {
            char msg[32];
            std::snprintf(msg, sizeof(msg), "%d+ DODGES!", treadmill.get_dodge_milestone());
            float a = std::min(1.0f, treadmill.get_dodge_milestone_timer());
            hud.draw_text_shadow(ui_shader, ortho, cx, 130.0f * s, cw * 1.04f, ch * 1.04f, msg, 0.15f, 1.0f, 0.60f, a);
        }

        // Bottom-Left: Speedometer & Distance
        int mph = static_cast<int>(treadmill.get_global_speed() * 1.8f);
        char spd_str[36];
        std::snprintf(spd_str, sizeof(spd_str), "%d MPH  %dM", mph, static_cast<int>(treadmill.get_distance_traveled()));
        hud.draw_text(ui_shader, ortho, pad, L.vh - 38.0f * s, cw * 0.80f, ch * 0.80f, spd_str, 0.25f, 1.0f, 0.55f, 0.92f);

        // Bottom-Center: Phase-Dash Charge Bar
        if (state == GameState::PLAYING) {
            float dr = player.get_dash_ready_ratio();
            Rect2D dbar = {cx - 56.0f * s, L.vh - 34.0f * s, 112.0f * s, 8.0f * s};
            Color3 dcol = (dr >= 0.99f) ? Color3{0.15f, 1.0f, 0.95f} : Color3{0.45f, 0.62f, 0.78f};
            hud.draw_progress_bar(ui_shader, ortho, dbar, dr, 4.0f * s, dcol);
            hud.draw_text_centered(ui_shader, ortho, cx, dbar.y - 14.0f * s, cw * 0.56f, ch * 0.56f,
                                   (dr >= 0.99f) ? "2X TAP: DASH READY" : "RECHARGING DASH", dcol.r, dcol.g, dcol.b, 0.90f);
        }

        // Bottom-Right: Shield Badge
        if (treadmill.is_shield_active()) {
            const char* sh = "[SHIELD]";
            float sw = HudRenderer::measure_text_width(sh, cw * 0.80f);
            hud.draw_text(ui_shader, ortho, L.vw - sw - pad, L.vh - 38.0f * s, cw * 0.80f, ch * 0.80f, sh, 0.05f, 0.95f, 1.0f, 1.0f);
        }

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
            Rect2D panel = {cx - p_w * 0.5f, L.vh * 0.27f, p_w, 280.0f * s};
            hud.draw_panel(ui_shader, ortho, panel, 18.0f * s, 2.0f,
                           0.05f, 0.09f, 0.20f, 0.94f, 0.02f, 0.03f, 0.09f, 0.96f,
                           0.05f, 0.92f, 1.00f, 0.92f);
            hud.draw_text_shadow(ui_shader, ortho, cx, panel.y + 20.0f * s, cw * 1.28f, ch * 1.28f, "PAUSED", 0.05f, 0.95f, 1.0f, 1.0f);
            hud.draw_button(ui_shader, ortho, L.pause_resume_btn, "RESUME", cw * 0.95f, ch * 0.95f, 12.0f * s, {0.05f, 0.95f, 1.0f}, true);
            hud.draw_button(ui_shader, ortho, L.pause_restart_btn, "RESTART RUN", cw * 0.84f, ch * 0.84f, 12.0f * s, {1.0f, 0.80f, 0.20f}, false);
            char aud_lbl[32];
            std::snprintf(aud_lbl, sizeof(aud_lbl), "AUDIO: %s", (save.sfx_enabled || save.music_enabled) ? "ON" : "MUTED");
            hud.draw_button(ui_shader, ortho, L.pause_audio_btn, aud_lbl, cw * 0.82f, ch * 0.82f, 12.0f * s, {0.65f, 0.55f, 1.0f}, false);
            hud.draw_button(ui_shader, ortho, L.pause_home_btn, "HOME MENU", cw * 0.84f, ch * 0.84f, 12.0f * s, {1.0f, 0.40f, 0.65f}, false);
        } else if (state == GameState::GAME_OVER) {
            hud.draw_box(ui_shader, ortho, 0.0f, 0.0f, L.vw, L.vh, 0.04f, 0.005f, 0.015f, 0.52f);
            const float go_w = std::min(L.vw - pad * 2.0f, 350.0f * s);
            Rect2D panel = {cx - go_w * 0.5f, L.vh * 0.20f, go_w, 378.0f * s};
            hud.draw_panel(ui_shader, ortho, panel, 18.0f * s, 2.0f,
                           0.10f, 0.03f, 0.07f, 0.94f, 0.03f, 0.01f, 0.04f, 0.96f,
                           1.00f, 0.24f, 0.42f, 0.92f);

            const char* hdr = treadmill.is_time_expired() ? "TIME UP!" : "RUN COMPLETE";
            hud.draw_text_shadow(ui_shader, ortho, cx, panel.y + 18.0f * s, cw * 1.24f, ch * 1.24f, hdr, 1.0f, 0.30f, 0.45f, 1.0f);
            if (treadmill.has_new_best()) {
                hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 52.0f * s, cw * 0.94f, ch * 0.94f, "NEW RECORD!", 1.0f, 0.88f, 0.15f, 1.0f);
            }

            char buf[48];
            std::snprintf(buf, sizeof(buf), "SCORE   %06d", treadmill.get_score());
            hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 82.0f * s, cw * 1.00f, ch * 1.00f, buf, 1.0f, 1.0f, 1.0f, 1.0f);
            std::snprintf(buf, sizeof(buf), "BEST    %06d", mode_best);
            hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 112.0f * s, cw * 0.90f, ch * 0.90f, buf, 1.0f, 0.86f, 0.20f, 1.0f);
            std::snprintf(buf, sizeof(buf), "+%d ORBS   DIST %dM", treadmill.get_run_orbs(), static_cast<int>(treadmill.get_distance_traveled()));
            hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 144.0f * s, cw * 0.76f, ch * 0.76f, buf, 0.30f, 1.0f, 0.68f, 0.95f);
            std::snprintf(buf, sizeof(buf), "WAVE %d  NEAR %d  MAX X%d", treadmill.get_intensity_wave(), treadmill.get_near_miss_count(), treadmill.get_best_run_combo());
            hud.draw_text_centered(ui_shader, ortho, cx, panel.y + 170.0f * s, cw * 0.70f, ch * 0.70f, buf, 0.82f, 0.88f, 1.0f, 0.92f);

            if (treadmill.can_revive() && save.total_orbs >= SaveData::REVIVE_ORB_COST) {
                char rev_lbl[36];
                std::snprintf(rev_lbl, sizeof(rev_lbl), "REVIVE (%d ORBS)", SaveData::REVIVE_ORB_COST);
                hud.draw_button(ui_shader, ortho, L.over_revive_btn, rev_lbl, cw * 0.86f, ch * 0.86f, 12.0f * s, {1.0f, 0.82f, 0.15f}, true);
            } else {
                hud.draw_button(ui_shader, ortho, L.over_revive_btn, "REVIVE UNAVAILABLE", cw * 0.72f, ch * 0.72f, 12.0f * s, {0.42f, 0.42f, 0.52f}, false);
            }

            hud.draw_button(ui_shader, ortho, L.over_retry_btn, "PLAY AGAIN", cw * 1.00f, ch * 1.00f, 14.0f * s, {0.05f, 0.95f, 1.0f}, true);
            hud.draw_button(ui_shader, ortho, L.over_home_btn,  "HOME MENU",  cw * 0.84f, ch * 0.84f, 12.0f * s, {0.88f, 0.75f, 1.0f}, false);
        }
    }
};

// =============================================================================
// APP STATE, PERSISTENCE & JNI CONTROLLERS
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
    float               hitstop_timer{0.0f};

    bool                touch_active{false};
    float               touch_down_x{0.0f};
    float               touch_down_y{0.0f};
    float               last_touch_x{0.0f};
    float               last_touch_y{0.0f};
    float               last_tap_time{-10.0f};
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
    if (f) { fwrite(&data, sizeof(SaveData), 1, f); fclose(f); }
    std::string legacy_path = get_legacy_save_file_path(app);
    if (!legacy_path.empty()) {
        FILE* lf = fopen(legacy_path.c_str(), "wb");
        if (lf) { fwrite(&data.high_score, sizeof(int), 1, lf); fclose(lf); }
    }
}

static SaveData load_profile_data(struct android_app* app) {
    SaveData data{};
    data.seal();
    std::string path = get_save_file_path(app);
    if (!path.empty()) {
        FILE* f = fopen(path.c_str(), "rb");
        if (f) {
            SaveData loaded_v4{};
            if (fread(&loaded_v4, sizeof(SaveData), 1, f) == 1 && loaded_v4.is_valid()) {
                fclose(f);
                return loaded_v4;
            }
            fseek(f, 0, SEEK_SET);
            SaveDataV2 loaded_v2{};
            if (fread(&loaded_v2, sizeof(SaveDataV2), 1, f) == 1 && loaded_v2.is_valid()) {
                fclose(f);
                return SaveData::migrate_from_v2(loaded_v2);
            }
            fclose(f);
        }
    }
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
    state->player.reset(state->save_data.get_upgrade_level(UpgradeType::OVERDRIVE_THRUSTER));
    state->renderer.trail.reset(state->player.pos);
    state->death_shake_timer = 0.0f;
    state->hitstop_timer = 0.0f;
    if (with_countdown) {
        state->countdown_timer = 0.72f;
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
            if (app->window != nullptr && state->egl.initialize(app->window)) {
                state->renderer.init();
                state->renderer.trail.reset(state->player.pos);
                state->timer.reset();
                state->animating = true;
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
                            trigger_haptic(state->app, 25, 95);
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
                        } else if (L.home_help_btn.contains(x, y)) {
                            state->game_state = GameState::HELP;
                            call_java_sfx(state->app, SFX_UI_CLICK);
                        } else if (L.home_about_btn.contains(x, y)) {
                            state->game_state = GameState::ABOUT;
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
                                    trigger_haptic(state->app, 55, 165);
                                }
                            }
                        } else {
                            for (int u = 0; u < 4; ++u) {
                                if (L.garage_upgrade_btns[u].contains(x, y)) {
                                    if (state->save_data.try_buy_upgrade(static_cast<UpgradeType>(u))) {
                                        state->treadmill.apply_upgrades(state->save_data);
                                        save_profile_data(state->app, state->save_data);
                                        call_java_sfx(state->app, SFX_UNLOCK);
                                        trigger_haptic(state->app, 55, 165);
                                    }
                                    break;
                                }
                            }
                        }
                        break;

                    case GameState::MISSIONS:
                    case GameState::HELP:
                    case GameState::ABOUT:
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
                            if (L.settings_rows[0].contains(x, y)) { state->save_data.music_enabled ^= 1; changed = true; }
                            else if (L.settings_rows[1].contains(x, y)) { state->save_data.sfx_enabled ^= 1; changed = true; }
                            else if (L.settings_rows[2].contains(x, y)) { state->save_data.haptics_enabled ^= 1; changed = true; }
                            else if (L.settings_rows[3].contains(x, y)) { state->save_data.control_scheme = (state->save_data.control_scheme + 1) % 2; changed = true; }
                            else if (L.settings_rows[4].contains(x, y)) { state->save_data.sensitivity = (state->save_data.sensitivity + 1) % 3; changed = true; }
                            else if (L.settings_rows[5].contains(x, y)) { state->save_data.world_curve_enabled ^= 1; changed = true; }
                            else if (L.settings_rows[6].contains(x, y)) { state->save_data.graphics_quality = (state->save_data.graphics_quality + 1) % 3; changed = true; }
                            else if (L.settings_rows[7].contains(x, y)) { state->save_data.show_fps ^= 1; changed = true; }
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
                        } else if (state->game_state == GameState::PLAYING) {
                            // Double-Tap Detection -> Warp Phase-Dash!
                            float now_t = state->renderer.elapsed_time;
                            if ((now_t - state->last_tap_time) <= 0.28f) {
                                if (state->player.trigger_phase_dash()) {
                                    state->renderer.shockwaves.spawn(state->player.pos, {0.15f, 1.0f, 0.95f}, 3.4f, 0.36f);
                                    state->renderer.particles.spawn_orb_burst(state->player.pos, {0.15f, 1.0f, 0.95f});
                                    call_java_sfx(state->app, SFX_PHASE_DASH);
                                    trigger_haptic(state->app, 40, 150);
                                }
                                state->last_tap_time = -10.0f;
                            } else {
                                state->last_tap_time = now_t;
                            }
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
                            state->player.reset(state->save_data.get_upgrade_level(UpgradeType::OVERDRIVE_THRUSTER));
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
                            state->renderer.shockwaves.spawn(state->player.pos, {1.0f, 0.85f, 0.15f}, 4.5f, 0.50f);
                            state->death_shake_timer = 0.0f;
                            state->game_state = GameState::PLAYING;
                            state->timer.reset();
                            call_java_sfx(state->app, SFX_POWERUP);
                            trigger_haptic(state->app, 75, 185);
                        } else if (L.over_retry_btn.contains(x, y)) {
                            call_java_sfx(state->app, SFX_UI_CLICK);
                            start_new_run(state, false);
                        } else if (L.over_home_btn.contains(x, y)) {
                            state->treadmill.reset();
                            state->player.reset(state->save_data.get_upgrade_level(UpgradeType::OVERDRIVE_THRUSTER));
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
                    float scale = hud_base_scale(vw, vh);

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

                    if (!state->vertical_gesture_consumed) {
                        if (total_dy < -32.0f * scale) {
                            if (state->player.jump()) {
                                state->renderer.shockwaves.spawn({state->player.pos.x, 0.12f, state->player.pos.z}, {0.10f, 0.95f, 1.0f}, 2.2f, 0.34f);
                                call_java_sfx(state->app, SFX_JUMP);
                                trigger_haptic(state->app, 22, 85);
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
    LOGI("   VeloSphere 4.0 - Apex Curved-World 3D Engine  ");
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
    state.player.reset(state.save_data.get_upgrade_level(UpgradeType::OVERDRIVE_THRUSTER));
    sync_java_settings(app, state.save_data);

    while (!app->destroyRequested) {
        int events = 0;
        struct android_poll_source* source = nullptr;
        while (ALooper_pollOnce(state.animating ? 0 : -1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source != nullptr) source->process(app, source);
            if (app->destroyRequested) break;
        }

        process_input_events(&state);

        if (state.animating && state.egl.is_ready) {
            const float raw_dt = state.timer.tick();
            state.renderer.elapsed_time += raw_dt;

            // Cinematic hitstop micro-slowmo on shield/overdrive smash
            float dt = raw_dt;
            if (state.hitstop_timer > 0.0f) {
                state.hitstop_timer = std::max(0.0f, state.hitstop_timer - raw_dt);
                dt = raw_dt * 0.20f;
            }

            if (state.game_state == GameState::COUNTDOWN) {
                state.countdown_timer -= raw_dt;
                if (state.countdown_timer <= 0.0f) {
                    if (state.countdown_value > 0) {
                        --state.countdown_value;
                        state.countdown_timer = (state.countdown_value == 0) ? 0.45f : 0.68f;
                        call_java_sfx(state.app, SFX_COUNTDOWN);
                    } else {
                        state.game_state = GameState::PLAYING;
                        state.countdown_timer = 0.0f;
                    }
                }
            } else if (state.game_state == GameState::PLAYING) {
                const float prev_milestone = state.treadmill.get_dodge_milestone_timer();
                const FrameEvents fev = state.treadmill.update(dt, state.player.pos, state.player.is_phase_dashing());

                if (state.treadmill.get_dodge_milestone_timer() > prev_milestone) trigger_haptic(state.app, 60, 110);
                if (fev.near_miss_triggered) {
                    state.renderer.shockwaves.spawn(state.player.pos, {1.0f, 0.88f, 0.15f}, 2.4f, 0.30f);
                    call_java_sfx(state.app, SFX_NEAR_MISS);
                    trigger_haptic(state.app, 28, 100);
                }
                if (fev.fever_started) {
                    state.renderer.shockwaves.spawn(state.player.pos, {1.0f, 0.25f, 0.92f}, 4.0f, 0.48f);
                    call_java_sfx(state.app, SFX_FEVER);
                    trigger_haptic(state.app, 90, 200);
                }

                state.player.update(dt, state.treadmill.get_global_speed());
                state.renderer.trail.update(state.player.pos, state.treadmill.get_global_speed(), dt);

                if (state.player.just_landed) {
                    state.renderer.shockwaves.spawn({state.player.pos.x, 0.08f, state.player.pos.z}, {0.15f, 0.95f, 1.0f}, 2.0f, 0.28f);
                    state.renderer.particles.spawn_orb_burst({state.player.pos.x, 0.12f, state.player.pos.z}, {0.15f, 0.92f, 1.0f});
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
                    state.renderer.shockwaves.spawn(state.player.pos, {1.0f, 0.25f, 0.92f}, 4.0f, 0.48f);
                    call_java_sfx(state.app, SFX_FEVER);
                    trigger_haptic(state.app, 90, 200);
                }
                if (outcome.powerup_picked_up) {
                    state.renderer.shockwaves.spawn(state.player.pos, {0.15f, 1.0f, 0.65f}, 3.2f, 0.40f);
                    call_java_sfx(state.app, SFX_POWERUP);
                    trigger_haptic(state.app, 45, 145);
                    state.renderer.particles.spawn_orb_burst(state.player.pos, {0.1f, 0.95f, 1.0f});
                }
                if (outcome.phase_dodged || outcome.obstacle_smashed || outcome.shield_broken) {
                    state.hitstop_timer = 0.045f;
                    state.death_shake_timer = 0.28f;
                    state.renderer.shockwaves.spawn(outcome.impact_pos, {0.10f, 0.98f, 1.0f}, 3.8f, 0.42f);
                    state.renderer.particles.spawn_shield_shatter(outcome.impact_pos);
                    call_java_sfx(state.app, SFX_SHIELD_SMASH);
                    trigger_haptic(state.app, 85, 195);
                }

                int mission_orbs = state.save_data.advance_missions(
                    orbs_delta, fev.dodges_this_frame, fev.near_miss_triggered ? 1 : 0,
                    fev.distance_delta_m, state.treadmill.get_combo(), fevers_delta);
                if (mission_orbs > 0) {
                    char mbuf[32];
                    std::snprintf(mbuf, sizeof(mbuf), "MISSION +%d ORBS!", mission_orbs);
                    state.treadmill.trigger_banner(mbuf, 1.6f);
                    call_java_sfx(state.app, SFX_MISSION_DONE);
                    trigger_haptic(state.app, 70, 175);
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

                if (state.death_shake_timer > 0.0f) state.death_shake_timer = std::max(0.0f, state.death_shake_timer - raw_dt);
            } else if (state.game_state == GameState::HOME ||
                       state.game_state == GameState::GARAGE ||
                       state.game_state == GameState::MISSIONS ||
                       state.game_state == GameState::SETTINGS ||
                       state.game_state == GameState::HELP ||
                       state.game_state == GameState::ABOUT) {
                state.player.update(raw_dt, 8.5f);
                state.renderer.trail.update(state.player.pos, 8.5f, raw_dt);
            } else if (state.game_state == GameState::GAME_OVER) {
                state.player.update(raw_dt, 0.0f);
                if (state.death_shake_timer > 0.0f) state.death_shake_timer = std::max(0.0f, state.death_shake_timer - raw_dt);
            }

            state.renderer.shockwaves.update(raw_dt);
            state.renderer.particles.update(raw_dt);

            state.renderer.render_frame(
                state.player, state.treadmill, state.save_data,
                state.game_state, state.garage_tab, state.garage_skin_idx,
                state.egl.width, state.egl.height,
                state.death_shake_timer, state.countdown_timer, state.countdown_value,
                state.timer.get_fps()
            );

            state.egl.swap_buffers();
        }
    }

    save_profile_data(state.app, state.save_data);
    state.renderer.destroy();
    state.egl.terminate();
}