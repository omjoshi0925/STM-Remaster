// Combat.hpp - Milestone 6: enemy AI + hero combat, driven by the original
// config tables (EnemysAttributeConfigs.bin, AttackIntervalTimeConfigs.bin)
// and the original animation clips.  Deterministic (millisecond-driven) so the
// whole fight loop is verifiable in host tests.
#pragma once
#include "Character.hpp"
#include <map>
#include <string>

namespace bdae {

struct EnemyStats {
    float hp = 40, moveSpeed = 150, attackRange = 200;
    float noticeRange = 7200;    // f2: vision radius (72 m), same for all enemies
    float rangedRange = 1200;    // f9: ranged-attack distance (gun 1200, rhino 3000)
    uint32_t attackIntervalMs = 2000;
    bool ranged = false;         // Range*/gun archetypes attack from rangedRange
};

// Parses configs/EnemysAttributeConfigs.bin (count, then records of
// [u16-len string][u32/float fields...], record starts at UPPERCASE names) and
// configs/AttackIntervalTimeConfigs.bin for the common melee interval.
// Field mapping (from cross-enemy comparison, documented in FORMAT_BDAE324.md):
//   f0=HP  f1=moveSpeed  f2=noticeRange(vision)  f7=attackRange  f9=rangedRange
std::map<std::string, EnemyStats> loadEnemyStats(const std::string& configsDir,
                                                 std::string& err);

const Clip* pickClip(const Model& m, std::initializer_list<const char*> names);

struct EnemyActor {
    enum State { IDLE, CHASE, ATTACK, COOLDOWN, HURT, DEAD };
    Model* model = nullptr;            // shared archetype model (posed by owner)
    EnemyStats stats;
    float hp = 0;
    State state = IDLE;
    uint32_t stateStartMs = 0;
    float x = 0, y = 0, z = 0, yaw = 0;
    const Clip *cIdle = nullptr, *cWalk = nullptr, *cAttack = nullptr,
               *cHurt = nullptr, *cDie = nullptr;
    const LevelRoom* level = nullptr;

    void bind(Model* m, const LevelRoom* lvl, const EnemyStats& s,
              float px, float py, float pz, float pyaw);
    // Advances the AI. Returns true if this update landed a hit on the hero.
    bool update(uint32_t nowMs, uint32_t dtMs, const Vec3& hero);
    void takeHit(float dmg, uint32_t nowMs);
    bool alive() const { return state != DEAD; }
    // Which clip + shared-timeline time the renderer should pose right now.
    void poseInfo(uint32_t nowMs, const Clip*& clip, uint32_t& timelineMs) const;
};

struct HeroCombat {
    float hp = 100;
    const Clip *cWindup = nullptr, *cRecover = nullptr;
    uint32_t punchStartMs = 0;         // 0 = not punching
    bool hitApplied = false;
    float damage = 10.0f, reach = 260.0f;

    void bind(const Model& hero);
    bool punching(uint32_t nowMs) const;
    void tryPunch(uint32_t nowMs);
    // Applies damage to enemies in the front arc at the strike frame.
    // Returns number of enemies hit this update.
    int update(uint32_t nowMs, const Vec3& pos, float yaw,
               std::vector<EnemyActor>& enemies);
    // Override clip for the renderer while a punch is active.
    bool poseInfo(uint32_t nowMs, const Clip*& clip, uint32_t& timelineMs) const;
};

} // namespace bdae
