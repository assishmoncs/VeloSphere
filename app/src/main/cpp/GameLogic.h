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

// =============================================================================
// GAME STATES, MODES & USER SETTINGS
// =============================================================================

enum class GameState {
    HOME,
    GARAGE,
    MISSIONS,
    SETTINGS,
    COUNTDOWN,
    PLAYING,
    PAUSED,
    GAME_OVER
};

enum class GarageTab : int {
    SKINS    = 0,
    UPGRADES = 1
};

enum class GameMode : int {
    ARCADE      = 0, // Classic progressive endless runner with all power-ups
    TIME_ATTACK = 1, // 90s clock; orbs, dodges & near-misses add bonus time
    HARDCORE    = 2  // High base speed, no shields, 2x score & orb multiplier
};

enum class GraphicsQuality : uint8_t {
    LOW    = 0, // Reduced particles, fast sky, optimal for budget GPUs
    MEDIUM = 1, // Balanced visual effects & 60 FPS stability
    HIGH   = 2  // Full neon horizon, max particles & trail fidelity
};

enum class ControlScheme : uint8_t {
    DRAG        = 0, // Smooth continuous horizontal drag steering
    SWIPE_LANES = 1  // Crisp left/right flick snaps cleanly between 3 lanes
};

enum class SensitivityLevel : uint8_t {
    LOW    = 0,
    MEDIUM = 1,
    HIGH   = 2
};

// =============================================================================
// SPHERE SKINS (7 UNLOCKABLE SYNTHWAVE SKINS)
// =============================================================================

enum class SphereSkin : int {
    CYBER_CYAN       = 0,
    SOLAR_FLARE      = 1,
    VOID_VIOLET      = 2,
    EMERALD_MATRIX   = 3,
    SUPERNOVA_CHROME = 4,
    PHANTOM_GOLD     = 5,
    PRISM_PULSE      = 6,
    COUNT            = 7
};

struct SkinPalette {
    const char* name;
    int         unlock_cost;
    Color3      core_color;
    Color3      energy_color;
};

inline constexpr std::array<SkinPalette, 7> SKIN_PALETTES = {{
    {"CYBER CYAN",     0,   {0.12f, 0.95f, 0.88f}, {1.00f, 0.25f, 0.80f}},
    {"SOLAR FLARE",    50,  {1.00f, 0.72f, 0.10f}, {1.00f, 0.15f, 0.20f}},
    {"VOID VIOLET",    120, {0.48f, 0.18f, 0.98f}, {0.05f, 0.95f, 1.00f}},
    {"EMERALD MATRIX", 200, {0.08f, 0.96f, 0.38f}, {0.95f, 1.00f, 0.15f}},
    {"SUPERNOVA",      350, {0.95f, 0.96f, 1.00f}, {1.00f, 0.20f, 0.95f}},
    {"PHANTOM GOLD",   500, {1.00f, 0.84f, 0.18f}, {1.00f, 0.98f, 0.70f}},
    {"PRISM PULSE",    750, {0.98f, 0.20f, 0.75f}, {0.08f, 1.00f, 0.92f}}
}};

// =============================================================================
// PERMANENT TECH UPGRADES (4 MODULES, LEVELS 0..5)
// =============================================================================

enum class UpgradeType : int {
    SHIELD_CAP         = 0, // Stronger shield, Lv3+ starts run with Aegis Shield
    MAGNET_COIL        = 1, // Longer Magnet duration & wider pull radius
    CHRONO_CORE        = 2, // Longer Slow-Mo & bonus score multiplier in Slow-Mo
    OVERDRIVE_THRUSTER = 3, // Longer Overdrive & faster Hyperflow/Fever charge
    COUNT              = 4
};

struct UpgradeInfo {
    const char* name;
    const char* short_desc;
    Color3      accent;
};

inline constexpr std::array<UpgradeInfo, 4> UPGRADE_CATALOG = {{
    {"AEGIS CAPACITOR", "START SHIELD LV3+",  {0.05f, 0.95f, 1.00f}},
    {"MAGNET COIL",     "WIDER ORB PULL",     {0.15f, 1.00f, 0.45f}},
    {"CHRONO CORE",     "LONGER SLOW-MO",     {0.62f, 0.38f, 1.00f}},
    {"HYPER THRUSTER",  "FASTER FEVER FILL",  {1.00f, 0.25f, 0.85f}}
}};

inline constexpr int MAX_UPGRADE_LEVEL = 5;
inline constexpr std::array<int, MAX_UPGRADE_LEVEL> UPGRADE_COSTS = {40, 80, 140, 220, 320};

// =============================================================================
// DYNAMIC SYNTHWAVE BIOMES (SMOOTHLY INTERPOLATED)
// =============================================================================

struct BiomePalette {
    const char* name;
    Color3      floor_base;
    Color3      grid_color;
    Color3      rail_color;
    Color3      sky_fog;
    Color3      sun_top;
    Color3      sun_bottom;
};

inline constexpr std::array<BiomePalette, 4> BIOME_PALETTES = {{
    {"NEON GRID",    {0.03f, 0.04f, 0.09f}, {0.00f, 0.85f, 1.00f}, {0.00f, 0.92f, 1.00f}, {0.03f, 0.03f, 0.09f}, {1.00f, 0.82f, 0.25f}, {1.00f, 0.16f, 0.62f}},
    {"SOLAR OUTRUN", {0.08f, 0.02f, 0.07f}, {1.00f, 0.45f, 0.15f}, {1.00f, 0.20f, 0.65f}, {0.08f, 0.02f, 0.07f}, {1.00f, 0.95f, 0.35f}, {1.00f, 0.25f, 0.12f}},
    {"CYBER MATRIX", {0.01f, 0.06f, 0.04f}, {0.10f, 0.98f, 0.45f}, {0.35f, 1.00f, 0.25f}, {0.01f, 0.05f, 0.04f}, {0.55f, 1.00f, 0.45f}, {0.00f, 0.78f, 0.68f}},
    {"VOID HORIZON", {0.04f, 0.02f, 0.10f}, {0.65f, 0.25f, 1.00f}, {0.00f, 0.95f, 1.00f}, {0.03f, 0.01f, 0.09f}, {0.95f, 0.40f, 1.00f}, {0.25f, 0.20f, 1.00f}}
}};

// =============================================================================
// ROTATING MISSION SYSTEM
// =============================================================================

enum class MissionType : uint8_t {
    COLLECT_ORBS      = 0, // Cumulative orbs collected
    DODGE_OBSTACLES   = 1, // Cumulative obstacles dodged
    SCORE_NEAR_MISSES = 2, // Cumulative near misses
    REACH_DISTANCE    = 3, // Cumulative distance traveled (meters)
    REACH_COMBO       = 4, // Reach a specific combo multiplier in a run
    TRIGGER_FEVER     = 5, // Trigger Hyperflow/Fever activations
    COUNT             = 6
};

struct MissionSlot {
    uint8_t  type{0};
    uint8_t  tier{1};
    uint16_t reward_orbs{40};
    int32_t  target{20};
    int32_t  progress{0};

    void format_title(char* buf, size_t buf_size) const {
        switch (static_cast<MissionType>(type)) {
            case MissionType::COLLECT_ORBS:
                std::snprintf(buf, buf_size, "COLLECT %d DATA ORBS", target);
                break;
            case MissionType::DODGE_OBSTACLES:
                std::snprintf(buf, buf_size, "DODGE %d OBSTACLES", target);
                break;
            case MissionType::SCORE_NEAR_MISSES:
                std::snprintf(buf, buf_size, "SCORE %d NEAR MISSES", target);
                break;
            case MissionType::REACH_DISTANCE:
                std::snprintf(buf, buf_size, "TRAVEL %d METERS", target);
                break;
            case MissionType::REACH_COMBO:
                std::snprintf(buf, buf_size, "REACH %dX COMBO", target);
                break;
            case MissionType::TRIGGER_FEVER:
                std::snprintf(buf, buf_size, "TRIGGER HYPERFLOW %dX", target);
                break;
            default:
                std::snprintf(buf, buf_size, "COMPLETE RUN");
                break;
        }
    }

    float progress_ratio() const {
        if (target <= 0) return 1.0f;
        return std::clamp(static_cast<float>(progress) / static_cast<float>(target), 0.0f, 1.0f);
    }
};

inline MissionSlot make_mission(MissionType type, uint8_t tier) {
    MissionSlot m{};
    m.type = static_cast<uint8_t>(type);
    m.tier = std::max<uint8_t>(1, tier);
    int t = static_cast<int>(m.tier);
    switch (type) {
        case MissionType::COLLECT_ORBS:
            m.target = 15 + (t - 1) * 12;
            m.reward_orbs = static_cast<uint16_t>(35 + (t - 1) * 15);
            break;
        case MissionType::DODGE_OBSTACLES:
            m.target = 20 + (t - 1) * 18;
            m.reward_orbs = static_cast<uint16_t>(40 + (t - 1) * 15);
            break;
        case MissionType::SCORE_NEAR_MISSES:
            m.target = 3 + (t - 1) * 3;
            m.reward_orbs = static_cast<uint16_t>(45 + (t - 1) * 20);
            break;
        case MissionType::REACH_DISTANCE:
            m.target = 350 + (t - 1) * 250;
            m.reward_orbs = static_cast<uint16_t>(40 + (t - 1) * 15);
            break;
        case MissionType::REACH_COMBO:
            m.target = std::min(5, 2 + t);
            m.reward_orbs = static_cast<uint16_t>(35 + (t - 1) * 20);
            break;
        case MissionType::TRIGGER_FEVER:
        default:
            m.target = std::min(5, t);
            m.reward_orbs = static_cast<uint16_t>(50 + (t - 1) * 25);
            break;
    }
    m.progress = 0;
    return m;
}

// =============================================================================
// LANES, OBSTACLES & POWER-UPS
// =============================================================================

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
    OVERDRIVE = 2, // Speed boost + invulnerable obstacle smash (+200 pts)
    SLOW_MO   = 3, // Time dilation (0.68x obstacle speed)
    MAGNET    = 4  // Magnetic pull attracting all nearby Data Orbs
};

// =============================================================================
// VERSIONED SAVE DATA WITH CRC32 INTEGRITY CHECK (v3 + v2 MIGRATION)
// =============================================================================

struct SaveDataV2 {
    static constexpr uint32_t MAGIC   = 0x56454C4F;
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
    uint32_t unlocked_skins_mask{0x01};
    int32_t  selected_skin{0};
    int32_t  selected_mode{0};
    uint8_t  sfx_enabled{1};
    uint8_t  music_enabled{1};
    uint8_t  haptics_enabled{1};
    uint8_t  reserved{0};
    uint32_t checksum{0};

    uint32_t compute_checksum() const {
        const auto* bytes = reinterpret_cast<const uint8_t*>(this);
        const size_t len = offsetof(SaveDataV2, checksum);
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= static_cast<uint32_t>(bytes[i]);
            for (int b = 0; b < 8; ++b) {
                crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1u)));
            }
        }
        return ~crc;
    }

    bool is_valid() const {
        return magic == MAGIC && version == VERSION && checksum == compute_checksum();
    }
};

struct SaveData {
    static constexpr uint32_t MAGIC   = 0x56454C4F; // "VELO"
    static constexpr uint32_t VERSION = 3;
    static constexpr int      REVIVE_ORB_COST = 25;

    uint32_t magic{MAGIC};
    uint32_t version{VERSION};
    int32_t  high_score{0};
    int32_t  best_score_time_attack{0};
    int32_t  best_score_hardcore{0};
    int32_t  total_orbs{0};
    int32_t  best_combo{1};
    int32_t  total_runs{0};
    int32_t  total_dodges{0};
    int32_t  total_near_misses{0};
    int32_t  total_distance_m{0};
    int32_t  missions_completed{0};
    uint32_t unlocked_skins_mask{0x01}; // Bit 0 = CYBER_CYAN unlocked
    int32_t  selected_skin{0};
    int32_t  selected_mode{0};
    uint8_t  upgrade_levels[4]{0, 0, 0, 0};
    MissionSlot missions[3]{};
    uint8_t  sfx_enabled{1};
    uint8_t  music_enabled{1};
    uint8_t  haptics_enabled{1};
    uint8_t  graphics_quality{static_cast<uint8_t>(GraphicsQuality::HIGH)};
    uint8_t  control_scheme{static_cast<uint8_t>(ControlScheme::DRAG)};
    uint8_t  sensitivity{static_cast<uint8_t>(SensitivityLevel::MEDIUM)};
    uint8_t  show_fps{0};
    uint8_t  reserved{0};
    uint32_t checksum{0};

    SaveData() {
        init_default_missions();
    }

    void init_default_missions() {
        missions[0] = make_mission(MissionType::COLLECT_ORBS, 1);
        missions[1] = make_mission(MissionType::DODGE_OBSTACLES, 1);
        missions[2] = make_mission(MissionType::SCORE_NEAR_MISSES, 1);
    }

    static SaveData migrate_from_v2(const SaveDataV2& old) {
        SaveData d{};
        d.high_score             = old.high_score;
        d.best_score_time_attack = old.best_score_time_attack;
        d.best_score_hardcore    = old.best_score_hardcore;
        d.total_orbs             = old.total_orbs;
        d.best_combo             = std::max(1, old.best_combo);
        d.total_runs             = old.total_runs;
        d.total_dodges           = old.total_dodges;
        d.unlocked_skins_mask    = old.unlocked_skins_mask | 0x01u;
        d.selected_skin          = std::clamp(old.selected_skin, 0, static_cast<int>(SphereSkin::COUNT) - 1);
        d.selected_mode          = std::clamp(old.selected_mode, 0, 2);
        d.sfx_enabled            = old.sfx_enabled;
        d.music_enabled          = old.music_enabled;
        d.haptics_enabled        = old.haptics_enabled;
        d.seal();
        return d;
    }

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

    int get_upgrade_level(UpgradeType type) const {
        int idx = static_cast<int>(type);
        if (idx < 0 || idx >= static_cast<int>(UpgradeType::COUNT)) return 0;
        return std::clamp(static_cast<int>(upgrade_levels[idx]), 0, MAX_UPGRADE_LEVEL);
    }

    int get_next_upgrade_cost(UpgradeType type) const {
        int lv = get_upgrade_level(type);
        if (lv >= MAX_UPGRADE_LEVEL) return 0;
        return UPGRADE_COSTS[static_cast<size_t>(lv)];
    }

    bool try_buy_upgrade(UpgradeType type) {
        int idx = static_cast<int>(type);
        if (idx < 0 || idx >= static_cast<int>(UpgradeType::COUNT)) return false;
        int lv = get_upgrade_level(type);
        if (lv >= MAX_UPGRADE_LEVEL) return false;
        int cost = UPGRADE_COSTS[static_cast<size_t>(lv)];
        if (total_orbs >= cost) {
            total_orbs -= cost;
            upgrade_levels[idx] = static_cast<uint8_t>(lv + 1);
            seal();
            return true;
        }
        return false;
    }

    bool try_spend_revive() {
        if (total_orbs < REVIVE_ORB_COST) return false;
        total_orbs -= REVIVE_ORB_COST;
        seal();
        return true;
    }

    int get_pilot_rank() const {
        return 1 + (missions_completed / 3);
    }

    const char* get_pilot_title() const {
        int rank = get_pilot_rank();
        if (rank <= 1) return "ROOKIE";
        if (rank == 2) return "PILOT";
        if (rank == 3) return "RACER";
        if (rank == 4) return "ACE";
        if (rank == 5) return "PHANTOM";
        return "LEGEND";
    }

    float get_touch_sensitivity_value() const {
        switch (static_cast<SensitivityLevel>(sensitivity)) {
            case SensitivityLevel::LOW:  return 5.4f;
            case SensitivityLevel::HIGH: return 9.2f;
            case SensitivityLevel::MEDIUM:
            default:                     return 7.2f;
        }
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

    // Advance mission progress; returns total bonus orbs awarded if any mission completed
    int advance_missions(int delta_orbs, int delta_dodges, int delta_near_misses,
                         int delta_distance_m, int current_combo, int delta_fevers) {
        int orbs_rewarded = 0;
        for (int i = 0; i < 3; ++i) {
            MissionSlot& m = missions[i];
            MissionType mt = static_cast<MissionType>(m.type);
            switch (mt) {
                case MissionType::COLLECT_ORBS:
                    m.progress += delta_orbs;
                    break;
                case MissionType::DODGE_OBSTACLES:
                    m.progress += delta_dodges;
                    break;
                case MissionType::SCORE_NEAR_MISSES:
                    m.progress += delta_near_misses;
                    break;
                case MissionType::REACH_DISTANCE:
                    m.progress += delta_distance_m;
                    break;
                case MissionType::REACH_COMBO:
                    if (current_combo > m.progress) m.progress = current_combo;
                    break;
                case MissionType::TRIGGER_FEVER:
                    m.progress += delta_fevers;
                    break;
                default:
                    break;
            }
            if (m.progress >= m.target) {
                orbs_rewarded += static_cast<int>(m.reward_orbs);
                total_orbs += static_cast<int>(m.reward_orbs);
                missions_completed += 1;
                // Cycle to a fresh mission type with incremented tier
                uint8_t next_type = static_cast<uint8_t>((m.type + 3 + i) % static_cast<int>(MissionType::COUNT));
                uint8_t next_tier = static_cast<uint8_t>(std::min(12, static_cast<int>(m.tier) + 1));
                m = make_mission(static_cast<MissionType>(next_type), next_tier);
            }
        }
        if (orbs_rewarded > 0) {
            seal();
        }
        return orbs_rewarded;
    }
};

// =============================================================================
// PATH SEGMENT (OBSTACLES, DUAL-LANE WALLS, DATA ORBS & POWER-UPS)
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

    // Optional second static blocker in another lane (always leaves >= 1 safe lane)
    bool      has_second_obstacle{false};
    TrackLane second_lane{TrackLane::LEFT};
    float     second_obstacle_x{0.0f};

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
// PLAYER SPHERE PHYSICS (STEERING, JUMP BUFFERING, SQUASH/STRETCH & SLAM)
// =============================================================================

class PlayerBall {
public:
    static constexpr float RADIUS            = 0.70f;
    static constexpr float TRACK_HALF_WIDTH  = 3.8f;
    static constexpr float LATERAL_BOUND     = TRACK_HALF_WIDTH - RADIUS - 0.2f;
    static constexpr float STEER_RESPONSE    = 19.0f;
    static constexpr float TOUCH_SENSITIVITY = 7.2f;
    static constexpr float JUMP_VELOCITY     = 12.2f;
    static constexpr float SLAM_VELOCITY     = -25.0f;
    static constexpr float GRAVITY           = 33.0f;
    static constexpr float JUMP_BUFFER_TIME  = 0.15f;

    Vec3  pos{0.0f, RADIUS, 0.0f};
    float target_x{0.0f};
    int   current_lane{1}; // 0=LEFT, 1=CENTER, 2=RIGHT
    float roll_angle{0.0f};
    float lateral_roll_angle{0.0f};
    float tilt_angle{0.0f};
    float lateral_velocity{0.0f};
    float bounce_offset_y{0.0f};

    // Jump, input buffer & squash/stretch feel
    float jump_y{0.0f};
    float vertical_velocity{0.0f};
    bool  is_airborne{false};
    float jump_buffer_timer{0.0f};
    float scale_y{1.0f};
    float scale_xz{1.0f};
    bool  just_landed{false};

    // Post-shield-break, revive, or overdrive invulnerability timer
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
        current_lane = 1;
        roll_angle = 0.0f;
        lateral_roll_angle = 0.0f;
        tilt_angle = 0.0f;
        lateral_velocity = 0.0f;
        bounce_offset_y = 0.0f;
        jump_y = 0.0f;
        vertical_velocity = 0.0f;
        is_airborne = false;
        jump_buffer_timer = 0.0f;
        scale_y = 1.0f;
        scale_xz = 1.0f;
        just_landed = false;
        invulnerable_timer = 0.0f;
        is_tumbling = false;
        tumble_pos = {0.0f, RADIUS, 0.0f};
        tumble_vel = {0.0f, 0.0f, 0.0f};
        tumble_rot = {0.0f, 0.0f, 0.0f};
        tumble_ang_vel = {0.0f, 0.0f, 0.0f};
    }

    void revive() {
        is_tumbling = false;
        jump_y = 0.0f;
        vertical_velocity = 0.0f;
        is_airborne = false;
        jump_buffer_timer = 0.0f;
        pos = {0.0f, RADIUS, 0.0f};
        target_x = 0.0f;
        current_lane = 1;
        scale_y = 1.0f;
        scale_xz = 1.0f;
        invulnerable_timer = 2.2f;
    }

    bool jump() {
        if (is_tumbling) return false;
        if (is_airborne) {
            // Buffer jump so pressing just before landing triggers instantaneously!
            jump_buffer_timer = JUMP_BUFFER_TIME;
            return false;
        }
        is_airborne = true;
        vertical_velocity = JUMP_VELOCITY;
        jump_buffer_timer = 0.0f;
        scale_y = 1.24f;
        scale_xz = 0.88f;
        return true;
    }

    bool slam() {
        if (is_tumbling || !is_airborne) return false;
        if (vertical_velocity > SLAM_VELOCITY * 0.5f) {
            vertical_velocity = SLAM_VELOCITY;
            scale_y = 1.28f;
            scale_xz = 0.85f;
            return true;
        }
        return false;
    }

    void update(float dt, float forward_speed) {
        just_landed = false;

        if (invulnerable_timer > 0.0f) {
            invulnerable_timer = std::max(0.0f, invulnerable_timer - dt);
        }
        if (jump_buffer_timer > 0.0f) {
            jump_buffer_timer = std::max(0.0f, jump_buffer_timer - dt);
        }

        // Smooth elastic recovery of squash & stretch
        float spring = std::min(1.0f, 14.0f * dt);
        scale_y  += (1.0f - scale_y) * spring;
        scale_xz += (1.0f - scale_xz) * spring;

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
                bool hard_slam = (vertical_velocity < -18.0f);
                vertical_velocity = 0.0f;
                is_airborne = false;
                just_landed = true;
                scale_y = hard_slam ? 0.68f : 0.80f;
                scale_xz = hard_slam ? 1.24f : 1.14f;

                // Execute buffered jump immediately on touchdown
                if (jump_buffer_timer > 0.0f) {
                    jump();
                }
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
        // Keep current_lane synced with closest lane
        constexpr float lane_w = (TRACK_HALF_WIDTH * 2.0f) / 3.0f;
        if (target_x < -lane_w * 0.5f) current_lane = 0;
        else if (target_x > lane_w * 0.5f) current_lane = 2;
        else current_lane = 1;
    }

    void steer_to_lane(int lane_delta) {
        constexpr float lane_w = (TRACK_HALF_WIDTH * 2.0f) / 3.0f;
        current_lane = std::clamp(current_lane + lane_delta, 0, 2);
        target_x = static_cast<float>(current_lane - 1) * lane_w;
    }
};

// =============================================================================
// TREADMILL SYSTEM (SEGMENTS, POWER-UPS, HYPERFLOW FEVER, COMBOS & MODES)
// =============================================================================

struct FrameEvents {
    bool near_miss_triggered{false};
    bool fever_started{false};
    bool biome_changed{false};
    int  dodges_this_frame{0};
    int  distance_delta_m{0};
};

class TreadmillSystem {
public:
    static constexpr size_t POOL_SIZE           = 10;
    static constexpr float  SEGMENT_SPACING     = 10.0f;
    static constexpr float  MIN_OBSTACLE_GAP    = 20.0f;
    static constexpr float  MOVING_SAFETY_GAP   = 30.0f;
    static constexpr float  DESPAWN_THRESHOLD_Z = -10.0f;
    static constexpr float  BASE_SPEED          = 18.0f;
    static constexpr float  HARDCORE_BASE_SPEED = 28.0f;
    static constexpr float  SPEED_ACCELERATION  = 0.52f;
    static constexpr float  MAX_SPEED           = 58.0f;
    static constexpr float  TIME_ATTACK_START   = 90.0f;
    static constexpr float  FEVER_DURATION      = 6.0f;
    static constexpr float  BIOME_LENGTH        = 450.0f;

    static constexpr float LANE_WIDTH    = (PlayerBall::TRACK_HALF_WIDTH * 2.0f) / 3.0f;
    static constexpr float LEFT_LANE_X   = -LANE_WIDTH;
    static constexpr float CENTER_LANE_X = 0.0f;
    static constexpr float RIGHT_LANE_X  = LANE_WIDTH;

    static constexpr float OBSTACLE_WIDTH  = LANE_WIDTH * 0.94f;
    static constexpr float OBSTACLE_HEIGHT = LANE_WIDTH * 0.94f;
    static constexpr float OBSTACLE_DEPTH  = LANE_WIDTH * 0.90f;

    static constexpr float MOVING_HALF_CYCLE_TIME = 1.10f;
    static constexpr float MOVING_WARNING_TIME    = 0.35f;

    static constexpr float MOVING_DIFFICULTY_START_SPEED = 25.0f;
    static constexpr float MOVING_DIFFICULTY_END_SPEED   = 55.0f;
    static constexpr float MOVING_MIN_SPAWN_PROBABILITY  = 0.36f;
    static constexpr float MOVING_MAX_SPAWN_PROBABILITY  = 0.56f;

    TreadmillSystem()
        : rng_(std::random_device{}()),
          dist_prob_(0.0f, 1.0f) {
        reset();
    }

    void set_game_mode(GameMode mode) {
        game_mode_ = mode;
    }

    [[nodiscard]] GameMode get_game_mode() const { return game_mode_; }

    void apply_upgrades(const SaveData& save) {
        shield_lv_    = save.get_upgrade_level(UpgradeType::SHIELD_CAP);
        magnet_lv_    = save.get_upgrade_level(UpgradeType::MAGNET_COIL);
        chrono_lv_    = save.get_upgrade_level(UpgradeType::CHRONO_CORE);
        overdrive_lv_ = save.get_upgrade_level(UpgradeType::OVERDRIVE_THRUSTER);
    }

    void reset() {
        base_speed_ = (game_mode_ == GameMode::HARDCORE) ? HARDCORE_BASE_SPEED : BASE_SPEED;
        global_speed_ = base_speed_;
        distance_traveled_ = 0.0f;
        last_recorded_dist_m_ = 0;
        score_ = 0;
        bonus_score_ = 0;
        dodge_count_ = 0;
        dodge_milestone_ = 0;
        dodge_milestone_timer_ = 0.0f;
        has_new_best_ = false;
        red_pattern_index_ = 0;
        last_biome_index_ = 0;
        revive_used_ = false;

        // Combo, Hyperflow Fever & collectibles
        combo_ = 1;
        best_run_combo_ = 1;
        dodge_streak_ = 0;
        near_miss_count_ = 0;
        run_orbs_ = 0;
        fever_meter_ = 0.0f;
        fever_active_ = false;
        fever_timer_ = 0.0f;
        run_fevers_ = 0;

        // Power-up state (Lv3+ Aegis Capacitor starts Arcade/Time Attack with a Shield!)
        shield_active_ = (game_mode_ != GameMode::HARDCORE && shield_lv_ >= 3);
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
            segments_[i].has_second_obstacle = false;
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

    void trigger_banner(const char* msg, float duration = 1.25f) {
        if (!msg) return;
        std::snprintf(banner_text_, sizeof(banner_text_), "%s", msg);
        banner_timer_ = duration;
    }

    void activate_powerup(PowerUpType type) {
        if (type == PowerUpType::SHIELD) {
            shield_active_ = true;
            trigger_banner("AEGIS SHIELD!", 1.2f);
            return;
        }
        active_powerup_ = type;
        switch (type) {
            case PowerUpType::OVERDRIVE:
                powerup_duration_ = 5.0f + static_cast<float>(overdrive_lv_) * 0.7f;
                powerup_timer_ = powerup_duration_;
                trigger_banner("OVERDRIVE BOOST!", 1.25f);
                break;
            case PowerUpType::SLOW_MO:
                powerup_duration_ = 6.0f + static_cast<float>(chrono_lv_) * 0.8f;
                powerup_timer_ = powerup_duration_;
                trigger_banner("CHRONO SLOW-MO!", 1.25f);
                break;
            case PowerUpType::MAGNET:
                powerup_duration_ = 8.0f + static_cast<float>(magnet_lv_) * 1.0f;
                powerup_timer_ = powerup_duration_;
                trigger_banner("ORB MAGNET!", 1.25f);
                break;
            default:
                break;
        }
    }

    bool consume_shield() {
        if (!shield_active_) return false;
        shield_active_ = false;
        int pts = (300 + shield_lv_ * 60) * get_effective_multiplier();
        bonus_score_ += pts;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "+%d SHIELD SMASH!", pts);
        trigger_banner(buf, 1.3f);
        return true;
    }

    float get_shield_invuln_duration() const {
        return 1.2f + static_cast<float>(shield_lv_) * 0.2f;
    }

    void clear_nearby_for_revive() {
        revive_used_ = true;
        for (auto& seg : segments_) {
            if (seg.z_position < 32.0f) {
                seg.has_obstacle = false;
                seg.has_second_obstacle = false;
                seg.cleared = true;
            }
        }
        trigger_banner("SECOND CHANCE!", 1.5f);
    }

    bool can_revive() const {
        return !revive_used_ && !time_expired_;
    }

    void reset_combo() {
        combo_ = 1;
        dodge_streak_ = 0;
        if (!fever_active_) {
            fever_meter_ *= 0.5f;
        }
    }

    bool add_fever_charge(float amount) {
        if (fever_active_) return false;
        float mult = 1.0f + static_cast<float>(overdrive_lv_) * 0.12f;
        fever_meter_ = std::min(1.0f, fever_meter_ + amount * mult);
        if (fever_meter_ >= 1.0f) {
            fever_active_ = true;
            fever_timer_ = FEVER_DURATION;
            ++run_fevers_;
            trigger_banner("HYPERFLOW 2X!", 1.45f);
            return true;
        }
        return false;
    }

    FrameEvents update(float dt, const Vec3& player_pos = {0.0f, PlayerBall::RADIUS, 0.0f}) {
        FrameEvents ev{};

        orb_spin_angle_ += 3.2f * dt;
        if (orb_spin_angle_ > 2.0f * PI) orb_spin_angle_ -= 2.0f * PI;

        if (banner_timer_ > 0.0f) {
            banner_timer_ = std::max(0.0f, banner_timer_ - dt);
        }

        // Hyperflow / Fever countdown
        if (fever_active_) {
            fever_timer_ -= dt;
            fever_meter_ = std::clamp(fever_timer_ / FEVER_DURATION, 0.0f, 1.0f);
            if (fever_timer_ <= 0.0f) {
                fever_active_ = false;
                fever_timer_ = 0.0f;
                fever_meter_ = 0.0f;
            }
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

        // Effective speed accounts for Overdrive (1.35x), Hyperflow (1.12x), or Slow-Mo (0.68x)
        float speed_mod = 1.0f;
        if (active_powerup_ == PowerUpType::OVERDRIVE) {
            speed_mod = 1.35f;
        } else if (active_powerup_ == PowerUpType::SLOW_MO) {
            speed_mod = 0.68f;
        } else if (fever_active_) {
            speed_mod = 1.12f;
        }
        global_speed_ = std::clamp(base_speed_ * speed_mod, 12.0f, MAX_SPEED * 1.25f);

        const float z_disp = global_speed_ * dt;
        distance_traveled_ += z_disp;

        int cur_dist_m = static_cast<int>(distance_traveled_);
        if (cur_dist_m > last_recorded_dist_m_) {
            ev.distance_delta_m = cur_dist_m - last_recorded_dist_m_;
            last_recorded_dist_m_ = cur_dist_m;
        }

        // Detect biome transition
        size_t cur_biome = get_biome_index();
        if (cur_biome != last_biome_index_) {
            last_biome_index_ = cur_biome;
            ev.biome_changed = true;
            char bbuf[32];
            std::snprintf(bbuf, sizeof(bbuf), "SECTOR: %s", BIOME_PALETTES[cur_biome].name);
            trigger_banner(bbuf, 1.5f);
        }

        const int eff_mult = get_effective_multiplier();
        const float magnet_range = 16.0f + static_cast<float>(magnet_lv_) * 2.5f;
        const float magnet_pull  = 28.0f + static_cast<float>(magnet_lv_) * 3.0f;

        for (auto& segment : segments_) {
            segment.z_position -= z_disp;
            segment.orb_pos.z = segment.z_position;
            segment.powerup_pos.z = segment.z_position;

            // Magnet attraction pulls uncollected orbs directly toward player
            if (segment.has_orb && !segment.orb_collected &&
                active_powerup_ == PowerUpType::MAGNET &&
                segment.z_position > -2.0f && segment.z_position < magnet_range) {
                Vec3 dir = (player_pos - segment.orb_pos);
                float dist = dir.length();
                if (dist > 0.05f) {
                    segment.orb_pos += (dir / dist) * (magnet_pull * dt);
                }
            }

            if (segment.has_obstacle && !segment.cleared && segment.z_position < -PlayerBall::RADIUS) {
                segment.cleared = true;
                ++dodge_count_;
                ++dodge_streak_;
                ++ev.dodges_this_frame;

                // Update combo every 3 clean dodges (up to 5x)
                combo_ = std::min(5, 1 + (dodge_streak_ / 3));
                if (combo_ > best_run_combo_) best_run_combo_ = combo_;

                // Check for near-miss bonus (close lateral pass or jumping cleanly over a low barrier)
                float lat_dist = std::fabs(player_pos.x - segment.obstacle_x_offset);
                if (segment.has_second_obstacle) {
                    lat_dist = std::min(lat_dist, std::fabs(player_pos.x - segment.second_obstacle_x));
                }

                bool jumped_over = (segment.obstacle_type == ObstacleType::LOW_BARRIER &&
                                    lat_dist < segment.obstacle_width * 0.65f &&
                                    player_pos.y > segment.obstacle_height);
                bool close_shave = (lat_dist < (segment.obstacle_width * 0.5f + PlayerBall::RADIUS + 0.58f));

                if (jumped_over || close_shave) {
                    ++near_miss_count_;
                    ev.near_miss_triggered = true;
                    int nm_pts = 200 * combo_ * eff_mult;
                    bonus_score_ += nm_pts;
                    if (fever_active_) {
                        fever_timer_ = std::min(FEVER_DURATION, fever_timer_ + 0.6f);
                    } else if (add_fever_charge(0.18f)) {
                        ev.fever_started = true;
                    }
                    if (game_mode_ == GameMode::TIME_ATTACK) {
                        time_remaining_ = std::min(99.0f, time_remaining_ + 1.5f);
                    }
                    if (!ev.fever_started) {
                        char buf[32];
                        std::snprintf(buf, sizeof(buf), "NEAR MISS +%d!", nm_pts);
                        trigger_banner(buf, 0.95f);
                    }
                } else {
                    bonus_score_ += 100 * combo_ * eff_mult;
                    if (add_fever_charge(0.06f)) {
                        ev.fever_started = true;
                    }
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
                const float eased_t = local_t * local_t * (3.0f - 2.0f * local_t);

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
                    segment.obstacle_x_offset = start_x + (second_x - start_x) * eased_t;
                } else {
                    segment.obstacle_x_offset = second_x + (third_x - second_x) * eased_t;
                }
            }
        }

        float speed_ratio = (base_speed_ - BASE_SPEED) / (MAX_SPEED - BASE_SPEED);
        float speed_mult = 1.0f + std::clamp(speed_ratio, 0.0f, 1.0f) * 1.5f;
        float chrono_bonus = (active_powerup_ == PowerUpType::SLOW_MO)
            ? (1.0f + static_cast<float>(chrono_lv_) * 0.10f) : 1.0f;
        score_ = static_cast<int>(distance_traveled_ * 1.5f * speed_mult *
                                  static_cast<float>(get_mode_multiplier()) * chrono_bonus) + bonus_score_;

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

        return ev;
    }

    bool collect_orb(PathSegment& seg) {
        if (!seg.has_orb || seg.orb_collected) return false;
        seg.orb_collected = true;
        int mult = get_mode_multiplier();
        run_orbs_ += mult;
        bonus_score_ += 50 * combo_ * get_effective_multiplier();
        bool started_fever = add_fever_charge(0.05f);
        if (game_mode_ == GameMode::TIME_ATTACK) {
            time_remaining_ = std::min(99.0f, time_remaining_ + 1.5f);
        }
        return started_fever;
    }

    void smash_obstacle(PathSegment& seg) {
        seg.has_obstacle = false;
        seg.has_second_obstacle = false;
        seg.cleared = true;
        ++dodge_count_;
        bonus_score_ += 200 * combo_ * get_effective_multiplier();
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

    [[nodiscard]] int get_effective_multiplier() const {
        return get_mode_multiplier() * (fever_active_ ? 2 : 1);
    }

    [[nodiscard]] size_t get_biome_index() const {
        return static_cast<size_t>(distance_traveled_ / BIOME_LENGTH) % BIOME_PALETTES.size();
    }

    // Smoothly interpolated biome palette across the last 40m of each sector!
    [[nodiscard]] BiomePalette get_interpolated_biome() const {
        const float pos_in_biome = std::fmod(distance_traveled_, BIOME_LENGTH);
        const size_t cur_idx = get_biome_index();
        const size_t next_idx = (cur_idx + 1) % BIOME_PALETTES.size();
        const float blend_zone = 45.0f;

        if (pos_in_biome <= (BIOME_LENGTH - blend_zone)) {
            return BIOME_PALETTES[cur_idx];
        }
        float t = smoothstep(BIOME_LENGTH - blend_zone, BIOME_LENGTH, pos_in_biome);
        const auto& a = BIOME_PALETTES[cur_idx];
        const auto& b = BIOME_PALETTES[next_idx];
        return {
            (t < 0.5f) ? a.name : b.name,
            Color3::lerp(a.floor_base, b.floor_base, t),
            Color3::lerp(a.grid_color, b.grid_color, t),
            Color3::lerp(a.rail_color, b.rail_color, t),
            Color3::lerp(a.sky_fog,    b.sky_fog,    t),
            Color3::lerp(a.sun_top,    b.sun_top,    t),
            Color3::lerp(a.sun_bottom, b.sun_bottom, t)
        };
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
    [[nodiscard]] bool  is_fever_active() const { return fever_active_; }
    [[nodiscard]] float get_fever_meter() const { return fever_meter_; }
    [[nodiscard]] int   get_run_fevers() const { return run_fevers_; }
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
        static constexpr std::array<TrackLane, 10> RED_PATTERN = {{
            TrackLane::LEFT,
            TrackLane::CENTER,
            TrackLane::RIGHT,
            TrackLane::LEFT,
            TrackLane::CENTER,
            TrackLane::RIGHT,
            TrackLane::CENTER,
            TrackLane::LEFT,
            TrackLane::RIGHT,
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
        seg.has_second_obstacle = false;
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

        seg.has_obstacle = !obstacle_too_close && (dist_prob_(rng_) > 0.26f);

        if (!seg.has_obstacle) {
            seg.lane = TrackLane::CENTER;
            seg.motion_target_lane = TrackLane::CENTER;
            seg.obstacle_x_offset = CENTER_LANE_X;
            seg.is_moving = false;

            // Spawn collectible orb or occasional power-up on open segments
            float roll = dist_prob_(rng_);
            if (roll < 0.60f) {
                int orb_l = static_cast<int>(dist_prob_(rng_) * 3.0f);
                seg.orb_lane = static_cast<TrackLane>(std::min(2, orb_l));
                seg.has_orb = true;
                seg.orb_pos = {lane_x(seg.orb_lane), 0.85f, seg.z_position};
            } else if (roll < 0.74f && distance_traveled_ > 70.0f) {
                int pu_l = static_cast<int>(dist_prob_(rng_) * 3.0f);
                seg.powerup_lane = static_cast<TrackLane>(std::min(2, pu_l));
                float p_roll = dist_prob_(rng_);
                if (game_mode_ != GameMode::HARDCORE && p_roll < 0.28f && !shield_active_) {
                    seg.powerup_type = PowerUpType::SHIELD;
                } else if (p_roll < 0.54f) {
                    seg.powerup_type = PowerUpType::MAGNET;
                } else if (p_roll < 0.78f) {
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

            float type_roll = dist_prob_(rng_);
            if (distance_traveled_ > 50.0f && type_roll < 0.28f) {
                seg.obstacle_type = ObstacleType::LOW_BARRIER;
                seg.obstacle_height = 0.72f;
                seg.obstacle_depth = 0.70f;
                seg.hover_y = 0.0f;
                // Incentive Data Orb directly above the low barrier for jump mastery
                seg.has_orb = true;
                seg.orb_lane = seg.lane;
                seg.orb_pos = {seg.obstacle_x_offset, 1.95f, seg.z_position};
            } else if (distance_traveled_ > 120.0f && type_roll < 0.46f) {
                seg.obstacle_type = ObstacleType::OVERHEAD_GATE;
                seg.obstacle_height = 1.15f;
                seg.obstacle_depth = 0.85f;
                seg.hover_y = 1.52f;
                seg.has_orb = true;
                seg.orb_lane = seg.lane;
                seg.orb_pos = {seg.obstacle_x_offset, 0.75f, seg.z_position};
            } else {
                seg.obstacle_type = ObstacleType::STATIC_BLOCK;

                // Dual-Lane Wall formation at higher speeds (always leaves 1 safe lane!)
                float dual_chance = 0.16f + 0.24f * difficulty_t;
                if (distance_traveled_ > 160.0f && dist_prob_(rng_) < dual_chance) {
                    int l1 = static_cast<int>(seg.lane);
                    int l2 = (l1 + ((dist_prob_(rng_) < 0.5f) ? 1 : 2)) % 3;
                    int safe_l = 3 - l1 - l2;
                    seg.has_second_obstacle = true;
                    seg.second_lane = static_cast<TrackLane>(l2);
                    seg.second_obstacle_x = lane_x(seg.second_lane);

                    // Place reward orb in the single guaranteed safe lane
                    seg.has_orb = true;
                    seg.orb_lane = static_cast<TrackLane>(safe_l);
                    seg.orb_pos = {lane_x(seg.orb_lane), 0.85f, seg.z_position};
                } else if (dist_prob_(rng_) < 0.52f) {
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
    int         last_recorded_dist_m_{0};
    int         score_{0};
    int         bonus_score_{0};
    int         best_score_{0};
    int         dodge_count_{0};
    int         dodge_milestone_{0};
    float       dodge_milestone_timer_{0.0f};
    bool        has_new_best_{false};
    size_t      last_biome_index_{0};
    bool        revive_used_{false};

    int         combo_{1};
    int         best_run_combo_{1};
    int         dodge_streak_{0};
    int         near_miss_count_{0};
    int         run_orbs_{0};

    float       fever_meter_{0.0f};
    bool        fever_active_{false};
    float       fever_timer_{0.0f};
    int         run_fevers_{0};

    bool        shield_active_{false};
    PowerUpType active_powerup_{PowerUpType::NONE};
    float       powerup_timer_{0.0f};
    float       powerup_duration_{1.0f};
    float       orb_spin_angle_{0.0f};

    // Upgrade levels synced from SaveData
    int         shield_lv_{0};
    int         magnet_lv_{0};
    int         chrono_lv_{0};
    int         overdrive_lv_{0};

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
    bool fever_started{false};
    bool powerup_picked_up{false};
    bool obstacle_smashed{false};
    bool shield_broken{false};
    bool lethal_collision{false};
    Vec3 impact_pos{};
};

class CollisionSystem {
public:
    static bool check_box_aabb(const Vec3& center, float radius,
                               float box_x, float box_y_base, float box_z,
                               float w, float h, float d) {
        if (box_z < -3.0f || box_z > 3.0f) return false;

        float hx = w * 0.5f;
        float hy = h * 0.5f;
        float hz = d * 0.5f;

        Vec3 box_center = {box_x, hy + box_y_base, box_z};

        float cx = std::clamp(center.x, box_center.x - hx, box_center.x + hx);
        float cy = std::clamp(center.y, box_center.y - hy, box_center.y + hy);
        float cz = std::clamp(center.z, box_center.z - hz, box_center.z + hz);

        float dx = center.x - cx;
        float dy = center.y - cy;
        float dz = center.z - cz;

        return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
    }

    static bool check_obstacle_aabb(const Vec3& center, float radius, const PathSegment& seg) {
        if (!seg.has_obstacle) return false;
        if (check_box_aabb(center, radius,
                           seg.obstacle_x_offset, seg.hover_y, seg.z_position,
                           seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth)) {
            return true;
        }
        if (seg.has_second_obstacle) {
            if (check_box_aabb(center, radius,
                               seg.second_obstacle_x, seg.hover_y, seg.z_position,
                               seg.obstacle_width, seg.obstacle_height, seg.obstacle_depth)) {
                return true;
            }
        }
        return false;
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
                float pickup_r = radius + 0.68f;
                if (dist_sq <= pickup_r * pickup_r) {
                    if (treadmill.collect_orb(seg)) {
                        out.fever_started = true;
                    }
                    out.orb_picked_up = true;
                }
            }

            // 2. Check Power-Up pickup
            if (seg.powerup_type != PowerUpType::NONE && !seg.powerup_collected) {
                float dist_sq = Vec3::distance_sq(center, seg.powerup_pos);
                float pickup_r = radius + 0.78f;
                if (dist_sq <= pickup_r * pickup_r) {
                    seg.powerup_collected = true;
                    treadmill.activate_powerup(seg.powerup_type);
                    out.powerup_picked_up = true;
                }
            }

            // 3. Check Obstacle collision (primary or secondary lane blocker)
            if (seg.has_obstacle && check_obstacle_aabb(center, radius, seg)) {
                out.impact_pos = {center.x, seg.hover_y + seg.obstacle_height * 0.5f, seg.z_position};

                if (treadmill.get_active_powerup() == PowerUpType::OVERDRIVE) {
                    treadmill.smash_obstacle(seg);
                    out.obstacle_smashed = true;
                } else if (treadmill.consume_shield()) {
                    seg.has_obstacle = false;
                    seg.has_second_obstacle = false;
                    seg.cleared = true;
                    ball.invulnerable_timer = treadmill.get_shield_invuln_duration();
                    out.shield_broken = true;
                } else if (ball.invulnerable_timer > 0.0f) {
                    seg.has_obstacle = false;
                    seg.has_second_obstacle = false;
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
