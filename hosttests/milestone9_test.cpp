// Milestone 9 headless verification: the living level.
// Original prop placements load, the attack-damage table parses, the 3-hit
// combo chains on real clip timing, knockback shoves enemies along the navmesh.
#include "GameFlow.hpp"
#include "Combat.hpp"
#include <cstdio>
#include <cmath>
#include <map>
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

    LevelRoom l1;
    if (!l1.loadFullLevel(root, "levelnew_01", e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }

    // ---------------- props ----------------
    std::map<std::string, int> byType;
    for (auto& p : l1.props) ++byType[p.type];
    std::printf("props: total %zu | ", l1.props.size());
    for (auto& kv : byType) std::printf("%s %d  ", kv.first.c_str(), kv.second);
    std::printf("| bonuses %zu\n", l1.bonuses.size());
    ck(l1.props.size() >= 180, "hundreds of original prop placements parsed");
    ck(byType["DestroyableObject"] >= 100, "destructibles present in force");
    ck(l1.bonuses.size() >= 80, "bonus pickups parsed", std::to_string(l1.bonuses.size()));

    std::set<std::string> unique;
    for (auto& p : l1.props) unique.insert(p.meshFile);
    int loaded = 0, skinnedProps = 0, failed = 0;
    std::string firstFail;
    for (auto& mf : unique) {
        Model m; std::string pe;
        std::string full = resolveCaseInsensitive(root + "/" + mf);
        bool ok = m.loadMesh(full, pe);
        if (!ok) {   // original app mounts every level pack; try the sibling level
            std::string alt = mf;
            size_t lp = alt.find("levelnew_01");
            if (lp != std::string::npos) alt.replace(lp, 11, "levelnew_02");
            ok = m.loadMesh(resolveCaseInsensitive(root + "/" + alt), pe);
        }
        if (ok) { ++loaded; if (m.skin.valid) ++skinnedProps; }
        else { ++failed; firstFail += mf + " "; }
    }
    std::printf("prop meshes: %zu unique, %d load (%d skinned), %d fail %s\n",
                unique.size(), loaded, skinnedProps, failed, firstFail.c_str());
    ck(failed == 2 && loaded >= 50,
       "50/52 prop meshes load; break_*wall ship only as _anim destruction states",
       firstFail);

    // ---------------- attack damage table ----------------
    auto dmg = loadAttackDamage(root + "/configs", e);
    int nz = 0; std::string samples;
    for (auto& kv : dmg)
        if (kv.second > 0 && kv.second <= 60) {
            ++nz;
            if (samples.size() < 120) samples += kv.first + "=" + std::to_string((int)kv.second) + " ";
        }
    std::printf("attack rows: %zu, plausible damage rows: %d\n  e.g. %s\n", dmg.size(), nz, samples.c_str());
    ck(dmg.size() >= 60, "attack table parses");
    ck(dmg.count("ATTACK_NONE") && dmg["ATTACK_NONE"] == 0, "null rows carry zero damage");
    ck(nz >= 8, "real damage values recovered");

    // ---------------- combo chain ----------------
    Model man;
    man.loadMesh(root + "/entities/meshes_bin/spiderman_mesh.bdae", e);
    man.loadAnimation(root + "/entities/meshes_bin/spiderman_anim.bdae", e);
    HeroCombat fists; fists.bind(man);
    ck(fists.stages[0].windup && fists.stages[1].windup && fists.stages[2].windup,
       "all three combo stages resolve to original clips");

    auto stats = loadEnemyStats(root + "/configs", e);
    Model thug;
    thug.loadMesh(root + "/entities/meshes_bin/thug_knife_mesh.bdae", e);
    thug.loadAnimation(root + "/entities/meshes_bin/thug_bat_anim.bdae", e);
    EnemyActor foe;
    foe.bind(&thug, &l1, stats["THUG_KNIFE"], l1.spawn.x + 150, l1.spawn.y, l1.spawn.z, 0);
    std::vector<EnemyActor> foes{foe};
    Vec3 pos = l1.spawn; float yaw = 0;

    uint32_t t = 1000; int landed = 0; int stagesSeen = 0; int lastStage = -1;
    fists.tryPunch(t);
    for (int i = 0; i < 4000 && foes[0].hp > 0; ++i) {
        t += 16;
        landed += fists.update(t, pos, yaw, foes);
        if (fists.punchStartMs && fists.stage != lastStage) { lastStage = fists.stage; ++stagesSeen; }
        if (fists.hitApplied && fists.punchStartMs) fists.tryPunch(t);   // player mashing = chain
        foes[0].update(t, 16, pos);
        if (!fists.punchStartMs) fists.tryPunch(t);
    }
    std::printf("combo fight: %d hits landed, stages seen %d, thug hp %.0f\n",
                landed, stagesSeen, foes[0].hp);
    ck(stagesSeen >= 3, "combo chains through all three stages");
    ck(landed >= 4 && foes[0].hp <= 0, "10+10+15 chain plus follow-up drops a 40 hp thug");

    // ---------------- knockback ----------------
    // anchor the shove test on the navmesh via the same projection the
    // character uses, and pick a push direction the nav actually allows
    Vec3 w0{};
    ck(l1.nearestWalkable(l1.spawn.x, l1.spawn.y, 600.0f, w0), "walkable anchor near spawn");
    float px = 0, py = 0, gz2;
    for (int k = 0; k < 8; ++k) {
        float a = k * 0.7853981f;
        if (l1.canStandAt(w0.x + std::cos(a) * 90.0f, w0.y + std::sin(a) * 90.0f, gz2)) {
            px = std::cos(a); py = std::sin(a); break;
        }
    }
    ck(px != 0 || py != 0, "an open push direction exists on the nav");
    EnemyActor kb;
    kb.bind(&thug, &l1, stats["THUG_KNIFE"], w0.x, w0.y, w0.z, 0);
    float d0 = 0;
    kb.takeHit(10, t, w0.x - px * 150.0f, w0.y - py * 150.0f);   // attacker behind
    float d1 = std::hypot(kb.x - w0.x, kb.y - w0.y);
    float gz;
    std::printf("knockback: %.0f -> %.0f units\n", d0, d1);
    ck(d1 > 40, "hit shoves the enemy back");
    ck(l1.canStandAt(kb.x, kb.y, gz), "knockback stays on walkable ground");

    std::printf("\n%s (%d failures)\n", fails ? "MILESTONE 9 SIM FAILED" : "MILESTONE 9 SIM PASSED", fails);
    return fails ? 1 : 0;
}
