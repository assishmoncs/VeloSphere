#pragma once

#include <array>
#include <random>
#include <algorithm>
#include <cmath>
#include "Math3D.h"

namespace velo {

enum class GameState {
    READY,      // Waiting for first touch/tap to start
    PLAYING,    // Active running, accelerating treadmill
    GAME_OVER   // Collided, showing game-over state until tap to restart
};

struct PathSegment {
    float z_position{0.0f};
    bool  has_obstacle{false};
    float obstacle_x_offset{0.0f};
    float obstacle_width{1.8f};
    float obstacle_height{1.5f};
    float obstacle_depth{1.0f};

    // Dynamic obstacle behavior
    bool  is_moving{false};
    float move_speed{0.0f};
    float move_phase{0.0f};
    float base_x{0.0f};
    float hover_phase{0.0f};
    float hover_y{0.0f};
    bool  cleared{false};
};

class PlayerBall {
public:
    static constexpr float RADIUS = 0.70f;
    static constexpr float TRACK_HALF_WIDTH = 3.8f;
    static constexpr float LATERAL_BOUND = TRACK_HALF_WIDTH - RADIUS - 0.2f;

    Vec3  pos{0.0f, RADIUS, 0.0f};
    float target_x{0.0f};
    float roll_angle{0.0f};         // Forward rolling rotation (X-axis)
    float lateral_roll_angle{0.0f}; // Sideways rolling rotation (Z-axis)
    float tilt_angle{0.0f};         // Dynamic banking tilt (Z-axis lean into turns)
    float lateral_velocity{0.0f};
    float bounce_offset_y{0.0f};

    // Impact tumble physics
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
        is_tumbling = false;
        tumble_pos = {0.0f, RADIUS, 0.0f};
        tumble_vel = {0.0f, 0.0f, 0.0f};
        tumble_rot = {0.0f, 0.0f, 0.0f};
        tumble_ang_vel = {0.0f, 0.0f, 0.0f};
    }

    void update(float dt, float forward_speed) {
        if (is_tumbling) {
            update_tumble(dt);
            return;
        }

        // Smooth responsive lerp towards target lateral position with inertia
        float diff = target_x - pos.x;
        float prev_x = pos.x;
        pos.x += diff * std::min(1.0f, 20.0f * dt);

        // Clamp to track boundaries
        if (pos.x < -LATERAL_BOUND) pos.x = -LATERAL_BOUND;
        if (pos.x >  LATERAL_BOUND) pos.x =  LATERAL_BOUND;

        float dx = pos.x - prev_x;
        lateral_velocity = (dt > 0.0001f) ? (dx / dt) : 0.0f;

        // Forward roll rotation matching linear treadmill motion
        roll_angle += (forward_speed / RADIUS) * dt;
        if (roll_angle > 2.0f * PI) {
            roll_angle -= 2.0f * PI;
        }

        // Realistic lateral roll: rolling around Z axis as sphere moves sideways
        lateral_roll_angle -= (dx / RADIUS);
        if (lateral_roll_angle > 2.0f * PI)  lateral_roll_angle -= 2.0f * PI;
        if (lateral_roll_angle < -2.0f * PI) lateral_roll_angle += 2.0f * PI;

        // Banking tilt: marble/bike banking dynamically into the steering turn
        float target_tilt = -std::clamp(lateral_velocity * 0.035f, -0.45f, 0.45f);
        tilt_angle += (target_tilt - tilt_angle) * std::min(1.0f, 16.0f * dt);

        // High-speed contact micro-suspension bounce
        bounce_offset_y = std::sin(roll_angle * 4.0f) * 0.02f * std::min(1.0f, forward_speed / 22.0f);
        pos.y = RADIUS + bounce_offset_y;
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
        tumble_vel.y -= 24.0f * dt; // gravity

        if (tumble_pos.y < RADIUS) {
            tumble_pos.y = RADIUS;
            tumble_vel.y = -tumble_vel.y * 0.45f; // elastic ground bounce
            tumble_vel.x *= 0.70f;
            tumble_vel.z *= 0.75f;
        }

        tumble_rot += tumble_ang_vel * dt;
        tumble_ang_vel *= std::max(0.0f, 1.0f - 1.8f * dt); // angular drag
    }

    void apply_touch_delta(float delta_x_screenspace, float touch_sensitivity = 7.5f) {
        target_x += delta_x_screenspace * touch_sensitivity;
        target_x = std::clamp(target_x, -LATERAL_BOUND, LATERAL_BOUND);
    }
};

class TreadmillSystem {
public:
    static constexpr size_t POOL_SIZE           = 8;
    static constexpr float  SEGMENT_SPACING     = 10.0f;
    static constexpr float  DESPAWN_THRESHOLD_Z = -10.0f;
    static constexpr float  BASE_SPEED          = 18.0f;
    static constexpr float  SPEED_ACCELERATION  = 0.55f;
    static constexpr float  MAX_SPEED           = 55.0f;

    TreadmillSystem()
        : rng_(std::random_device{}()),
          dist_lane_(-2.2f, 2.2f),
          dist_prob_(0.0f, 1.0f) {
        reset();
    }

    void reset() {
        global_speed_ = BASE_SPEED;
        distance_traveled_ = 0.0f;
        score_ = 0;
        bonus_score_ = 0;
        last_bonus_display_timer_ = 0.0f;
        has_new_best_ = false;

        for (size_t i = 0; i < POOL_SIZE; ++i) {
            segments_[i].z_position = static_cast<float>(i) * SEGMENT_SPACING;
            segments_[i].obstacle_width = 1.8f;
            segments_[i].obstacle_height = 1.5f;
            segments_[i].obstacle_depth = 1.0f;
            segments_[i].is_moving = false;
            segments_[i].cleared = false;
            segments_[i].hover_phase = 0.0f;
            segments_[i].hover_y = 0.0f;

            if (i < 2) {
                // Clear runway at spawn
                segments_[i].has_obstacle = false;
                segments_[i].obstacle_x_offset = 0.0f;
            } else {
                setup_segment(segments_[i]);
            }
        }
    }

    void update(float dt) {
        // Accelerate global speed over time using Delta Time
        global_speed_ = std::min(MAX_SPEED, global_speed_ + SPEED_ACCELERATION * dt);

        const float z_disp = global_speed_ * dt;
        distance_traveled_ += z_disp;

        // Dynamic speed scoring multiplier (1.0x at BASE_SPEED up to 2.5x at MAX_SPEED)
        float speed_ratio = (global_speed_ - BASE_SPEED) / (MAX_SPEED - BASE_SPEED);
        float speed_mult = 1.0f + std::clamp(speed_ratio, 0.0f, 1.0f) * 1.5f;
        score_ = static_cast<int>(distance_traveled_ * 1.5f * speed_mult) + bonus_score_;

        if (best_score_ > 0 && score_ > best_score_) {
            has_new_best_ = true;
        }

        // Translate segments along Z-axis toward stationary camera
        for (auto& segment : segments_) {
            segment.z_position -= z_disp;

            // Near-miss / obstacle clearance bonus (+100 points)
            if (segment.has_obstacle && !segment.cleared && segment.z_position < -PlayerBall::RADIUS) {
                segment.cleared = true;
                bonus_score_ += 100;
                last_bonus_display_timer_ = 1.2f;
            }

            // Update moving obstacles with realistic hovering & horizontal oscillation
            if (segment.is_moving) {
                segment.move_phase += segment.move_speed * dt;
                segment.obstacle_x_offset = segment.base_x + std::sin(segment.move_phase) * 1.3f;
                segment.obstacle_x_offset = std::clamp(segment.obstacle_x_offset, -2.5f, 2.5f);

                segment.hover_phase += 4.0f * dt;
                segment.hover_y = std::sin(segment.hover_phase) * 0.12f;
            } else {
                segment.hover_y = 0.0f;
            }
        }

        if (last_bonus_display_timer_ > 0.0f) {
            last_bonus_display_timer_ -= dt;
        }

        // Teleportation / Recycle check
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

    [[nodiscard]] const std::array<PathSegment, POOL_SIZE>& get_segments() const { return segments_; }
    [[nodiscard]] float get_global_speed() const { return global_speed_; }
    [[nodiscard]] int   get_score() const { return score_; }
    [[nodiscard]] int   get_best_score() const { return best_score_; }
    [[nodiscard]] bool  has_new_best() const { return has_new_best_; }
    [[nodiscard]] float get_bonus_timer() const { return last_bonus_display_timer_; }

    void set_best_score(int b) { best_score_ = b; }

private:
    void setup_segment(PathSegment& seg) {
        seg.has_obstacle = (dist_prob_(rng_) > 0.35f);
        seg.base_x = dist_lane_(rng_);
        seg.obstacle_x_offset = seg.base_x;
        seg.cleared = false;
        seg.hover_phase = dist_prob_(rng_) * 6.28f;
        seg.hover_y = 0.0f;

        // Size variation
        seg.obstacle_width = 1.6f + dist_prob_(rng_) * 0.6f;
        seg.obstacle_height = 1.4f + dist_prob_(rng_) * 0.4f;
        seg.obstacle_depth = 1.0f;

        // Fast speeds unlock moving hazards
        if (global_speed_ > 28.0f && dist_prob_(rng_) > 0.6f) {
            seg.is_moving = true;
            seg.move_speed = 2.0f + dist_prob_(rng_) * 2.0f;
            seg.move_phase = dist_prob_(rng_) * 6.28f;
        } else {
            seg.is_moving = false;
        }
    }

    std::array<PathSegment, POOL_SIZE> segments_{};
    float global_speed_{BASE_SPEED};
    float distance_traveled_{0.0f};
    int   score_{0};
    int   bonus_score_{0};
    int   best_score_{0};
    float last_bonus_display_timer_{0.0f};
    bool  has_new_best_{false};

    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_lane_;
    std::uniform_real_distribution<float> dist_prob_;
};

// =============================================================================
// SPHERE-AABB 3D COLLISION SYSTEM
// =============================================================================

class CollisionSystem {
public:
    static bool check_collision(const PlayerBall& ball, const TreadmillSystem& treadmill) {
        const Vec3& center = ball.pos;
        const float radius = PlayerBall::RADIUS;
        const float radius_sq = radius * radius;
        const auto& segments = treadmill.get_segments();

        return std::any_of(segments.begin(), segments.end(), [&](const PathSegment& seg) {
            if (!seg.has_obstacle) return false;

            // Only check segments near the player (player is at Z = 0)
            if (seg.z_position < -3.0f || seg.z_position > 3.0f) return false;

            float hx = seg.obstacle_width * 0.5f;
            float hy = seg.obstacle_height * 0.5f;
            float hz = seg.obstacle_depth * 0.5f;

            // Obstacle world center: Y = hy + hover_y (sitting on track plane with hover oscillation)
            Vec3 box_center = {seg.obstacle_x_offset, hy + seg.hover_y, seg.z_position};

            // Find closest point on AABB to sphere center
            float cx = std::clamp(center.x, box_center.x - hx, box_center.x + hx);
            float cy = std::clamp(center.y, box_center.y - hy, box_center.y + hy);
            float cz = std::clamp(center.z, box_center.z - hz, box_center.z + hz);

            float dx = center.x - cx;
            float dy = center.y - cy;
            float dz = center.z - cz;

            return (dx * dx + dy * dy + dz * dz) <= radius_sq;
        });
    }
};

} // namespace velo
