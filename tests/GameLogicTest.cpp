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

    Vec3 z_axis = Vec3::cross({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    assert(approx_eq(z_axis.z, 1.0f));

    Mat4 tr = Mat4::translate(2.0f, -3.0f, 5.0f);
    Vec3 pt = tr.transform_point({1.0f, 1.0f, 1.0f});
    assert(approx_eq(pt.x, 3.0f) && approx_eq(pt.y, -2.0f) && approx_eq(pt.z, 6.0f));

    Color3 c = Color3::lerp({0.0f, 0.0f, 0.0f}, {1.0f, 0.5f, 0.25f}, 0.5f);
    assert(approx_eq(c.r, 0.5f) && approx_eq(c.g, 0.25f) && approx_eq(c.b, 0.125f));

    Rect2D r{10.0f, 20.0f, 100.0f, 50.0f};
    assert(r.contains(50.0f, 40.0f) && !r.contains(5.0f, 40.0f));

    std::cout << "[PASS] Math3D vectors, matrices, Color3::lerp & Rect2D\n";
}

void test_player_gyro_jump_buffer_and_phase_dash() {
    PlayerBall ball;
    ball.reset(3); // Hyper Thruster Lv3 reduces dash cooldown
    assert(!ball.is_airborne);
    assert(approx_eq(ball.pos.y, PlayerBall::RADIUS));

    // Jump + 360 flip + input buffer
    assert(ball.jump());
    assert(ball.is_airborne);
    ball.update(0.10f, 22.0f);
    assert(ball.jump_flip_angle > 0.5f);
    assert(ball.gyro_outer_angle > 0.0f);
    assert(ball.gyro_inner_angle < 0.0f);

    assert(ball.slam());
    assert(!ball.jump()); // buffers jump while airborne
    for (int i = 0; i < 6; ++i) ball.update(0.02f, 22.0f);
    assert(ball.is_airborne); // buffered jump fired on landing

    // Phase-Dash activation & cooldown
    ball.reset(3);
    assert(ball.get_dash_ready_ratio() >= 0.99f);
    assert(ball.trigger_phase_dash());
    assert(ball.is_phase_dashing());
    assert(!ball.trigger_phase_dash()); // cannot re-trigger while active/cooling

    std::cout << "[PASS] PlayerBall gyro rings, 360 jump flip, buffer & Phase-Dash\n";
}

void test_hazards_phase_dodge_and_world_curve() {
    PlayerBall ball;
    TreadmillSystem treadmill;
    ball.reset();
    treadmill.reset();

    // World curve toggles cleanly
    Vec2 flat = treadmill.get_world_curve(false);
    assert(approx_eq(flat.x, 0.0f) && approx_eq(flat.y, 0.0f));
    Vec2 curved = treadmill.get_world_curve(true);
    assert(curved.y < 0.0f);

    // Laser Spinner jump clearance
    PathSegment spin_seg{};
    spin_seg.has_obstacle = true;
    spin_seg.obstacle_type = ObstacleType::LASER_SPINNER;
    spin_seg.z_position = 0.0f;
    spin_seg.obstacle_x_offset = 0.0f;
    spin_seg.obstacle_width = TreadmillSystem::LANE_WIDTH * 1.10f;
    spin_seg.obstacle_height = 0.82f;
    spin_seg.obstacle_depth = 0.72f;
    spin_seg.hover_y = 0.0f;
    assert(CollisionSystem::check_obstacle_aabb(ball.pos, PlayerBall::RADIUS, spin_seg));
    assert(!CollisionSystem::check_obstacle_aabb({0.0f, PlayerBall::RADIUS + 1.25f, 0.0f}, PlayerBall::RADIUS, spin_seg));

    // Split Gate opens center gap cleanly on approach
    auto& segs = treadmill.get_segments_mut();
    segs[0].has_obstacle = true;
    segs[0].obstacle_type = ObstacleType::SPLIT_GATE;
    segs[0].has_second_obstacle = true;
    segs[0].z_position = 16.0f;
    treadmill.update(0.016f, ball.pos, false);
    assert(segs[0].split_open_t > 0.95f);
    assert(segs[0].obstacle_x_offset < -1.5f);
    assert(segs[0].second_obstacle_x > 1.5f);

    // Phase-Dash phases right through an obstacle without lethal hit!
    segs[0].z_position = 0.0f;
    segs[0].obstacle_x_offset = 0.0f;
    segs[0].has_second_obstacle = false;
    assert(ball.trigger_phase_dash());
    InteractionOutcome out = CollisionSystem::resolve_interactions(ball, treadmill);
    assert(out.phase_dodged);
    assert(!out.lethal_collision);

    std::cout << "[PASS] Laser Spinner, Split Gate, World Curve & Phase-Dash dodge\n";
}

void test_powerups_fever_revive_and_save_v4() {
    PlayerBall ball;
    TreadmillSystem treadmill;
    ball.reset();
    treadmill.reset();

    // Hyperflow Fever 2x
    bool triggered = false;
    for (int i = 0; i < 6; ++i) {
        if (treadmill.add_fever_charge(0.20f)) triggered = true;
    }
    assert(triggered && treadmill.is_fever_active() && treadmill.get_effective_multiplier() == 2);

    // Revive
    assert(treadmill.can_revive());
    treadmill.clear_nearby_for_revive();
    assert(!treadmill.can_revive());

    // SaveData v4 integrity, upgrades, missions & v2 migration
    SaveData save{};
    save.high_score = 22000;
    save.total_orbs = 600;
    save.world_curve_enabled = 1;
    save.seal();
    assert(save.is_valid());
    assert(save.try_unlock_skin(5));
    assert(save.is_skin_unlocked(5));

    save.total_orbs = 300;
    assert(save.try_buy_upgrade(UpgradeType::SHIELD_CAP));
    assert(save.try_buy_upgrade(UpgradeType::SHIELD_CAP));
    assert(save.try_buy_upgrade(UpgradeType::SHIELD_CAP));
    treadmill.set_game_mode(GameMode::ARCADE);
    treadmill.apply_upgrades(save);
    treadmill.reset();
    assert(treadmill.is_shield_active());

    int rewarded = save.advance_missions(15, 0, 0, 0, 1, 0);
    assert(rewarded > 0 && save.missions_completed == 1);

    SaveDataV2 old_v2{};
    old_v2.high_score = 42000;
    old_v2.total_orbs = 275;
    old_v2.unlocked_skins_mask = 0x07;
    old_v2.checksum = old_v2.compute_checksum();
    SaveData migrated = SaveData::migrate_from_v2(old_v2);
    assert(migrated.is_valid() && migrated.high_score == 42000 && migrated.is_skin_unlocked(2));

    std::cout << "[PASS] Hyperflow Fever, Revive, SaveData v4 & v2->v4 migration\n";
}

} // namespace

int main() {
    std::cout << "=== Running VeloSphere 4.0 Apex Native C++17 Unit Tests ===\n";
    test_math3d();
    test_player_gyro_jump_buffer_and_phase_dash();
    test_hazards_phase_dodge_and_world_curve();
    test_powerups_fever_revive_and_save_v4();
    std::cout << "=== ALL TESTS PASSED ===\n";
    return 0;
}
