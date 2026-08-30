// Milestone 5 headless verification: the ENTIRE original level 1 (all rooms),
// vertex colours, enemy roster with meshes/animations, and a long free-roam
// simulation across the merged navmesh.
#include "Character.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <map>
#include <set>
#include <algorithm>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;

    // ---------------- full level ----------------
    LevelRoom level;
    if (!level.loadFullLevel(root, "levelnew_01", e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }
    std::printf("FULL LEVEL 1: %zu visual batches, %zu verts, %zu tris | collision %zu tris | nav %zu tris\n",
                level.visualBatches.size(), level.visualVertexCount(), level.visualTriangleCount(),
                level.collision.indices.size() / 3, level.navmesh.indices.size() / 3);
    std::printf("  enemies: %zu   markers: %zu   spawn: (%.0f %.0f %.0f)\n",
                level.enemies.size(), level.markers.size(), level.spawn.x, level.spawn.y, level.spawn.z);
    ck(level.visualBatches.size() >= 2, "visual geometry spans multiple batches");
    ck(level.visualTriangleCount() > 20000, "full level exceeds 20k visual triangles",
       std::to_string(level.visualTriangleCount()) + " tris");
    for (const TriMesh& b : level.visualBatches)
        if (b.vertices.size() > 60000) { ck(false, "batch exceeds uint16 budget"); break; }
    ck(level.navmesh.indices.size() / 3 > 100, "merged navmesh has full-level coverage",
       std::to_string(level.navmesh.indices.size() / 3) + " tris");
    ck(level.hasSpawn, "SpiderMan spawn present");
    ck(level.enemies.size() >= 60, "level enemy roster parsed",
       std::to_string(level.enemies.size()) + " placements");

    // vertex colours must be meaningful (baked lighting), not all white
    std::set<uint32_t> distinct;
    for (const TriMesh& b : level.visualBatches)
        for (size_t i = 0; i < b.vertices.size(); i += 7) {
            uint32_t c;
            std::memcpy(&c, b.vertices[i].color, 4);
            distinct.insert(c);
            if (distinct.size() > 300) break;
        }
    ck(distinct.size() > 100, "baked vertex lighting decoded",
       std::to_string(distinct.size()) + "+ distinct colours");

    // every ground-level enemy placement must be walkable (cross-validation)
    int walk = 0, ground = 0;
    for (auto& en : level.enemies) {
        float z;
        if (en.pos.z < 120.0f) { ++ground; if (level.canStandAt(en.pos.x, en.pos.y, z)) ++walk; }
    }
    std::printf("  ground-level enemy placements on navmesh: %d/%d\n", walk, ground);
    ck(walk >= ground * 3 / 4, "most ground enemies stand on the merged navmesh");

    // ---------------- enemy characters ----------------
    struct Roster { const char* type; const char* mesh; const char* anim; };
    const Roster roster[] = {
        {"MeleeThugEnemy_bat",   "thug_bat_mesh.bdae",     "thug_bat_anim.bdae"},
        {"MeleeThugEnemy_knife", "thug_knife_mesh.bdae",   "thug_bat_anim.bdae"},
        {"RangeThug_molotov",    "thug_molotov_mesh.bdae", "thug_molotov_anim.bdae"},
    };
    for (const Roster& r : roster) {
        Model m;
        std::string me = root + "/entities/meshes_bin/";
        bool ok = m.loadMesh(me + r.mesh, e) && m.loadAnimation(me + r.anim, e);
        int bound = 0;
        for (auto& ch : m.channels) if (ch.node >= 0) ++bound;
        const Clip* idle = m.findClip("idle");
        std::string det;
        if (ok) det = std::to_string(m.meshes.front().vertices.size()) + " verts, " +
                      std::to_string(m.skin.jointNode.size()) + " joints, " +
                      std::to_string(bound) + "/" + std::to_string(m.channels.size()) +
                      " channels bound, idle=" + (idle ? "yes" : "NO");
        ck(ok && idle && bound == (int)m.channels.size() && m.skin.valid,
           (std::string("enemy loads + animates: ") + r.type).c_str(), ok ? det : e);
        if (ok && idle) {
            // Enemy clips are authored at arbitrary scene offsets (thug idle sits
            // at Z~320), so measure and place via the skinned anchor.
            Vec3 anchor = skinnedAnchor(m, idle->startMs);
            const Mesh& mesh = m.meshes.front();
            const auto& S = m.skinningMatrices();
            float lo = 1e30f, hi = -1e30f;
            for (const Vertex& v : mesh.vertices) {
                float z = 0;
                for (int k = 0; k < 4; ++k) if (v.weight[k] > 0)
                    z += transformPoint(S[v.bone[k]], Vec3{v.px, v.py, v.pz}).z * v.weight[k];
                lo = std::min(lo, z); hi = std::max(hi, z);
            }
            ck(hi - lo > 100.0f && hi - lo < 260.0f,
               (std::string("  idle skinned height humanoid: ") + r.type).c_str(),
               std::to_string(hi - lo) + " units");
            ck(std::fabs(lo - anchor.z) < 1.0f,
               (std::string("  anchor grounds the character: ") + r.type).c_str(),
               "clip offset Z=" + std::to_string(anchor.z));
        }
    }

    // ---------------- hero long roam over the whole level ----------------
    Model man;
    if (!man.loadMesh(root + "/entities/meshes_bin/spiderman_mesh.bdae", e) ||
        !man.loadAnimation(root + "/entities/meshes_bin/spiderman_anim.bdae", e)) {
        std::printf("FATAL %s\n", e.c_str()); return 1;
    }
    Character hero;
    hero.bind(&man, &level);
    hero.spawnAt(level.spawn, level.spawnYaw);
    ck(hero.grounded(), "hero grounded at spawn");

    // roam toward a sequence of real waypoints/checkpoints across rooms
    std::vector<Vec3> targets;
    for (auto& mk : level.markers)
        if (mk.first == "CheckPoint" && mk.second.z < 400.0f) targets.push_back(mk.second);
    std::printf("  roaming toward %zu ground checkpoints\n", targets.size());
    int offNav = 0, reached = 0;
    float travelled = 0;
    Vec3 prev = hero.position();
    size_t ti = 0;
    for (int f = 0; f < 3600 && ti < targets.size(); ++f) {   // up to 60 sim-seconds
        Vec3 p = hero.position();
        float dx = targets[ti].x - p.x, dy = targets[ti].y - p.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 120.0f) { ++reached; ++ti; continue; }
        hero.update(1.0f / 60.0f, dx / len, dy / len);
        float z;
        if (!level.canStandAt(hero.position().x, hero.position().y, z)) ++offNav;
        Vec3 q = hero.position();
        travelled += std::sqrt((q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y));
        prev = q;
    }
    std::printf("  travelled %.0f units, reached %d/%zu checkpoints (straight-line steering, no pathfinding)\n",
                travelled, reached, targets.size());
    ck(offNav == 0, "hero never leaves walkable ground while roaming",
       std::to_string(offNav) + " off-nav frames");
    ck(travelled > 2000.0f, "hero traverses a substantial part of the level",
       std::to_string((int)travelled) + " units");
    ck(reached >= 1, "at least one original checkpoint reached by straight-line steering");

    std::printf("\n%s (%d failures)\n", fails ? "MILESTONE 5 SIM FAILED" : "MILESTONE 5 SIM PASSED", fails);
    return fails ? 1 : 0;
}
