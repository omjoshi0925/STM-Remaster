// Per-cinematic character animation files bind to their character meshes.
#include "Cinematic.hpp"
#include "Level.hpp"
#include <cmath>
#include <cstdio>
#include <dirent.h>
#include <set>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
static float poseDelta(Model& m) {
    const Clip& c0 = m.clips[0];
    m.poseAtTime(c0.startMs); std::vector<Mat4> A = m.worldTransforms();
    m.poseAtTime(c0.startMs + (c0.endMs - c0.startMs) / 2); std::vector<Mat4> B = m.worldTransforms();
    float d = 0; for (size_t i = 0; i < A.size() && i < B.size(); ++i) d += std::fabs(A[i].m[12] - B[i].m[12]) + std::fabs(A[i].m[14] - B[i].m[14]);
    return d;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;
    struct { const char* mesh; const char* anim; } pairs[] = {
        {"entities/meshes_bin/spiderman_mesh.bdae", "levelnew_01/meshes_bin/spiderman_lv1_start.bdae"},
        {"entities/meshes_bin/spiderman_mesh.bdae", "levelnew_01/meshes_bin/spiderman_lv1_end.bdae"},
        {"entities/meshes_bin/rhino_mesh.bdae",     "levelnew_01/meshes_bin/rhino_lv1_end.bdae"},
        {"entities/meshes_bin/thug_bat_mesh.bdae",  "levelnew_01/meshes_bin/thug_bat01_lv1_start.bdae"},
    };
    for (auto& p : pairs) {
        Model m; std::string me;
        bool ok = m.loadMesh(root + "/" + p.mesh, me);
        me.clear();
        ok = ok && m.loadAnimation(resolveCaseInsensitive(root + "/" + p.anim), me) && !m.clips.empty() && m.skin.valid;
        float delta = ok ? poseDelta(m) : 0;
        char d[128]; snprintf(d, sizeof d, "%zu clips, %zu joints, %.1f s, pose delta %.0f",
                              m.clips.size(), m.skin.jointNode.size(),
                              m.clips.empty() ? 0.0 : (m.clips[0].endMs - m.clips[0].startMs) / 1000.0, delta);
        ck(ok && delta > 100.0f, p.anim, ok ? d : me);
    }
    // the player thread's PlayDAEAnim resolves through daeAnimAt
    Cinematic c; int scripted = 0;
    std::string dir = root + "/levelnew_01/cinematics";
    DIR* dd = opendir(dir.c_str());
    while (dirent* en = readdir(dd)) {
        std::string n = en->d_name;
        if (n.size() < 5 || n.compare(n.size() - 4, 4, ".cff") != 0) continue;
        if (!c.load(dir + "/" + n, e)) continue;
        const CineThread* pt = c.thread(3);
        Cinematic::DaeAnim da;
        if (pt && c.daeAnimAt(pt->objectId, c.durationMs, da)) ++scripted;
    }
    closedir(dd);
    ck(scripted >= 4, "player-thread PlayDAEAnim resolves in the scripts that use it", std::to_string(scripted));
    std::printf("\n%s (%d failures)\n", fails ? "DAE ANIM FAILED" : "DAE ANIM PASSED", fails);
    return fails ? 1 : 0;
}
