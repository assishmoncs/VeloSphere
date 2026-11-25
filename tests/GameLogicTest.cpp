#include <iostream>
#include <cassert>
#include <cmath>
#include "Math3D.h"
#include "GameLogic.h"

using namespace velo;

namespace {

constexpr float EPS = 1e-4f;

bool approx_eq(float a, float b, float tol = EPS) {
    return std::fabs(a - b) <= tol;
}

void test_math3d() {
    Vec3 a{3.0f, 4.0f, 0.0f};
    assert(approx_eq(a.length(), 5.0f));
    Vec3 n = a.normalized();
    assert(approx_eq(n.x, 0.6f) && approx_eq(n.y, 0.8f));

    Vec3 x_axis{1.0f, 0.0f, 0.0f};
    Vec3 y_axis{0.0f, 1.0f, 0.0f};
    Vec3 z_axis = Vec3::cross(x_axis, y_axis);
    assert(approx_eq(z_axis.z, 1.0f));

    Mat4 tr = Mat4::translate(2.0f, -3.0f, 5.0f);
    Vec3 pt = tr.transform_point({1.0f, 1.0f, 1.0f});
    assert(approx_eq(pt.x, 3.0f) && approx_eq(pt.y, -2.0f) && approx_eq(pt.z, 6.0f));

    Color3 c = Color3::lerp({0.0f, 0.0f, 0.0f}, {1.0f, 0.5f, 0.25f}, 0.5f);
    assert(approx_eq(c.r, 0.5f) && approx_eq(c.g, 0.25f) && approx_eq(c.b, 0.125f));

    Rect2D r{10.0f, 20.0f, 100.0f, 50.0f};
    assert(r.contains(50.0f, 40.0f));
    assert(!r.contains(5.0f, 40.0f));

    std::cout << "[PASS] Math3D vectors, matrices, Color3::lerp & Rect2D\n";
}

void test_player_ball_physics_jump_buffer_and_lanes() {
    PlayerBall ball;
    ball.reset();
    assert(!ball.is_airborne);
    assert(approx_eq(ball.pos.y, PlayerBall::RADIUS));

    // Jump initiation & stretch
    assert(ball.jump());
    assert(ball.is_airborne);
    assert(ball.scale_y > 1.1f);

    // Advance in air
    ball.update(0.10f, 20.0f);
    assert(ball.pos.y > PlayerBall::RADIUS + 0.5f);

    // Mid-air slam accelerates descent
    assert(ball.slam());
    // Buffer a jump right before landing -> should auto-jump on touchdown
    assert(!ball.jump()); // returns false while airborne, sets jump_buffer_timer
    assert(ball.jump_buffer_timer > 0.0f);

    for (int i = 0; i < 6; ++i) {
        ball.update(0.02f, 20.0f);
    }
    // Should have landed and immediately triggered the buffered jump!
    assert(ball.is_airborne);

    // Lane-snap steering
    ball.reset();
    ball.steer_to_lane(1);
    assert(ball.current_lane == 2);
    assert(ball.target_x > 1.5f);
    ball.steer_to_lane(-2);
    assert(ball.current_lane == 0);
    assert(ball.target_x < -1.5f);

    // Lateral steering bounds clamping
    ball.apply_touch_delta(100.0f);
    for (int i = 0; i < 30; ++i) {
        ball.update(0.02f, 20.0f);
    }
    assert(ball.pos.x <= PlayerBall::LATERAL_BOUND + EPS);

    std::cout << "[PASS] PlayerBall jump buffering, slam, squash/stretch & lane-snap\n";
}

void test_collision_and_dual_lane_obstacles() {
    PlayerBall ball;
    TreadmillSystem treadmill;
    ball.reset();
    treadmill.reset();

    // Low Laser Barrier jump clearance
    PathSegment low_seg{};
    low_seg.has_obstacle = true;
    low_seg.z_position = 0.0f;
    low_seg.obstacle_x_offset = 0.0f;
    low_seg.obstacle_width = TreadmillSystem::OBSTACLE_WIDTH;
    low_seg.obstacle_height = 0.72f;
    low_seg.obstacle_depth = 0.70f;
    low_seg.hover_y = 0.0f;
    low_seg.obstacle_type = ObstacleType::LOW_BARRIER;

    assert(CollisionSystem::check_obstacle_aabb(ball.pos, PlayerBall::RADIUS, low_seg));
    Vec3 airborne_pos{0.0f, PlayerBall::RADIUS + 1.15f, 0.0f};
    assert(!CollisionSystem::check_obstacle_aabb(airborne_pos, PlayerBall::RADIUS, low_seg));

    // Overhead Gate clearance when grounded vs airborne
    PathSegment gate_seg{};
    gate_seg.has_obstacle = true;
    gate_seg.z_position = 0.0f;
    gate_seg.obstacle_x_offset = 0.0f;
    gate_seg.obstacle_width = TreadmillSystem::OBSTACLE_WIDTH;
    gate_seg.obstacle_height = 1.15f;
    gate_seg.obstacle_depth = 0.85f;
    gate_seg.hover_y = 1.52f;
    gate_seg.obstacle_type = ObstacleType::OVERHEAD_GATE;

    assert(!CollisionSystem::check_obstacle_aabb(ball.pos, PlayerBall::RADIUS, gate_seg));
    assert(CollisionSystem::check_obstacle_aabb(airborne_pos, PlayerBall::RADIUS, gate_seg));

    // Dual-Lane Wall (LEFT + RIGHT blocked, CENTER safe)
    PathSegment dual_seg{};
    dual_seg.has_obstacle = true;
    dual_seg.z_position = 0.0f;
    dual_seg.obstacle_x_offset = TreadmillSystem::LEFT_LANE_X;
    dual_seg.has_second_obstacle = true;
    dual_seg.second_obstacle_x = TreadmillSystem::RIGHT_LANE_X;
    dual_seg.obstacle_width = TreadmillSystem::OBSTACLE_WIDTH;
    dual_seg.obstacle_height = TreadmillSystem::OBSTACLE_HEIGHT;
    dual_seg.obstacle_depth = TreadmillSystem::OBSTACLE_DEPTH;

    assert(!CollisionSystem::check_obstacle_aabb({0.0f, PlayerBall::RADIUS, 0.0f}, PlayerBall::RADIUS, dual_seg));
    assert(CollisionSystem::check_obstacle_aabb({TreadmillSystem::LEFT_LANE_X, PlayerBall::RADIUS, 0.0f}, PlayerBall::RADIUS, dual_seg));
    assert(CollisionSystem::check_obstacle_aabb({TreadmillSystem::RIGHT_LANE_X, PlayerBall::RADIUS, 0.0f}, PlayerBall::RADIUS, dual_seg));

    std::cout << "[PASS] Low Barrier, Overhead Gate & Dual-Lane Wall 3D collisions\n";
}

void test_powerups_fever_and_revive() {
    PlayerBall ball;
    TreadmillSystem treadmill;
    ball.reset();
    treadmill.reset();

    // Activate Shield and verify absorption
    treadmill.activate_powerup(PowerUpType::SHIELD);
    assert(treadmill.is_shield_active());

    auto& segs = treadmill.get_segments_mut();
    segs[0].has_obstacle = true;
    segs[0].z_position = 0.0f;
    segs[0].obstacle_x_offset = 0.0f;
    segs[0].obstacle_width = TreadmillSystem::OBSTACLE_WIDTH;
    segs[0].obstacle_height = TreadmillSystem::OBSTACLE_HEIGHT;
    segs[0].obstacle_depth = TreadmillSystem::OBSTACLE_DEPTH;
    segs[0].hover_y = 0.0f;

    InteractionOutcome outcome = CollisionSystem::resolve_interactions(ball, treadmill);
    assert(outcome.shield_broken);
    assert(!outcome.lethal_collision);
    assert(!treadmill.is_shield_active());
    assert(ball.invulnerable_timer > 0.0f);

    // Test Hyperflow / Fever charge and 2x multiplier stacking
    assert(!treadmill.is_fever_active());
    bool triggered = false;
    for (int i = 0; i < 6; ++i) {
        if (treadmill.add_fever_charge(0.20f)) triggered = true;
    }
    assert(triggered);
    assert(treadmill.is_fever_active());
    assert(treadmill.get_effective_multiplier() == 2);

    // Test Magnet orb pull
    treadmill.activate_powerup(PowerUpType::MAGNET);
    segs[1].has_orb = true;
    segs[1].orb_collected = false;
    segs[1].z_position = 6.0f;
    segs[1].orb_pos = {TreadmillSystem::RIGHT_LANE_X, 0.85f, 6.0f};
    float initial_x = segs[1].orb_pos.x;
    treadmill.update(0.10f, ball.pos);
    assert(segs[1].orb_pos.x < initial_x);

    // Test Revive clearing nearby obstacles & one-time use limit
    assert(treadmill.can_revive());
    segs[2].has_obstacle = true;
    segs[2].z_position = 12.0f;
    treadmill.clear_nearby_for_revive();
    assert(!segs[2].has_obstacle);
    assert(!treadmill.can_revive());

    std::cout << "[PASS] Power-Ups, Hyperflow Fever 2x & Revive system\n";
}

void test_save_v3_upgrades_missions_and_v2_migration() {
    SaveData save{};
    save.high_score = 18500;
    save.total_orbs = 600;
    save.seal();
    assert(save.is_valid());

    // Unlock skin 5 (PHANTOM GOLD, 500 orbs)
    assert(!save.is_skin_unlocked(5));
    assert(save.try_unlock_skin(5));
    assert(save.is_skin_unlocked(5));
    assert(save.total_orbs == 100);
    assert(save.is_valid());

    // Buy Aegis Capacitor upgrades up to Lv3 (40 + 80 + 140 = 260 orbs)
    save.total_orbs = 300;
    assert(save.try_buy_upgrade(UpgradeType::SHIELD_CAP)); // Lv1
    assert(save.try_buy_upgrade(UpgradeType::SHIELD_CAP)); // Lv2
    assert(save.try_buy_upgrade(UpgradeType::SHIELD_CAP)); // Lv3
    assert(save.get_upgrade_level(UpgradeType::SHIELD_CAP) == 3);
    assert(save.total_orbs == 40);

    // Verify Lv3 Aegis Capacitor grants starting shield in Arcade mode
    TreadmillSystem tm;
    tm.set_game_mode(GameMode::ARCADE);
    tm.apply_upgrades(save);
    tm.reset();
    assert(tm.is_shield_active());

    // Advance default mission 0 (COLLECT 15 ORBS) -> completes & awards orbs!
    int orbs_before = save.total_orbs;
    int rewarded = save.advance_missions(15, 0, 0, 0, 1, 0);
    assert(rewarded > 0);
    assert(save.total_orbs == orbs_before + rewarded);
    assert(save.missions_completed == 1);

    // Tamper detection
    save.high_score = 999999;
    assert(!save.is_valid());

    // Verify v2 -> v3 seamless binary migration
    SaveDataV2 old_v2{};
    old_v2.high_score = 42000;
    old_v2.total_orbs = 275;
    old_v2.unlocked_skins_mask = 0x07;
    old_v2.checksum = old_v2.compute_checksum();
    assert(old_v2.is_valid());

    SaveData migrated = SaveData::migrate_from_v2(old_v2);
    assert(migrated.is_valid());
    assert(migrated.high_score == 42000);
    assert(migrated.total_orbs == 275);
    assert(migrated.is_skin_unlocked(2));

    std::cout << "[PASS] SaveData v3, Tech Upgrades, Missions & v2->v3 migration\n";
}

} // namespace

int main() {
    std::cout << "=== Running VeloSphere 3.0 Native C++17 Unit Tests ===\n";
    test_math3d();
    test_player_ball_physics_jump_buffer_and_lanes();
    test_collision_and_dual_lane_obstacles();
    test_powerups_fever_and_revive();
    test_save_v3_upgrades_missions_and_v2_migration();
    std::cout << "=== ALL TESTS PASSED ===\n";
    return 0;
}
