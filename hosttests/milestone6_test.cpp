// Milestone 6 headless verification: config-driven combat.
// A knife thug notices Spider-Man, chases him across the real navmesh, attacks
// on the original 2-second interval; Spider-Man punches back and the thug's
// config HP (40) runs out in exactly ceil(40/10)=4 punches.
#include "Combat.hpp"
#include <cstdio>
#include <cmath>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;

    // ------------------------------------------------ config tables
    auto stats = loadEnemyStats(root + "/configs", e);
    std::printf("enemy stat records parsed: %zu\n", stats.size());
    for (auto& kv : stats)
        if (kv.first.rfind("THUG", 0) == 0 || kv.first == "RHINO")
            std::printf("  %-14s hp=%.0f speed=%.0f attackRange=%.0f notice=%.0f ranged=%.0f interval=%ums\n",
                        kv.first.c_str(), kv.second.hp, kv.second.moveSpeed,
                        kv.second.attackRange, kv.second.noticeRange, kv.second.rangedRange,
                        kv.second.attackIntervalMs);
    ck(stats.count("THUG_KNIFE") && stats["THUG_KNIFE"].hp == 40 &&
       stats["THUG_KNIFE"].moveSpeed == 150 && stats["THUG_KNIFE"].attackRange == 200,
       "THUG_KNIFE stats match the raw table (40 hp, 150 u/s, 200 range)");
    ck(stats.count("THUG_GUN") && stats["THUG_GUN"].rangedRange == 1200,
       "THUG_GUN ranged-attack distance is 1200");
    ck(stats["THUG_KNIFE"].noticeRange == 7200,
       "vision radius (f2) is the universal 7200");
    ck(stats.count("RHINO") && stats["RHINO"].hp == 100,
       "RHINO hp 100 (boss-tier confirms field mapping)");
    ck(stats["THUG_KNIFE"].attackIntervalMs == 2000,
       "melee attack interval from AttackIntervalTimeConfigs = 2000 ms");

    // ------------------------------------------------ world + actors
    LevelRoom level;
    if (!level.loadFullLevel(root, "levelnew_01", e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }
    Model thug;
    if (!thug.loadMesh(root + "/entities/meshes_bin/thug_knife_mesh.bdae", e) ||
        !thug.loadAnimation(root + "/entities/meshes_bin/thug_bat_anim.bdae", e)) {
        std::printf("FATAL %s\n", e.c_str()); return 1;
    }
    Model man;
    man.loadMesh(root + "/entities/meshes_bin/spiderman_mesh.bdae", e);
    man.loadAnimation(root + "/entities/meshes_bin/spiderman_anim.bdae", e);

    // hero at the original spawn; thug at the nearest original knife placement
    Vec3 hero = level.spawn; float hz;
    level.canStandAt(hero.x, hero.y, hz); hero.z = hz;
    const LevelRoom::EnemySpawn* sp = nullptr; float best = 1e30f;
    for (auto& en : level.enemies) {
        if (en.type != "MeleeThugEnemy_knife" || en.pos.z > 120) continue;
        float d = std::hypot(en.pos.x - hero.x, en.pos.y - hero.y);
        if (d < best) { best = d; sp = &en; }
    }
    ck(sp != nullptr, "an original knife-thug placement exists near spawn",
       sp ? ("dist " + std::to_string((int)best)) : "");
    std::vector<EnemyActor> foes(1);
    foes[0].bind(&thug, &level, stats["THUG_KNIFE"], sp->pos.x, sp->pos.y, sp->pos.z, sp->yaw);
    std::printf("clips: idle=%s walk=%s attack=%s hurt=%s die=%s\n",
                foes[0].cIdle ? "ok" : "MISSING", foes[0].cWalk ? "ok" : "MISSING",
                foes[0].cAttack ? "ok" : "MISSING", foes[0].cHurt ? "ok" : "MISSING",
                foes[0].cDie ? foes[0].cDie->name.c_str() : "MISSING");
    ck(foes[0].cIdle && foes[0].cAttack && foes[0].cHurt && foes[0].cDie,
       "idle/attack/hurt/death clips all resolve from the original animation set");

    HeroCombat fists;
    fists.bind(man);
    ck(fists.stages[0].windup && fists.stages[0].recover, "hero punch clips resolve");

    // ------------------------------------------------ phase 1: notice + chase
    // Teleport hero within notice range but outside attack range.
    hero.x = foes[0].x + 900; hero.y = foes[0].y;
    level.canStandAt(hero.x, hero.y, hero.z);
    uint32_t t = 0; const uint32_t dt = 16;
    float startDist = std::hypot(foes[0].x - hero.x, foes[0].y - hero.y);
    int heroHits = 0; uint32_t firstHitMs = 0, secondHitMs = 0;
    while (t < 20000 && heroHits < 2) {
        t += dt;
        if (foes[0].update(t, dt, hero)) {
            ++heroHits;
            if (heroHits == 1) firstHitMs = t; else if (heroHits == 2) secondHitMs = t;
        }
    }
    float endDist = std::hypot(foes[0].x - hero.x, foes[0].y - hero.y);
    std::printf("chase: %0.f -> %.0f units; first hit at %ums, second at %ums\n",
                startDist, endDist, firstHitMs, secondHitMs);
    ck(endDist < startDist * 0.4f, "thug closed most of the distance on the navmesh");
    ck(heroHits >= 2, "thug lands repeated attacks");
    ck(secondHitMs - firstHitMs >= foes[0].stats.attackIntervalMs,
       "attacks respect the original 2000 ms interval",
       std::to_string(secondHitMs - firstHitMs) + " ms apart");
    float gz;
    ck(level.canStandAt(foes[0].x, foes[0].y, gz), "thug never left walkable ground");

    // ------------------------------------------------ phase 2: hero fights back
    Vec3 hpos{foes[0].x + 150, foes[0].y, foes[0].z};
    float toThug = std::atan2(foes[0].y - hpos.y, foes[0].x - hpos.x);
    int punches = 0, landed = 0;
    while (foes[0].alive() && punches < 8 && t < 60000) {
        fists.tryPunch(t); ++punches;
        uint32_t end = t + 4000;
        while (t < end) { t += dt; landed += fists.update(t, hpos, toThug, foes);
                          foes[0].update(t, dt, hpos); }
    }
    std::printf("punches thrown %d, landed %d, thug hp %.0f, state %s\n",
                punches, landed, foes[0].hp, foes[0].alive() ? "ALIVE" : "DEAD");
    ck(!foes[0].alive(), "thug defeated");
    ck(punches == 4 && landed == 4, "exactly ceil(40 hp / 10 dmg) = 4 punches",
       std::to_string(punches) + " thrown");
    const Clip* c; uint32_t tl;
    foes[0].poseInfo(t, c, tl);
    ck(c == foes[0].cDie && tl == foes[0].cDie->startMs + (foes[0].cDie->endMs - foes[0].cDie->startMs - 1),
       "corpse holds the last frame of the death clip");

    // punch misses when facing away
    EnemyActor dummy; dummy.bind(&thug, &level, stats["THUG_KNIFE"],
                                 hpos.x + 150, hpos.y, hpos.z, 0);
    std::vector<EnemyActor> d2{dummy};
    // dummy stands EAST of the hero; punching WEST (toward the corpse) must miss it
    fists.punchStartMs = 0; fists.tryPunch(t);
    uint32_t e2 = t + 3000; int wrongWay = 0;
    while (t < e2) { t += dt; wrongWay += fists.update(t, hpos, toThug, d2); }
    ck(wrongWay == 0 && d2[0].hp == 40, "punch arc respects facing (no hit behind)");

    std::printf("\n%s (%d failures)\n", fails ? "MILESTONE 6 SIM FAILED" : "MILESTONE 6 SIM PASSED", fails);
    return fails ? 1 : 0;
}
