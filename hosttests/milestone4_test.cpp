// Headless run of the Milestone 4 integrated scene: original skinned Spider-Man,
// original Level 1 Room 1, spawn from the original scene data, navmesh ground
// collision, and animation state switching driven by simulated stick input.
#include "Character.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstring>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    Model man; std::string e;
    if (!man.loadMesh(root + "/entities/meshes_bin/spiderman_mesh.bdae", e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }
    if (!man.loadAnimation(root + "/entities/meshes_bin/spiderman_anim.bdae", e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }

    LevelRoom room;
    if (!room.load(root, "levelnew_01", {"levelnew_01.irr", "levelnew_01_0_Room1.irr"}, e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }
    std::printf("Level 1 / Room 1: visual %zu tris, collision %zu tris, navmesh %zu tris\n",
                room.visualTriangleCount(), room.collision.indices.size()/3, room.navmesh.indices.size()/3);
    std::printf("spawn from original scene data: hasSpawn=%d (%.1f %.1f %.1f) yaw=%.2f\n",
                (int)room.hasSpawn, room.spawn.x, room.spawn.y, room.spawn.z, room.spawnYaw);
    ck(room.hasSpawn, "SpiderMan spawn node found in the original level scene");

    // measure the authored stride so the locomotion speed is not invented
    const Clip* run = man.findClip("run");
    int lf = -1; for (size_t i = 0; i < man.nodes.size(); ++i) if (man.nodes[i].name == "Bip01_L_Foot") lf = (int)i;
    float minY = 1e30f, maxY = -1e30f;
    for (uint32_t t = run->startMs; t <= run->endMs; t += 10) {
        man.poseAtTime(t);
        float y = man.worldTransforms()[lf].m[13];
        minY = std::min(minY, y); maxY = std::max(maxY, y);
    }
    float stride = maxY - minY, cycle = (run->endMs - run->startMs) / 1000.0f;
    std::printf("authored run stride = %.1f units over %.2fs -> ~%.0f units/s\n", stride, cycle, stride / cycle * 2.0f);

    Character hero;
    ck(hero.bind(&man, &room), "character bound to idle_stand / walk / run clips");
    hero.runSpeed = stride / cycle * 2.0f;
    hero.walkSpeed = hero.runSpeed * 0.33f;
    hero.spawnAt(room.spawn, room.spawnYaw);
    std::printf("after ground snap: (%.1f %.1f %.1f) grounded=%d\n", hero.position().x, hero.position().y, hero.position().z, (int)hero.grounded());
    ck(hero.grounded(), "spawn point snaps onto the original navmesh");

    // 6 seconds at 60 Hz: idle, walk, run, turn, then stop
    int offNav = 0, moved = 0; float startX = hero.position().x, startY = hero.position().y;
    std::string seen;
    const char* last = "";
    for (int f = 0; f < 360; ++f) {
        // head from the entrance toward the room interior (where the thugs are)
        float tx = 12830.0f - hero.position().x, ty = -7421.0f - hero.position().y;
        float tl = std::sqrt(tx * tx + ty * ty); if (tl > 1) { tx /= tl; ty /= tl; }
        float mx = 0, my = 0;
        if (f >= 60 && f < 120)       { mx = tx * 0.35f; my = ty * 0.35f; }
        else if (f >= 120 && f < 300) { mx = tx; my = ty; }
        hero.update(1.0f / 60.0f, mx, my);
        float z;
        if (!room.canStandAt(hero.position().x, hero.position().y, z)) ++offNav;
        if (std::strcmp(last, hero.stateName())) { seen += std::string(last[0] ? " -> " : "") + hero.stateName(); last = hero.stateName(); }
        if (f == 359) moved = (int)std::sqrt(std::pow(hero.position().x - startX, 2) + std::pow(hero.position().y - startY, 2));
    }
    std::printf("state sequence: %s\n", seen.c_str());
    std::printf("travelled %d units, ended at (%.1f %.1f %.1f)\n", moved, hero.position().x, hero.position().y, hero.position().z);
    ck(offNav == 0, "player never leaves walkable ground during 360 simulated frames",
       std::to_string(offNav) + " off-navmesh frames");
    ck(moved > 100, "player actually traverses the room", std::to_string(moved) + " units");
    ck(seen.find("walk") != std::string::npos && seen.find("run") != std::string::npos,
       "locomotion state machine reaches walk and run");

    // skinned silhouette must stay sane while moving
    float worstH = 0; const Mesh& mesh = man.meshes.front();
    for (int f = 0; f < 60; ++f) {
        hero.update(1.0f / 60.0f, 1.0f, 0.0f);
        const auto& S = man.skinningMatrices();
        float lo = 1e30f, hi = -1e30f;
        for (const Vertex& v : mesh.vertices) {
            float z = 0;
            for (int k = 0; k < 4; ++k) if (v.weight[k] > 0)
                z += transformPoint(S[v.bone[k]], Vec3{v.px, v.py, v.pz}).z * v.weight[k];
            lo = std::min(lo, z); hi = std::max(hi, z);
        }
        worstH = std::max(worstH, std::fabs((hi - lo) - 140.0f));
    }
    ck(worstH < 60.0f, "skinned character height stays humanoid while running",
       "worst deviation from 140 units = " + std::to_string(worstH));

    std::printf("\n%s (%d failures)\n", fails ? "MILESTONE 4 SIM FAILED" : "MILESTONE 4 SIM PASSED", fails);
    return fails ? 1 : 0;
}
