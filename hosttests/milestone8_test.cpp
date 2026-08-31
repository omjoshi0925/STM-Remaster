// Milestone 8 headless verification: both levels + level flow.
#include "GameFlow.hpp"
#include "Combat.hpp"
#include <cstdio>
#include <cmath>
#include <set>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;

    // ---------------- Level 2 loads with full data ----------------
    LevelRoom l2;
    ck(l2.loadFullLevel(root, "levelnew_02", e), "LEVEL 2 loads end to end", e);
    std::printf("LEVEL 2: %zu batches, %zu verts, %zu tris | collision %zu | nav %zu | enemies %zu | markers %zu\n",
                l2.visualBatches.size(), l2.visualVertexCount(), l2.visualTriangleCount(),
                l2.collision.indices.size() / 3, l2.navmesh.indices.size() / 3,
                l2.enemies.size(), l2.markers.size());
    ck(l2.visualTriangleCount() > 15000, "L2 visual geometry is a full level");
    ck(l2.hasSpawn, "L2 spawn present");
    float z2;
    ck(l2.canStandAt(l2.spawn.x, l2.spawn.y, z2), "L2 spawn is on the merged navmesh");
    int cps = 0;
    for (auto& mk : l2.markers) if (mk.first == "CheckPoint") ++cps;
    ck(cps >= 10, "L2 has its checkpoint chain", std::to_string(cps) + " checkpoints");

    // every enemy type in BOTH levels maps to a runtime archetype
    static const char* known[] = {"MeleeThugEnemy_bat", "MeleeThugEnemy_knife",
                                  "RangeThug_molotov", "MeleeThug_gun",
                                  "RangeThug_hammer", "RangeThug_big",
                                  "Boss_Sandman", "Boss_Rhino"};
    LevelRoom l1;
    l1.loadFullLevel(root, "levelnew_01", e);
    std::set<std::string> unmapped;
    for (const LevelRoom* L : {&l1, &l2})
        for (auto& en : L->enemies) {
            bool ok = false;
            for (const char* k : known) if (en.type.rfind(k, 0) == 0) ok = true;
            if (!ok) unmapped.insert(en.type);
        }
    std::string um;
    for (auto& s : unmapped) um += s + " ";
    ck(unmapped.empty(), "every enemy placement in L1+L2 maps to an archetype", um);

    // ---------------- strings ----------------
    StringTable strs;
    ck(strs.load(root + "/xlsStrings/MAIN.map", root + "/xlsStrings/MAIN_EN.data", e),
       "xlsStrings key/value tables pair up", e);
    std::printf("  %zu localized strings; STR_GAME_NAME %s\n", strs.byKey.size(),
                strs.byKey.count("STR_GAME_NAME") ? "present" : "MISSING");
    ck(strs.byKey.size() > 300 && strs.byKey.count("STR_GAME_NAME"),
       "string table is the real localization set");

    // ---------------- flow: L1 start->finish->L2, death->respawn ----------------
    GameFlow flow;
    uint32_t t = 1000;
    flow.beginLevel(l1, 0, t);
    ck(flow.phase == GameFlow::TITLE && flow.comicFirst == 1 && flow.checkpointsAll.size() >= 10,
       "L1 session opens on the chapter card with the checkpoint chain (boot videos are renderer-side)",
       std::to_string(flow.checkpointsAll.size()) + " checkpoints");
    flow.startPlay(t += 2000);
    // walk the checkpoint chain (teleport visits: flow logic under test, not locomotion)
    bool completed = false;
    for (auto& cp : flow.checkpointsAll) {
        t += 500;
        completed = flow.updatePlaying(cp, t) || completed;
    }
    ck(completed && flow.phase == GameFlow::COMPLETE,
       "visiting the full original checkpoint chain completes the level",
       std::to_string(flow.visitedCount()) + "/" + std::to_string(flow.checkpointsAll.size()));

    flow.beginLevel(l2, 1, t += 1000);
    flow.startPlay(t += 1000);
    Vec3 mid = flow.checkpointsAll[flow.checkpointsAll.size() / 2];
    flow.updatePlaying(flow.checkpointsAll[0], t += 500);
    flow.updatePlaying(mid, t += 500);
    Vec3 before = flow.checkpoint;
    flow.onDeath(t += 500);
    ck(flow.phase == GameFlow::DEAD, "death enters the DEAD phase");
    flow.respawn(t += 1500);
    ck(flow.phase == GameFlow::PLAYING &&
       before.x == flow.checkpoint.x && before.y == flow.checkpoint.y,
       "respawn returns to the last reached checkpoint");

    // ---------------- stats table completeness + bosses ----------------
    auto stats = loadEnemyStats(root + "/configs", e);
    std::printf("  stat records: %zu (SANDMAN %s, RHINO %s)\n", stats.size(),
                stats.count("SANDMAN") ? "yes" : "NO", stats.count("RHINO") ? "yes" : "NO");
    ck(stats.size() >= 20 && stats.count("SANDMAN") && stats.count("THUG_MOLOTOV"),
       "stats parser recovers the full 25-record table");
    for (auto bossFile : {"sandman", "rhino"}) {
        Model bm;
        std::string mp = root + "/entities/meshes_bin/" + std::string(bossFile) + "_mesh.bdae";
        std::string ap = root + "/entities/meshes_bin/" + std::string(bossFile) + "_anim.bdae";
        bool ok = bm.loadMesh(mp, e) && bm.loadAnimation(ap, e);
        EnemyActor ba;
        if (ok) ba.bind(&bm, &l1, stats.count("SANDMAN") ? stats["SANDMAN"] : EnemyStats{}, 0, 0, 0, 0);
        ck(ok && ba.cIdle && ba.cAttack && bm.skin.valid && bm.skin.jointNode.size() <= 40,
           (std::string("boss loads + fits bone budget: ") + bossFile).c_str(),
           ok ? std::to_string(bm.skin.jointNode.size()) + " joints" : e);
    }

    // ---------------- ranged enemies keep their distance ----------------
    EnemyStats gun = stats.count("THUG_GUN") ? stats["THUG_GUN"] : EnemyStats{};
    gun.ranged = true;
    Model gm;
    gm.loadMesh(root + "/entities/meshes_bin/thug_gun_mesh.bdae", e);
    gm.loadAnimation(root + "/entities/meshes_bin/thug_gun_anim.bdae", e);
    EnemyActor g;
    g.bind(&gm, &l1, gun, l1.spawn.x + 900, l1.spawn.y, l1.spawn.z, 0);
    ck(g.cIdle && g.cAttack, "gun thug clips resolve");
    Vec3 hero = l1.spawn;
    uint32_t hits = 0; float minDist = 1e9f;
    for (uint32_t tt = 0; tt < 15000; tt += 16) {
        if (g.update(tt, 16, hero)) ++hits;
        minDist = std::fmin(minDist, std::hypot(g.x - hero.x, g.y - hero.y));
    }
    std::printf("  gun thug: %u ranged hits, closest approach %.0f units\n", hits, minDist);
    ck(hits >= 2, "gun thug attacks from range on the interval");
    ck(minDist > 600.0f, "gun thug never closes to melee (fires from ~1200)");

    std::printf("\n%s (%d failures)\n", fails ? "MILESTONE 8 SIM FAILED" : "MILESTONE 8 SIM PASSED", fails);
    return fails ? 1 : 0;
}
