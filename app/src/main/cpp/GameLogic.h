#pragma once

#include <array>
#include <random>
#include <algorithm>
#include <cmath>
#include "Math3D.h"

namespace velo {

enum class GameState {
    READY,
    PLAYING,
    GAME_OVER
};

// The runner has exactly three playable lanes. Obstacles may only use these
// lane centers; moving hazards transition between adjacent lanes only.
enum class TrackLane : int {
    LEFT = 0,
    CENTER = 1,
    RIGHT = 2
};

struct PathSegment {
    float z_position{0.0f};
    bool  has_obstacle{false};
    TrackLane lane{TrackLane::CENTER};
    TrackLane motion_target_lane{TrackLane::CENTER};

    // Obstacles have fixed dimensions so a single obstacle always belongs to
    // exactly one lane.
    float obstacle_x_offset{0.0f};
    float obstacle_width{1.60f};
    float obstacle_height{1.50f};
    float obstacle_depth{1.00f};

    // Moving-hazard state. Yellow hazards smoothly travel between their two
    // assigned lane endpoints and continuously reverse direction.
    bool  is_moving{false};
    float motion_phase{0.0f};
    float motion_warning_time{0.0f};

    // Kept as a rendering/collision compatibility field; obstacle vertical
    // position is intentionally fixed at ground level.
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
    float roll_angle{0.0f};
    float lateral_roll_angle{0.0f};
    float tilt_angle{0.0f};
    float lateral_velocity{0.0f};
    float bounce_offset_y{0.0f};

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

        float diff = target_x - pos.x;
        float prev_x = pos.x;
        pos.x += diff * std::min(1.0f, 20.0f * dt);

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

    void apply_touch_delta(float delta_x_screenspace, float touch_sensitivity = 7.5f) {
        target_x += delta_x_screenspace * touch_sensitivity;
        target_x = std::clamp(target_x, -LATERAL_BOUND, LATERAL_BOUND);
    }
};

class TreadmillSystem {
public:
    static constexpr size_t POOL_SIZE           = 8;
    static constexpr float  SEGMENT_SPACING     = 10.0f;
    static constexpr float  MIN_OBSTACLE_GAP    = 20.0f;
    static constexpr float  DESPAWN_THRESHOLD_Z = -10.0f;
    static constexpr float  BASE_SPEED          = 18.0f;
    static constexpr float  SPEED_ACCELERATION  = 0.55f;
    static constexpr float  MAX_SPEED           = 55.0f;

    // Three equal-width lanes across the existing track.
    static constexpr float LANE_WIDTH = (PlayerBall::TRACK_HALF_WIDTH * 2.0f) / 3.0f;
    static constexpr float LEFT_LANE_X = -LANE_WIDTH;
    static constexpr float CENTER_LANE_X = 0.0f;
    static constexpr float RIGHT_LANE_X = LANE_WIDTH;

    // Fixed obstacle geometry.
    static constexpr float OBSTACLE_WIDTH  = 1.60f;
    static constexpr float OBSTACLE_HEIGHT = 1.50f;
    static constexpr float OBSTACLE_DEPTH  = 1.00f;

    // One-way travel time between the two lane endpoints for a moving
    // (yellow) hazard. The motion loops continuously and uses smoothstep.
    static constexpr float MOVING_HALF_CYCLE_TIME = 1.10f;
    static constexpr float MOVING_WARNING_TIME = 0.35f;

    TreadmillSystem()
        : rng_(std::random_device{}()),
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
        red_pattern_index_ = 0;

        for (size_t i = 0; i < POOL_SIZE; ++i) {
            segments_[i].z_position = static_cast<float>(i) * SEGMENT_SPACING;
            segments_[i].obstacle_width = OBSTACLE_WIDTH;
            segments_[i].obstacle_height = OBSTACLE_HEIGHT;
            segments_[i].obstacle_depth = OBSTACLE_DEPTH;
            segments_[i].is_moving = false;
            segments_[i].motion_phase = 0.0f;
            segments_[i].motion_warning_time = 0.0f;
            segments_[i].hover_y = 0.0f;
            segments_[i].cleared = false;

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

    void update(float dt) {
        global_speed_ = std::min(MAX_SPEED, global_speed_ + SPEED_ACCELERATION * dt);

        const float z_disp = global_speed_ * dt;
        distance_traveled_ += z_disp;

        for (auto& segment : segments_) {
            segment.z_position -= z_disp;

            if (segment.has_obstacle && !segment.cleared && segment.z_position < -PlayerBall::RADIUS) {
                segment.cleared = true;
                bonus_score_ += 100;
                last_bonus_display_timer_ = 1.2f;
            }

            segment.hover_y = 0.0f;

            if (segment.is_moving && segment.has_obstacle) {
                // Brief warning hold at the left edge before the sweep begins.
                if (segment.motion_warning_time > 0.0f) {
                    segment.motion_warning_time =
                        std::max(0.0f, segment.motion_warning_time - dt);
                    segment.obstacle_x_offset = LEFT_LANE_X;
                    continue;
                }

                segment.motion_phase += dt / MOVING_HALF_CYCLE_TIME;
                if (segment.motion_phase >= 2.0f) {
                    segment.motion_phase -= 2.0f;
                }

                // Yellow hazards sweep across the entire three-lane track:
                // LEFT -> CENTER -> RIGHT -> CENTER -> LEFT ...
                const float from_x = LEFT_LANE_X;
                const float target_x = RIGHT_LANE_X;

                // Ping-pong between the two outer lane endpoints:
                // 0 -> 1 -> 0, with smooth acceleration/deceleration.
                const float cycle = segment.motion_phase;
                const float progress = (cycle <= 1.0f) ? cycle : (2.0f - cycle);
                const float eased_t = progress * progress * (3.0f - 2.0f * progress);

                segment.obstacle_x_offset =
                    from_x + (target_x - from_x) * eased_t;
            }
        }

        // Calculate score after obstacle clearance so +100 dodge bonuses
        // are reflected immediately in the same update frame.
        float speed_ratio = (global_speed_ - BASE_SPEED) / (MAX_SPEED - BASE_SPEED);
        float speed_mult = 1.0f + std::clamp(speed_ratio, 0.0f, 1.0f) * 1.5f;
        score_ = static_cast<int>(distance_traveled_ * 1.5f * speed_mult) + bonus_score_;

        if (best_score_ > 0 && score_ > best_score_) {
            has_new_best_ = true;
        }

        if (last_bonus_display_timer_ > 0.0f) {
            last_bonus_display_timer_ -= dt;
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

    [[nodiscard]] const std::array<PathSegment, POOL_SIZE>& get_segments() const { return segments_; }
    [[nodiscard]] float get_global_speed() const { return global_speed_; }
    [[nodiscard]] int   get_score() const { return score_; }
    [[nodiscard]] int   get_best_score() const { return best_score_; }
    [[nodiscard]] bool  has_new_best() const { return has_new_best_; }
    [[nodiscard]] float get_bonus_timer() const { return last_bonus_display_timer_; }

    void set_best_score(int b) { best_score_ = b; }

private:
    static float lane_x(TrackLane lane) {
        switch (lane) {
            case TrackLane::LEFT:   return LEFT_LANE_X;
            case TrackLane::RIGHT:  return RIGHT_LANE_X;
            case TrackLane::CENTER:
            default:                return CENTER_LANE_X;
        }
    }

    static TrackLane red_pattern_lane(size_t index) {
        // Stationary red obstacles use a simple, repeatable weave. The
        // center lane appears regularly, giving the player readable choices
        // instead of arbitrary lane jumps.
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
        seg.obstacle_width = OBSTACLE_WIDTH;
        seg.obstacle_height = OBSTACLE_HEIGHT;
        seg.obstacle_depth = OBSTACLE_DEPTH;

        // Keep at least one full segment clear between hazards. This gives the
        // player a consistent reaction window instead of back-to-back obstacles.
        bool obstacle_too_close = false;

        for (const auto& other : segments_) {
            if (&other == &seg || !other.has_obstacle) continue;

            const float gap = std::fabs(other.z_position - seg.z_position);
            if (gap < MIN_OBSTACLE_GAP) {
                obstacle_too_close = true;
                break;
            }
        }

        seg.has_obstacle = !obstacle_too_close && (dist_prob_(rng_) > 0.30f);

        if (!seg.has_obstacle) {
            seg.lane = TrackLane::CENTER;
            seg.motion_target_lane = TrackLane::CENTER;
            seg.obstacle_x_offset = CENTER_LANE_X;
            seg.is_moving = false;
            return;
        }

        // A full-track yellow sweep needs more breathing room than a
        // stationary red obstacle. Do not spawn a moving hazard too close
        // to any other obstacle.
        bool moving_space_clear = true;
        for (const auto& other : segments_) {
            if (&other == &seg || !other.has_obstacle) continue;
            if (std::fabs(other.z_position - seg.z_position) < MOVING_SAFETY_GAP) {
                moving_space_clear = false;
                break;
            }
        }

        // Only the hazards selected as moving are yellow.
        // Their movement spans the full track from left lane to right lane.
        if (moving_space_clear && global_speed_ > 28.0f && dist_prob_(rng_) > 0.58f) {
            seg.is_moving = true;
            seg.lane = TrackLane::LEFT;
            seg.motion_target_lane = TrackLane::RIGHT;
            seg.obstacle_x_offset = LEFT_LANE_X;
            seg.motion_warning_time = MOVING_WARNING_TIME;
        } else {
            seg.is_moving = false;
            seg.lane = red_pattern_lane(red_pattern_index_++);
            seg.motion_target_lane = seg.lane;
            seg.obstacle_x_offset = lane_x(seg.lane);
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
    std::uniform_real_distribution<float> dist_prob_;
    size_t red_pattern_index_{0};
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

            return (dx * dx + dy * dy + dz * dz) <= radius_sq;
        });
    }
};

} // namespace velo
