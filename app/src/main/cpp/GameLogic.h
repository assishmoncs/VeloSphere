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

    // Moving-hazard state. Motion is deterministic: hold in the source lane,
    // make one smooth lane change, then stay locked in the destination lane.
    bool  is_moving{false};
    float motion_distance{0.0f};

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

    // Moving hazards follow a distance-based, deterministic lane change.
    // This keeps the motion independent of frame rate and still predictable
    // at higher game speeds.
    static constexpr float MOVING_HOLD_DISTANCE       = 5.0f;
    static constexpr float MOVING_TRANSITION_DISTANCE = 8.0f;

    TreadmillSystem()
        : rng_(std::random_device{}()),
          dist_lane_(0, 2),
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
            segments_[i].obstacle_width = OBSTACLE_WIDTH;
            segments_[i].obstacle_height = OBSTACLE_HEIGHT;
            segments_[i].obstacle_depth = OBSTACLE_DEPTH;
            segments_[i].is_moving = false;
            segments_[i].motion_distance = 0.0f;
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

        float speed_ratio = (global_speed_ - BASE_SPEED) / (MAX_SPEED - BASE_SPEED);
        float speed_mult = 1.0f + std::clamp(speed_ratio, 0.0f, 1.0f) * 1.5f;
        score_ = static_cast<int>(distance_traveled_ * 1.5f * speed_mult) + bonus_score_;

        if (best_score_ > 0 && score_ > best_score_) {
            has_new_best_ = true;
        }

        for (auto& segment : segments_) {
            segment.z_position -= z_disp;

            if (segment.has_obstacle && !segment.cleared && segment.z_position < -PlayerBall::RADIUS) {
                segment.cleared = true;
                bonus_score_ += 100;
                last_bonus_display_timer_ = 1.2f;
            }

            segment.hover_y = 0.0f;

            if (segment.is_moving && segment.has_obstacle) {
                segment.motion_distance += z_disp;

                const float from_x = lane_x(segment.lane);
                const float target_x = lane_x(segment.motion_target_lane);

                if (segment.motion_distance <= MOVING_HOLD_DISTANCE) {
                    segment.obstacle_x_offset = from_x;
                } else {
                    const float transition_progress =
                        (segment.motion_distance - MOVING_HOLD_DISTANCE) / MOVING_TRANSITION_DISTANCE;

                    if (transition_progress >= 1.0f) {
                        // Once the lane change is complete, lock the hazard
                        // into the destination lane before it reaches the player.
                        segment.obstacle_x_offset = target_x;
                    } else {
                        const float t = std::clamp(transition_progress, 0.0f, 1.0f);
                        // Smoothstep gives a deliberate, readable acceleration/deceleration.
                        const float eased_t = t * t * (3.0f - 2.0f * t);
                        segment.obstacle_x_offset =
                            from_x + (target_x - from_x) * eased_t;
                    }
                }
            }
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

    static TrackLane moving_target_for(TrackLane lane) {
        // Moving hazards only cross one lane boundary at a time.
        switch (lane) {
            case TrackLane::LEFT:  return TrackLane::CENTER;
            case TrackLane::RIGHT: return TrackLane::CENTER;
            case TrackLane::CENTER:
            default:               return TrackLane::RIGHT;
        }
    }

    void setup_segment(PathSegment& seg) {
        seg.has_obstacle = (dist_prob_(rng_) > 0.30f);
        seg.cleared = false;
        seg.hover_y = 0.0f;
        seg.motion_distance = 0.0f;
        seg.obstacle_width = OBSTACLE_WIDTH;
        seg.obstacle_height = OBSTACLE_HEIGHT;
        seg.obstacle_depth = OBSTACLE_DEPTH;

        if (!seg.has_obstacle) {
            seg.lane = TrackLane::CENTER;
            seg.motion_target_lane = TrackLane::CENTER;
            seg.obstacle_x_offset = CENTER_LANE_X;
            seg.is_moving = false;
            return;
        }

        seg.lane = static_cast<TrackLane>(dist_lane_(rng_));
        seg.obstacle_x_offset = lane_x(seg.lane);

        // Moving hazards appear later in the run, but their movement itself
        // is completely deterministic after they spawn.
        if (global_speed_ > 28.0f && dist_prob_(rng_) > 0.58f) {
            seg.is_moving = true;
            seg.motion_target_lane = moving_target_for(seg.lane);
        } else {
            seg.is_moving = false;
            seg.motion_target_lane = seg.lane;
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
    std::uniform_int_distribution<int> dist_lane_;
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
