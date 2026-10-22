#pragma once

#include <array>
#include <random>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "Math3D.h"

namespace velo {

enum class GameState {
    READY,
    COUNTDOWN,
    PLAYING,
    PAUSED,
    GAME_OVER
};

enum class GameMode : int {
    ARCADE      = 0, // Classic progressive endless runner with all power-ups
    TIME_ATTACK = 1, // 90s clock; orbs, dodges & near-misses add bonus time
    HARDCORE    = 2  // High base speed, no shields, 2x score & orb multiplier
};

enum class SphereSkin : int {
    CYBER_CYAN       = 0,
    SOLAR_FLARE      = 1,
    VOID_VIOLET      = 2,
    EMERALD_MATRIX   = 3,
    SUPERNOVA_CHROME = 4,
    COUNT            = 5
};

struct SkinPalette {
    const char* name;
    int         unlock_cost;
    Color3      core_color;
    Color3      energy_color;
};

inline constexpr std::array<SkinPalette, 5> SKIN_PALETTES = {{
    {"CYBER CYAN",     0,   {0.12f, 0.95f, 0.88f}, {1.00f, 0.25f, 0.80f}},
    {"SOLAR FLARE",    50,  {1.00f, 0.72f, 0.10f}, {1.00f, 0.15f, 0.20f}},
    {"VOID VIOLET",    120, {0.48f, 0.18f, 0.98f}, {0.05f, 0.95f, 1.00f}},
    {"EMERALD MATRIX", 200, {0.08f, 0.96f, 0.38f}, {0.95f, 1.00f, 0.15f}},
    {"SUPERNOVA",      350, {0.95f, 0.96f, 1.00f}, {1.00f, 0.20f, 0.95f}}
}};

struct BiomePalette {
    const char* name;
    Color3      floor_base;
    Color3      grid_color;
    Color3      rail_color;
    Color3      sky_fog;
};

inline constexpr std::array<BiomePalette, 4> BIOME_PALETTES = {{
    {"NEON GRID",    {0.03f, 0.04f, 0.09f}, {0.00f, 0.85f, 1.00f}, {0.00f, 0.92f, 1.00f}, {0.02f, 0.03f, 0.06f}},
    {"SOLAR OUTRUN", {0.08f, 0.02f, 0.07f}, {1.00f, 0.45f, 0.15f}, {1.00f, 0.20f, 0.65f}, {0.06f, 0.01f, 0.05f}},
    {"CYBER MATRIX", {0.01f, 0.06f, 0.04f}, {0.10f, 0.98f, 0.45f}, {0.35f, 1.00f, 0.25f}, {0.01f, 0.04f, 0.02f}},
    {"VOID HORIZON", {0.04f, 0.02f, 0.10f}, {0.65f, 0.25f, 1.00f}, {0.00f, 0.95f, 1.00f}, {0.02f, 0.01f, 0.07f}}
}};

// The runner has exactly three playable lanes.
enum class TrackLane : int {
    LEFT   = 0,
    CENTER = 1,
    RIGHT  = 2
};

enum class ObstacleType : int {
    STATIC_BLOCK   = 0, // Full-height ruby barrier (weave around)
    MOVING_SWEEPER = 1, // Amber sweeping hazard (predict & weave)
    LOW_BARRIER    = 2, // Low purple laser hurdle (jump over or weave)
    OVERHEAD_GATE  = 3  // Elevated crimson gate (stay grounded to roll under)
};

enum class PowerUpType : int {
    NONE      = 0,
    SHIELD    = 1, // Absorbs 1 obstacle impact and shatters it (+300 pts)
    OVERDRIVE = 2, // 5.0s speed boost + invulnerable obstacle smash (+200 pts)
    SLOW_MO   = 3, // 6.0s time dilation (0.65x obstacle speed)
    MAGNET    = 4  // 8.0s magnetic pull attracting all nearby Data Orbs
};

// =============================================================================
// VERSIONED SAVE DATA WITH CRC32 INTEGRITY CHECK
// =============================================================================

struct SaveData {
    static constexpr uint32_t MAGIC   = 0x56454C4F; // "VELO"
    static constexpr uint32_t VERSION = 2;

    uint32_t magic{MAGIC};
    uint32_t version{VERSION};
    int32_t  high_score{0};
    int32_t  best_score_time_attack{0};
    int32_t  best_score_hardcore{0};
    int32_t  total_orbs{0};
    int32_t  best_combo{1};
    int32_t  total_runs{0};
    int32_t  total_dodges{0};
    uint32_t unlocked_skins_mask{0x01}; // Bit 0 = CYBER_CYAN unlocked
    int32_t  selected_skin{0};
    int32_t  selected_mode{0};
    uint8_t  sfx_enabled{1};
    uint8_t  music_enabled{1};
    uint8_t  haptics_enabled{1};
    uint8_t  reserved{0};
    uint32_t checksum{0};

    uint32_t compute_checksum() const {
        const auto* bytes = reinterpret_cast<const uint8_t*>(this);
        const size_t len = offsetof(SaveData, checksum);
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= static_cast<uint32_t>(bytes[i]);
            for (int b = 0; b < 8; ++b) {
                crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1u)));
            }
        }
        return ~crc;
    }

    void seal() {
        magic = MAGIC;
        version = VERSION;
        checksum = compute_checksum();
    }

    bool is_valid() const {
        return magic == MAGIC && version == VERSION && checksum == compute_checksum();
    }

    bool is_skin_unlocked(int skin_idx) const {
        if (skin_idx <= 0 || skin_idx >= static_cast<int>(SphereSkin::COUNT)) return skin_idx == 0;
        return (unlocked_skins_mask & (1u << static_cast<uint32_t>(skin_idx))) != 0;
    }

    bool try_unlock_skin(int skin_idx) {
        if (skin_idx < 0 || skin_idx >= static_cast<int>(SphereSkin::COUNT)) return false;
        if (is_skin_unlocked(skin_idx)) return true;
        int cost = SKIN_PALETTES[static_cast<size_t>(skin_idx)].unlock_cost;
        if (total_orbs >= cost) {
            total_orbs -= cost;
            unlocked_skins_mask |= (1u << static_cast<uint32_t>(skin_idx));
            seal();
            return true;
        }
        return false;
    }

    int get_best_for_mode(GameMode mode) const {
        switch (mode) {
            case GameMode::TIME_ATTACK: return best_score_time_attack;
            case GameMode::HARDCORE:    return best_score_hardcore;
            case GameMode::ARCADE:
            default:                    return high_score;
        }
    }

    bool update_best_for_mode(GameMode mode, int score) {
        bool updated = false;
        if (score > high_score) {
            high_score = score;
            updated = true;
        }
        if (mode == GameMode::TIME_ATTACK && score > best_score_time_attack) {
            best_score_time_attack = score;
            updated = true;
        } else if (mode == GameMode::HARDCORE && score > best_score_hardcore) {
            best_score_hardcore = score;
            updated = true;
        }
        if (updated) seal();
        return updated;
    }
};

// =============================================================================
// PATH SEGMENT (OBSTACLES, DATA ORBS & POWER-UPS)
// =============================================================================

struct PathSegment {
    float z_position{0.0f};
    bool  has_obstacle{false};
    TrackLane lane{TrackLane::CENTER};
    TrackLane motion_target_lane{TrackLane::CENTER};
    ObstacleType obstacle_type{ObstacleType::STATIC_BLOCK};

    float obstacle_x_offset{0.0f};
    float obstacle_width{1.60f};
    float obstacle_height{1.50f};
    float obstacle_depth{1.00f};

    bool  is_moving{false};
    float motion_phase{0.0f};
    float motion_warning_time{0.0f};
    int   motion_direction{1};

    float hover_y{0.0f};
    bool  cleared{false};

    // Collectible Data Orb
    bool      has_orb{false};
    bool      orb_collected{false};
    TrackLane orb_lane{TrackLane::CENTER};
    Vec3      orb_pos{0.0f, 0.85f, 0.0f};

    // Collectible Power-Up
    PowerUpType powerup_type{PowerUpType::NONE};
    bool        powerup_collected{false};
    TrackLane   powerup_lane{TrackLane::CENTER};
    Vec3        powerup_pos{0.0f, 1.05f, 0.0f};
};

// =============================================================================
// PLAYER SPHERE PHYSICS (STEERING, BANKING, JUMPING & SLAMMING)
// =============================================================================

class PlayerBall {
public:
    static constexpr float RADIUS            = 0.70f;
    static constexpr float TRACK_HALF_WIDTH  = 3.8f;
    static constexpr float LATERAL_BOUND     = TRACK_HALF_WIDTH - RADIUS - 0.2f;
    static constexpr float STEER_RESPONSE    = 18.0f;
    static constexpr float TOUCH_SENSITIVITY = 7.0f;
    static constexpr float JUMP_VELOCITY     = 11.8f;
    static constexpr float SLAM_VELOCITY     = -24.0f;
    static constexpr float GRAVITY           = 32.0f;

    Vec3  pos{0.0f, RADIUS, 0.0f};
    float target_x{0.0f};
    float roll_angle{0.0f};
    float lateral_roll_angle{0.0f};
    float tilt_angle{0.0f};
    float lateral_velocity{0.0f};
    float bounce_offset_y{0.0f};

    // Jump & vertical dynamics
    float jump_y{0.0f};
    float vertical_velocity{0.0f};
    bool  is_airborne{false};

    // Post-shield-break or overdrive invulnerability timer
    float invulnerable_timer{0.0f};

    // Collision tumble state
    bool  is_tumbling{false};
    Vec3  tumble_pos{0.0f, RADIUS, 0.0f};
    Vec3  tumble_vel{0.0f, 0.0f, 0.0f};
    Vec3  tumble_rot{0.0f, 0.0f, 0.0f};
    Vec3  tumble_ang_vel{0.0f, 0.0f, 0.0f};

    void reset() {
        pos = {0.0f, RADIUS, 0.0f};
        target_x = 0.0f;
        roll_angle = 0.0f;
        lateral_roll_angle = 0.0f;
        tilt_angle = 0.0f;
        lateral_velocity = 0.0f;
        bounce_offset_y = 0.0f;
        jump_y = 0.0f;
        vertical_velocity = 0.0f;
        is_airborne = false;
        invulnerable_timer = 0.0f;
        is_tumbling = false;
        tumble_pos = {0.0f, RADIUS, 0.0f};
        tumble_vel = {0.0f, 0.0f, 0.0f};
        tumble_rot = {0.0f, 0.0f, 0.0f};
        tumble_ang_vel = {0.0f, 0.0f, 0.0f};
    }

    bool jump() {
        if (is_tumbling || is_airborne) return false;
        is_airborne = true;
        vertical_velocity = JUMP_VELOCITY;
        return true;
    }

    bool slam() {
        if (is_tumbling || !is_airborne) return false;
        if (vertical_velocity > SLAM_VELOCITY * 0.5f) {
            vertical_velocity = SLAM_VELOCITY;
            return true;
        }
        return false;
    }

    void update(float dt, float forward_speed) {
        if (invulnerable_timer > 0.0f) {
            invulnerable_timer = std::max(0.0f, invulnerable_timer - dt);
        }

        if (is_tumbling) {
            update_tumble(dt);
            return;
        }

        float diff = target_x - pos.x;
        float prev_x = pos.x;
        pos.x += diff * std::min(1.0f, STEER_RESPONSE * dt);

        if (pos.x < -LATERAL_BOUND) pos.x = -LATERAL_BOUND;
        if (pos.x >  LATERAL_BOUND) pos.x =  LATERAL_BOUND;

        float dx = pos.x - prev_x;
        lateral_velocity = (dt > 0.0001f) ? (dx / dt) : 0.0f;

        roll_angle += (forward_speed / RADIUS) * dt;
        if (roll_angle > 2.0f * PI) {
            roll_angle -= 2.0f * PI;
        }

        lateral_roll_angle -= (dx / RADIUS);
        if (lateral_roll_angle > 2.0f * PI)  lateral_roll_angle -= 2.0f * PI;
        if (lateral_roll_angle < -2.0f * PI) lateral_roll_angle += 2.0f * PI;

        float target_tilt = -std::clamp(lateral_velocity * 0.035f, -0.45f, 0.45f);
        tilt_angle += (target_tilt - tilt_angle) * std::min(1.0f, 16.0f * dt);

        if (is_airborne) {
            jump_y += vertical_velocity * dt;
            vertical_velocity -= GRAVITY * dt;
            if (jump_y <= 0.0f) {
                jump_y = 0.0f;
                vertical_velocity = 0.0f;
                is_airborne = false;
            }
            bounce_offset_y = 0.0f;
        } else {
            bounce_offset_y = std::sin(roll_angle * 4.0f) * 0.02f * std::min(1.0f, forward_speed / 22.0f);
        }

        pos.y = RADIUS + jump_y + bounce_offset_y;
    }

    void trigger_collision() {
        is_tumbling = true;
        tumble_pos = pos;
        tumble_vel = {-lateral_velocity * 0.4f, 7.0f, -5.5f};
        tumble_rot = {roll_angle, 0.0f, tilt_angle + lateral_roll_angle};
        tumble_ang_vel = {12.0f, 6.0f, -9.0f};
    }

    void update_tumble(float dt) {
        tumble_pos += tumble_vel * dt;
        tumble_vel.y -= 24.0f * dt;

        if (tumble_pos.y < RADIUS) {
            tumble_pos.y = RADIUS;
            tumble_vel.y = -tumble_vel.y * 0.45f;
            tumble_vel.x *= 0.70f;
            tumble_vel.z *= 0.75f;
        }

        tumble_rot += tumble_ang_vel * dt;
        tumble_ang_vel *= std::max(0.0f, 1.0f - 1.8f * dt);
    }

    void apply_touch_delta(float delta_x_screenspace, float touch_sensitivity = TOUCH_SENSITIVITY) {
        target_x += delta_x_screenspace * touch_sensitivity;
        target_x = std::clamp(target_x, -LATERAL_BOUND, LATERAL_BOUND);
    }

    void steer_to_lane(int lane_delta) {
        constexpr float step = (TRACK_HALF_WIDTH * 2.0f) / 3.0f;
        target_x = std::clamp(target_x + static_cast<float>(lane_delta) * step, -LATERAL_BOUND, LATERAL_BOUND);
    }
};

// =============================================================================
// TREADMILL SYSTEM (SEGMENTS, POWER-UPS, ORBS, COMBOS & MODES)
// =============================================================================

class TreadmillSystem {
public:
    static constexpr size_t POOL_SIZE           = 8;
    static constexpr float  SEGMENT_SPACING     = 10.0f;
    static constexpr float  MIN_OBSTACLE_GAP    = 20.0f;
    static constexpr float  MOVING_SAFETY_GAP   = 30.0f;
    static constexpr float  DESPAWN_THRESHOLD_Z = -10.0f;
    static constexpr float  BASE_SPEED          = 18.0f;
    static constexpr float  HARDCORE_BASE_SPEED = 28.0f;
    static constexpr float  SPEED_ACCELERATION  = 0.55f;
    static constexpr float  MAX_SPEED           = 55.0f;
    static constexpr float  TIME_ATTACK_START   = 90.0f;

    static constexpr float LANE_WIDTH    = (PlayerBall::TRACK_HALF_WIDTH * 2.0f) / 3.0f;
    static constexpr float LEFT_LANE_X   = -LANE_WIDTH;
    static constexpr float CENTER_LANE_X = 0.0f;
    static constexpr float RIGHT_LANE_X  = LANE_WIDTH;

    static constexpr float OBSTACLE_WIDTH  = LANE_WIDTH;
    static constexpr float OBSTACLE_HEIGHT = LANE_WIDTH;
    static constexpr float OBSTACLE_DEPTH  = LANE_WIDTH;

    static constexpr float MOVING_HALF_CYCLE_TIME = 1.10f;
    static constexpr float MOVING_WARNING_TIME    = 0.35f;

    static constexpr float MOVING_DIFFICULTY_START_SPEED = 26.0f;
    static constexpr float MOVING_DIFFICULTY_END_SPEED   = 55.0f;
    static constexpr float MOVING_MIN_SPAWN_PROBABILITY  = 0.40f;
    static constexpr float MOVING_MAX_SPAWN_PROBABILITY  = 0.58f;

    TreadmillSystem()
        : rng_(std::random_device{}()),
          dist_prob_(0.0f, 1.0f) {
        reset();
    }

    void set_game_mode(GameMode mode) {
        game_mode_ = mode;
    }

    [[nodiscard]] GameMode get_game_mode() const { return game_mode_; }

    void reset() {
        base_speed_ = (game_mode_ == GameMode::HARDCORE) ? HARDCORE_BASE_SPEED : BASE_SPEED;
        global_speed_ = base_speed_;
        distance_traveled_ = 0.0f;
        score_ = 0;
        bonus_score_ = 0;
        dodge_count_ = 0;
        dodge_milestone_ = 0;
        dodge_milestone_timer_ = 0.0f;
        has_new_best_ = false;
        red_pattern_index_ = 0;

        // Combo & collectibles
        combo_ = 1;
        best_run_combo_ = 1;
        dodge_streak_ = 0;
        near_miss_count_ = 0;
        run_orbs_ = 0;

        // Power-up state
        shield_active_ = false;
        active_powerup_ = PowerUpType::NONE;
        powerup_timer_ = 0.0f;
        powerup_duration_ = 1.0f;
        orb_spin_angle_ = 0.0f;

        // Mode state
        time_remaining_ = TIME_ATTACK_START;
        time_expired_ = false;

        // Floating HUD alert banner
        std::memset(banner_text_, 0, sizeof(banner_text_));
        banner_timer_ = 0.0f;

        for (size_t i = 0; i < POOL_SIZE; ++i) {
            segments_[i].z_position = static_cast<float>(i) * SEGMENT_SPACING;
            segments_[i].obstacle_width = OBSTACLE_WIDTH;
            segments_[i].obstacle_height = OBSTACLE_HEIGHT;
            segments_[i].obstacle_depth = OBSTACLE_DEPTH;
            segments_[i].obstacle_type = ObstacleType::STATIC_BLOCK;
            segments_[i].is_moving = false;
            segments_[i].motion_phase = 0.0f;
            segments_[i].motion_warning_time = 0.0f;
            segments_[i].motion_direction = 1;
            segments_[i].hover_y = 0.0f;
            segments_[i].cleared = false;
            segments_[i].has_orb = false;
            segments_[i].orb_collected = false;
            segments_[i].powerup_type = PowerUpType::NONE;
            segments_[i].powerup_collected = false;

            if (i < 2) {
                segments_[i].has_obstacle = false;
                segments_[i].lane = TrackLane::CENTER;
                segments_[i].motion_target_lane = TrackLane::CENTER;
                segments_[i].obstacle_x_offset = CENTER_LANE_X;
            } else {
                setup_segment(segments_[i]);
            }
        }
    }

    void trigger_banner(const char* msg, float duration = 1.15f) {
        if (!msg) return;
        std::snprintf(banner_text_, sizeof(banner_text_), "%s", msg);
        banner_timer_ = duration;
    }

    void activate_powerup(PowerUpType type) {
        if (type == PowerUpType::SHIELD) {
            shield_active_ = true;
            trigger_banner("SHIELD ACTIVE!", 1.2f);
            return;
        }
        active_powerup_ = type;
        switch (type) {
            case PowerUpType::OVERDRIVE:
                powerup_duration_ = 5.0f;
                powerup_timer_ = 5.0f;
                trigger_banner("OVERDRIVE BOOST!", 1.2f);
                break;
            case PowerUpType::SLOW_MO:
                powerup_duration_ = 6.0f;
                powerup_timer_ = 6.0f;
                trigger_banner("CHRONO SLOW-MO!", 1.2f);
                break;
            case PowerUpType::MAGNET:
                powerup_duration_ = 8.0f;
                powerup_timer_ = 8.0f;
                trigger_banner("ORB MAGNET!", 1.2f);
                break;
            default:
                break;
        }
    }

    bool consume_shield() {
        if (!shield_active_) return false;
        shield_active_ = false;
        bonus_score_ += 300 * get_mode_multiplier();
        trigger_banner("+300 SHIELD SMASH!", 1.3f);
        return true;
    }

    void reset_combo() {
        combo_ = 1;
        dodge_streak_ = 0;
    }

    void update(float dt, const Vec3& player_pos = {0.0f, PlayerBall::RADIUS, 0.0f}) {
        orb_spin_angle_ += 3.2f * dt;
        if (orb_spin_angle_ > 2.0f * PI) orb_spin_angle_ -= 2.0f * PI;

        if (banner_timer_ > 0.0f) {
            banner_timer_ = std::max(0.0f, banner_timer_ - dt);
        }

        // Power-up countdown
        if (active_powerup_ != PowerUpType::NONE) {
            powerup_timer_ -= dt;
            if (powerup_timer_ <= 0.0f) {
                powerup_timer_ = 0.0f;
                active_powerup_ = PowerUpType::NONE;
            }
        }

        // Time Attack countdown
        if (game_mode_ == GameMode::TIME_ATTACK) {
            time_remaining_ = std::max(0.0f, time_remaining_ - dt);
            if (time_remaining_ <= 0.0f) {
                time_expired_ = true;
            }
        }

        float accel = (game_mode_ == GameMode::HARDCORE) ? (SPEED_ACCELERATION * 1.35f) : SPEED_ACCELERATION;
        base_speed_ = std::min(MAX_SPEED, base_speed_ + accel * dt);

        // Effective speed accounts for Overdrive (1.35x) or Slow-Mo (0.65x)
        float speed_mod = 1.0f;
        if (active_powerup_ == PowerUpType::OVERDRIVE) {
            speed_mod = 1.35f;
        } else if (active_powerup_ == PowerUpType::SLOW_MO) {
            speed_mod = 0.68f;
        }
        global_speed_ = std::clamp(base_speed_ * speed_mod, 12.0f, MAX_SPEED * 1.25f);

        const float z_disp = global_speed_ * dt;
        distance_traveled_ += z_disp;

        const int mode_mult = get_mode_multiplier();

        for (auto& segment : segments_) {
            segment.z_position -= z_disp;
            segment.orb_pos.z = segment.z_position;
            segment.powerup_pos.z = segment.z_position;

            // Magnet attraction pulls uncollected orbs directly toward player
            if (segment.has_orb && !segment.orb_collected &&
                active_powerup_ == PowerUpType::MAGNET &&
                segment.z_position > -2.0f && segment.z_position < 16.0f) {
                Vec3 dir = (player_pos - segment.orb_pos);
                float dist = dir.length();
                if (dist > 0.05f) {
                    segment.orb_pos += (dir / dist) * (28.0f * dt);
                }
            }

            if (segment.has_obstacle && !segment.cleared && segment.z_position < -PlayerBall::RADIUS) {
                segment.cleared = true;
                ++dodge_count_;
                ++dodge_streak_;

                // Update combo every 3 clean dodges (up to 5x)
                combo_ = std::min(5, 1 + (dodge_streak_ / 3));
                if (combo_ > best_run_combo_) best_run_combo_ = combo_;

                // Check for near-miss bonus (close lateral pass or jumping cleanly over a low barrier)
                float lat_dist = std::fabs(player_pos.x - segment.obstacle_x_offset);
                bool jumped_over = (segment.obstacle_type == ObstacleType::LOW_BARRIER &&
                                    lat_dist < segment.obstacle_width * 0.65f &&
                                    player_pos.y > segment.obstacle_height);
                bool close_shave = (lat_dist < (segment.obstacle_width * 0.5f + PlayerBall::RADIUS + 0.55f));

                if (jumped_over || close_shave) {
                    ++near_miss_count_;
                    int nm_pts = 200 * combo_ * mode_mult;
                    bonus_score_ += nm_pts;
                    if (game_mode_ == GameMode::TIME_ATTACK) {
                        time_remaining_ = std::min(99.0f, time_remaining_ + 1.5f);
                    }
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "NEAR MISS +%d!", nm_pts);
                    trigger_banner(buf, 0.95f);
                } else {
                    bonus_score_ += 100 * combo_ * mode_mult;
                    if (game_mode_ == GameMode::TIME_ATTACK) {
                        time_remaining_ = std::min(99.0f, time_remaining_ + 0.4f);
                    }
                }

                if (dodge_count_ % 25 == 0) {
                    dodge_milestone_ = dodge_count_;
                    dodge_milestone_timer_ = 1.2f;
                }
            }

            if (segment.is_moving && segment.has_obstacle) {
                if (segment.motion_warning_time > 0.0f) {
                    segment.motion_warning_time =
                        std::max(0.0f, segment.motion_warning_time - dt);
                    segment.obstacle_x_offset = lane_x(segment.lane);
                    continue;
                }

                float phase_speed = (active_powerup_ == PowerUpType::SLOW_MO) ? 0.65f : 1.0f;
                segment.motion_phase += (dt * phase_speed) / MOVING_HALF_CYCLE_TIME;
                if (segment.motion_phase >= 2.0f) {
                    segment.motion_phase -= 2.0f;
                }

                const float cycle = segment.motion_phase;
                const float half_cycle = std::floor(cycle);
                const float local_t = cycle - half_cycle;
                const float eased_t =
                    local_t * local_t * (3.0f - 2.0f * local_t);

                TrackLane second_lane;
                TrackLane third_lane;

                if (segment.lane == TrackLane::LEFT) {
                    second_lane = TrackLane::RIGHT;
                    third_lane = TrackLane::LEFT;
                } else if (segment.lane == TrackLane::RIGHT) {
                    second_lane = TrackLane::LEFT;
                    third_lane = TrackLane::RIGHT;
                } else if (segment.motion_direction < 0) {
                    second_lane = TrackLane::LEFT;
                    third_lane = TrackLane::RIGHT;
                } else {
                    second_lane = TrackLane::RIGHT;
                    third_lane = TrackLane::LEFT;
                }

                const float start_x = lane_x(segment.lane);
                const float second_x = lane_x(second_lane);
                const float third_x = lane_x(third_lane);

                if (cycle < 1.0f) {
                    segment.obstacle_x_offset =
                        start_x + (second_x - start_x) * eased_t;
                } else {
                    segment.obstacle_x_offset =
                        second_x + (third_x - second_x) * eased_t;
                }
            }
        }

        float speed_ratio = (base_speed_ - BASE_SPEED) / (MAX_SPEED - BASE_SPEED);
        float speed_mult = 1.0f + std::clamp(speed_ratio, 0.0f, 1.0f) * 1.5f;
        score_ = static_cast<int>(distance_traveled_ * 1.5f * speed_mult * static_cast<float>(mode_mult)) + bonus_score_;

        if (best_score_ > 0 && score_ > best_score_) {
            has_new_best_ = true;
        }

        if (dodge_milestone_timer_ > 0.0f) {
            dodge_milestone_timer_ = std::max(0.0f, dodge_milestone_timer_ - dt);
        }

        for (auto& segment : segments_) {
            if (segment.z_position < DESPAWN_THRESHOLD_Z) {
                float max_z = -1000.0f;
                for (const auto& other : segments_) {
                    if (&other != &segment && other.z_position > max_z) {
                        max_z = other.z_position;
                    }
                }

                segment.z_position = max_z + SEGMENT_SPACING;
                setup_segment(segment);
            }
        }
    }

    void collect_orb(PathSegment& seg) {
        if (!seg.has_orb || seg.orb_collected) return;
        seg.orb_collected = true;
        int mult = get_mode_multiplier();
        run_orbs_ += mult;
        bonus_score_ += 50 * combo_ * mult;
        if (game_mode_ == GameMode::TIME_ATTACK) {
            time_remaining_ = std::min(99.0f, time_remaining_ + 1.5f);
        }
    }

    void smash_obstacle(PathSegment& seg) {
        seg.has_obstacle = false;
        seg.cleared = true;
        ++dodge_count_;
        bonus_score_ += 200 * combo_ * get_mode_multiplier();
    }

    void apply_time_penalty(float seconds) {
        if (game_mode_ == GameMode::TIME_ATTACK) {
            time_remaining_ = std::max(0.0f, time_remaining_ - seconds);
            if (time_remaining_ <= 0.0f) {
                time_expired_ = true;
            }
        }
    }

    [[nodiscard]] int get_mode_multiplier() const {
        return (game_mode_ == GameMode::HARDCORE) ? 2 : 1;
    }

    [[nodiscard]] size_t get_biome_index() const {
        return static_cast<size_t>(distance_traveled_ / 450.0f) % BIOME_PALETTES.size();
    }

    [[nodiscard]] const std::array<PathSegment, POOL_SIZE>& get_segments() const { return segments_; }
    [[nodiscard]] std::array<PathSegment, POOL_SIZE>& get_segments_mut() { return segments_; }
    [[nodiscard]] float get_global_speed() const { return global_speed_; }
    [[nodiscard]] float get_distance_traveled() const { return distance_traveled_; }
    [[nodiscard]] int   get_score() const { return score_; }
    [[nodiscard]] int   get_best_score() const { return best_score_; }
    [[nodiscard]] bool  has_new_best() const { return has_new_best_; }
    [[nodiscard]] int   get_dodge_count() const { return dodge_count_; }
    [[nodiscard]] int   get_dodge_milestone() const { return dodge_milestone_; }
    [[nodiscard]] float get_dodge_milestone_timer() const { return dodge_milestone_timer_; }
    [[nodiscard]] int   get_combo() const { return combo_; }
    [[nodiscard]] int   get_best_run_combo() const { return best_run_combo_; }
    [[nodiscard]] int   get_near_miss_count() const { return near_miss_count_; }
    [[nodiscard]] int   get_run_orbs() const { return run_orbs_; }
    [[nodiscard]] bool  is_shield_active() const { return shield_active_; }
    [[nodiscard]] PowerUpType get_active_powerup() const { return active_powerup_; }
    [[nodiscard]] float get_powerup_timer() const { return powerup_timer_; }
    [[nodiscard]] float get_powerup_progress() const {
        return (powerup_duration_ > 0.0f) ? std::clamp(powerup_timer_ / powerup_duration_, 0.0f, 1.0f) : 0.0f;
    }
    [[nodiscard]] float get_orb_spin_angle() const { return orb_spin_angle_; }
    [[nodiscard]] float get_time_remaining() const { return time_remaining_; }
    [[nodiscard]] bool  is_time_expired() const { return time_expired_; }
    [[nodiscard]] const char* get_banner_text() const { return banner_text_; }
    [[nodiscard]] float get_banner_timer() const { return banner_timer_; }

    void set_best_score(int b) { best_score_ = b; }

    static float lane_x(TrackLane lane) {
        switch (lane) {
            case TrackLane::LEFT:   return LEFT_LANE_X;
            case TrackLane::RIGHT:  return RIGHT_LANE_X;
            case TrackLane::CENTER:
            default:                return CENTER_LANE_X;
        }
    }

private:
    static TrackLane red_pattern_lane(size_t index) {
        static constexpr std::array<TrackLane, 8> RED_PATTERN = {{
            TrackLane::LEFT,
            TrackLane::CENTER,
            TrackLane::RIGHT,
            TrackLane::CENTER,
            TrackLane::RIGHT,
            TrackLane::CENTER,
            TrackLane::LEFT,
            TrackLane::CENTER
        }};
        return RED_PATTERN[index % RED_PATTERN.size()];
    }

    void setup_segment(PathSegment& seg) {
        seg.cleared = false;
        seg.hover_y = 0.0f;
        seg.motion_phase = 0.0f;
        seg.motion_warning_time = 0.0f;
        seg.motion_direction = 1;
        seg.obstacle_width = OBSTACLE_WIDTH;
        seg.obstacle_height = OBSTACLE_HEIGHT;
        seg.obstacle_depth = OBSTACLE_DEPTH;
        seg.obstacle_type = ObstacleType::STATIC_BLOCK;
        seg.has_orb = false;
        seg.orb_collected = false;
        seg.powerup_type = PowerUpType::NONE;
        seg.powerup_collected = false;

        bool obstacle_too_close = false;
        for (const auto& other : segments_) {
            if (&other == &seg || !other.has_obstacle) continue;
            const float gap = std::fabs(other.z_position - seg.z_position);
            if (gap < MIN_OBSTACLE_GAP) {
                obstacle_too_close = true;
                break;
            }
        }

        seg.has_obstacle = !obstacle_too_close && (dist_prob_(rng_) > 0.28f);

        if (!seg.has_obstacle) {
            seg.lane = TrackLane::CENTER;
            seg.motion_target_lane = TrackLane::CENTER;
            seg.obstacle_x_offset = CENTER_LANE_X;
            seg.is_moving = false;

            // Spawn collectible orb or occasional power-up on open segments
            float roll = dist_prob_(rng_);
            if (roll < 0.55f) {
                int orb_l = static_cast<int>(dist_prob_(rng_) * 3.0f);
                seg.orb_lane = static_cast<TrackLane>(std::min(2, orb_l));
                seg.has_orb = true;
                seg.orb_pos = {lane_x(seg.orb_lane), 0.85f, seg.z_position};
            } else if (roll < 0.68f && distance_traveled_ > 80.0f) {
                int pu_l = static_cast<int>(dist_prob_(rng_) * 3.0f);
                seg.powerup_lane = static_cast<TrackLane>(std::min(2, pu_l));
                float p_roll = dist_prob_(rng_);
                if (game_mode_ != GameMode::HARDCORE && p_roll < 0.30f && !shield_active_) {
                    seg.powerup_type = PowerUpType::SHIELD;
                } else if (p_roll < 0.55f) {
                    seg.powerup_type = PowerUpType::MAGNET;
                } else if (p_roll < 0.80f) {
                    seg.powerup_type = PowerUpType::SLOW_MO;
                } else {
                    seg.powerup_type = PowerUpType::OVERDRIVE;
                }
                seg.powerup_pos = {lane_x(seg.powerup_lane), 1.05f, seg.z_position};
            }
            return;
        }

        bool moving_space_clear = true;
        for (const auto& other : segments_) {
            if (&other == &seg || !other.has_obstacle) continue;
            if (std::fabs(other.z_position - seg.z_position) < MOVING_SAFETY_GAP) {
                moving_space_clear = false;
                break;
            }
        }

        const float difficulty_t = std::clamp(
            (base_speed_ - MOVING_DIFFICULTY_START_SPEED) /
                (MOVING_DIFFICULTY_END_SPEED - MOVING_DIFFICULTY_START_SPEED),
            0.0f, 1.0f);

        const float moving_spawn_probability =
            MOVING_MIN_SPAWN_PROBABILITY +
            (MOVING_MAX_SPAWN_PROBABILITY - MOVING_MIN_SPAWN_PROBABILITY) * difficulty_t;

        if (moving_space_clear &&
            base_speed_ > MOVING_DIFFICULTY_START_SPEED &&
            dist_prob_(rng_) < moving_spawn_probability) {
            seg.is_moving = true;
            seg.obstacle_type = ObstacleType::MOVING_SWEEPER;

            const int start_lane = static_cast<int>(dist_prob_(rng_) * 3.0f);
            seg.lane = static_cast<TrackLane>(std::min(2, start_lane));

            if (seg.lane == TrackLane::LEFT) {
                seg.motion_direction = 1;
                seg.motion_target_lane = TrackLane::RIGHT;
                seg.obstacle_x_offset = LEFT_LANE_X;
            } else if (seg.lane == TrackLane::CENTER) {
                seg.motion_direction = (dist_prob_(rng_) < 0.5f) ? -1 : 1;
                seg.motion_target_lane =
                    (seg.motion_direction < 0) ? TrackLane::LEFT : TrackLane::RIGHT;
                seg.obstacle_x_offset = CENTER_LANE_X;
            } else {
                seg.motion_direction = -1;
                seg.motion_target_lane = TrackLane::LEFT;
                seg.obstacle_x_offset = RIGHT_LANE_X;
            }

            seg.motion_phase = 0.0f;
            seg.motion_warning_time = MOVING_WARNING_TIME;
        } else {
            seg.is_moving = false;
            seg.lane = red_pattern_lane(red_pattern_index_++);
            seg.motion_target_lane = seg.lane;
            seg.obstacle_x_offset = lane_x(seg.lane);

            // Introduce Low Laser Barriers (jumpable) and Overhead Gates (roll under)
            float type_roll = dist_prob_(rng_);
            if (distance_traveled_ > 60.0f && type_roll < 0.26f) {
                seg.obstacle_type = ObstacleType::LOW_BARRIER;
                seg.obstacle_height = 0.72f;
                seg.obstacle_depth = 0.70f;
                seg.hover_y = 0.0f;
                // Place an incentive Data Orb directly above the low barrier for jump mastery!
                seg.has_orb = true;
                seg.orb_lane = seg.lane;
                seg.orb_pos = {seg.obstacle_x_offset, 1.95f, seg.z_position};
            } else if (distance_traveled_ > 140.0f && type_roll < 0.44f) {
                seg.obstacle_type = ObstacleType::OVERHEAD_GATE;
                seg.obstacle_height = 1.15f;
                seg.obstacle_depth = 0.85f;
                seg.hover_y = 1.52f; // Bottom of gate is at y = 1.52f, safe for grounded ball (top = 1.40f)
                // Place an orb underneath the overhead gate
                seg.has_orb = true;
                seg.orb_lane = seg.lane;
                seg.orb_pos = {seg.obstacle_x_offset, 0.75f, seg.z_position};
            } else {
                seg.obstacle_type = ObstacleType::STATIC_BLOCK;
                // Occasionally spawn an orb in an adjacent safe lane
                if (dist_prob_(rng_) < 0.48f) {
                    TrackLane safe_lane = (seg.lane == TrackLane::CENTER)
                        ? ((dist_prob_(rng_) < 0.5f) ? TrackLane::LEFT : TrackLane::RIGHT)
                        : TrackLane::CENTER;
                    seg.has_orb = true;
                    seg.orb_lane = safe_lane;
                    seg.orb_pos = {lane_x(safe_lane), 0.85f, seg.z_position};
                }
            }
        }
    }

    std::array<PathSegment, POOL_SIZE> segments_{};
    GameMode    game_mode_{GameMode::ARCADE};
    float       base_speed_{BASE_SPEED};
    float       global_speed_{BASE_SPEED};
    float       distance_traveled_{0.0f};
    int         score_{0};
    int         bonus_score_{0};
    int         best_score_{0};
    int         dodge_count_{0};
    int         dodge_milestone_{0};
    float       dodge_milestone_timer_{0.0f};
    bool        has_new_best_{false};

    int         combo_{1};
    int         best_run_combo_{1};
    int         dodge_streak_{0};
    int         near_miss_count_{0};
    int         run_orbs_{0};

    bool        shield_active_{false};
    PowerUpType active_powerup_{PowerUpType::NONE};
    float       powerup_timer_{0.0f};
    float       powerup_duration_{1.0f};
    float       orb_spin_angle_{0.0f};

    float       time_remaining_{TIME_ATTACK_START};
    bool        time_expired_{false};

    char        banner_text_[32]{};
    float       banner_timer_{0.0f};

    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_prob_;
    size_t red_pattern_index_{0};
};

// =============================================================================
// SPHERE-AABB 3D COLLISION & PICKUP RESOLUTION SYSTEM
// =============================================================================

struct InteractionOutcome {
    bool orb_picked_up{false};
    bool powerup_picked_up{false};
    bool obstacle_smashed{false};
    bool shield_broken{false};
    bool lethal_collision{false};
    Vec3 impact_pos{};
};

class CollisionSystem {
public:
    static bool check_obstacle_aabb(const Vec3& center, float radius, const PathSegment& seg) {
        if (!seg.has_obstacle) return false;
        if (seg.z_position < -3.0f || seg.z_position > 3.0f) return false;

        float hx = seg.obstacle_width * 0.5f;
        float hy = seg.obstacle_height * 0.5f;
        float hz = seg.obstacle_depth * 0.5f;

        Vec3 box_center = {seg.obstacle_x_offset, hy + seg.hover_y, seg.z_position};

        float cx = std::clamp(center.x, box_center.x - hx, box_center.x + hx);
        float cy = std::clamp(center.y, box_center.y - hy, box_center.y + hy);
        float cz = std::clamp(center.z, box_center.z - hz, box_center.z + hz);

        float dx = center.x - cx;
        float dy = center.y - cy;
        float dz = center.z - cz;

        return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
    }

    static bool check_collision(const PlayerBall& ball, const TreadmillSystem& treadmill) {
        if (ball.invulnerable_timer > 0.0f) return false;
        const Vec3& center = ball.pos;
        const float radius = PlayerBall::RADIUS;
        const auto& segments = treadmill.get_segments();

        return std::any_of(segments.begin(), segments.end(), [&](const PathSegment& seg) {
            return check_obstacle_aabb(center, radius, seg);
        });
    }

    static InteractionOutcome resolve_interactions(PlayerBall& ball, TreadmillSystem& treadmill) {
        InteractionOutcome out{};
        const Vec3& center = ball.pos;
        const float radius = PlayerBall::RADIUS;
        auto& segments = treadmill.get_segments_mut();

        for (auto& seg : segments) {
            // 1. Check Data Orb pickup
            if (seg.has_orb && !seg.orb_collected) {
                float dist_sq = Vec3::distance_sq(center, seg.orb_pos);
                float pickup_r = radius + 0.65f;
                if (dist_sq <= pickup_r * pickup_r) {
                    treadmill.collect_orb(seg);
                    out.orb_picked_up = true;
                }
            }

            // 2. Check Power-Up pickup
            if (seg.powerup_type != PowerUpType::NONE && !seg.powerup_collected) {
                float dist_sq = Vec3::distance_sq(center, seg.powerup_pos);
                float pickup_r = radius + 0.75f;
                if (dist_sq <= pickup_r * pickup_r) {
                    seg.powerup_collected = true;
                    treadmill.activate_powerup(seg.powerup_type);
                    out.powerup_picked_up = true;
                }
            }

            // 3. Check Obstacle collision
            if (seg.has_obstacle && check_obstacle_aabb(center, radius, seg)) {
                out.impact_pos = {seg.obstacle_x_offset, seg.hover_y + seg.obstacle_height * 0.5f, seg.z_position};

                if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
                    treadmill.smash_obstacle(seg);
                    out.obstacle_smashed = true;
                } else if (treadmill.consume_shield()) {
                    seg.has_obstacle = false;
                    seg.cleared = true;
                    ball.invulnerable_timer = 1.2f;
                    out.shield_broken = true;
                } else if (ball.invulnerable_timer > 0.0f) {
                    seg.has_obstacle = false;
                    seg.cleared = true;
                } else {
                    out.lethal_collision = true;
                    break;
                }
            }
        }

        return out;
    }
};

} // namespace velo
