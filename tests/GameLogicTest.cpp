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

    std::cout << "[PASS] Math3D vector & matrix transformations\n";
}

void test_player_ball_physics_and_jump() {
    PlayerBall ball;
    ball.reset();
    assert(!ball.is_airborne);
    assert(approx_eq(ball.pos.y, PlayerBall::RADIUS));

    // Jump initiation
    bool jumped = ball.jump();
    assert(jumped);
    assert(ball.is_airborne);
    assert(!ball.jump()); // Cannot double-jump while airborne

    // Advance 0.15s in air
    ball.update(0.10f, 20.0f);
    assert(ball.pos.y > PlayerBall::RADIUS + 0.5f);

    // Mid-air slam accelerates descent
    bool slammed = ball.slam();
    assert(slammed);
    for (int i = 0; i < 20; ++i) {
        ball.update(0.02f, 20.0f);
    }
    assert(!ball.is_airborne);
    assert(approx_eq(ball.jump_y, 0.0f));

    // Lateral steering bounds clamping
    ball.apply_touch_delta(100.0f);
    for (int i = 0; i < 30; ++i) {
        ball.update(0.02f, 20.0f);
    }
    assert(ball.pos.x <= PlayerBall::LATERAL_BOUND + EPS);

    std::cout << "[PASS] PlayerBall steering, jump, and slam physics\n";
}

void test_collision_and_obstacle_types() {
    PlayerBall ball;
    TreadmillSystem treadmill;
    ball.reset();
    treadmill.reset();

    // Test Low Laser Barrier jump clearance
    PathSegment low_seg{};
    low_seg.has_obstacle = true;
    low_seg.z_position = 0.0f;
    low_seg.obstacle_x_offset = 0.0f;
    low_seg.obstacle_width = TreadmillSystem::OBSTACLE_WIDTH;
    low_seg.obstacle_height = 0.72f;
    low_seg.obstacle_depth = 0.70f;
    low_seg.hover_y = 0.0f;
    low_seg.obstacle_type = ObstacleType::LOW_BARRIER;

    // Grounded ball hits the low barrier
    assert(CollisionSystem::check_obstacle_aabb(ball.pos, PlayerBall::RADIUS, low_seg));

    // Jumping ball at jump_y = 1.15f clears the low barrier
    Vec3 airborne_pos{0.0f, PlayerBall::RADIUS + 1.15f, 0.0f};
    assert(!CollisionSystem::check_obstacle_aabb(airborne_pos, PlayerBall::RADIUS, low_seg));

    // Test Overhead Gate clearance when grounded vs airborne
    PathSegment gate_seg{};
    gate_seg.has_obstacle = true;
    gate_seg.z_position = 0.0f;
    gate_seg.obstacle_x_offset = 0.0f;
    gate_seg.obstacle_width = TreadmillSystem::OBSTACLE_WIDTH;
    gate_seg.obstacle_height = 1.15f;
    gate_seg.obstacle_depth = 0.85f;
    gate_seg.hover_y = 1.52f;
    gate_seg.obstacle_type = ObstacleType::OVERHEAD_GATE;

    // Grounded ball rolls safely under Overhead Gate
    assert(!CollisionSystem::check_obstacle_aabb(ball.pos, PlayerBall::RADIUS, gate_seg));
    // Jumping ball collides with Overhead Gate
    assert(CollisionSystem::check_obstacle_aabb(airborne_pos, PlayerBall::RADIUS, gate_seg));

    std::cout << "[PASS] Low Laser Barrier & Overhead Gate vertical collision geometry\n";
}

void test_powerups_shield_and_magnet() {
    PlayerBall ball;
    TreadmillSystem treadmill;
    ball.reset();
    treadmill.reset();

    // Activate Shield and test obstacle absorption
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

    // Test Magnet attraction pulling an off-lane orb into the player
    treadmill.activate_powerup(PowerUpType::MAGNET);
    assert(treadmill.get_active_powerup() == PowerUpType::MAGNET);
    segs[1].has_orb = true;
    segs[1].orb_collected = false;
    segs[1].z_position = 6.0f;
    segs[1].orb_pos = {TreadmillSystem::RIGHT_LANE_X, 0.85f, 6.0f};

    float initial_x = segs[1].orb_pos.x;
    treadmill.update(0.10f, ball.pos);
    assert(segs[1].orb_pos.x < initial_x); // Pulled left toward player at x = 0

    std::cout << "[PASS] Shield absorption, invulnerability & Magnet orb attraction\n";
}

void test_save_data_checksum_and_skins() {
    SaveData save{};
    save.high_score = 14500;
    save.total_orbs = 150;
    save.seal();

    assert(save.is_valid());
    assert(save.is_skin_unlocked(0));
    assert(!save.is_skin_unlocked(1));

    // Unlock Solar Flare (cost 50 orbs)
    bool unlocked = save.try_unlock_skin(1);
    assert(unlocked);
    assert(save.is_skin_unlocked(1));
    assert(save.total_orbs == 100);
    assert(save.is_valid());

    // Tamper with high_score without resealing -> checksum validation must fail
    save.high_score = 999999;
    assert(!save.is_valid());

    std::cout << "[PASS] SaveData CRC32 integrity verification & skin unlocking\n";
}

} // namespace

int main() {
    std::cout << "=== Running VeloSphere 2.0 Native C++17 Unit Tests ===\n";
    test_math3d();
    test_player_ball_physics_and_jump();
    test_collision_and_obstacle_types();
    test_powerups_shield_and_magnet();
    test_save_data_checksum_and_skins();
    std::cout << "=== ALL TESTS PASSED ===\n";
    return 0;
}
