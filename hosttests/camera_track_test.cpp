// PlayDAEAnim camera animations: the camera BDAEs decode to a moving path.
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
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;
    CameraTrack ct;
    ck(ct.load(root + "/levelnew_01/meshes_bin/Camera_Lv1_End.bdae", e), "Camera_Lv1_End.bdae loads as a camera track", e);
    ck(ct.eyeNode >= 0 && ct.targetNode >= 0 && ct.eyeNode != ct.targetNode, "eye and target nodes identified",
       std::to_string(ct.eyeNode) + "/" + std::to_string(ct.targetNode));
    ck(ct.durationMs > 30000 && ct.durationMs < 60000, "track is tens of seconds long", std::to_string(ct.durationMs));
    Vec3 e0, t0, e1, t1;
    ck(ct.sample(0, e0, t0) && ct.sample(ct.durationMs / 2, e1, t1), "samples at start and midpoint");
    float moved = std::sqrt((e1.x - e0.x) * (e1.x - e0.x) + (e1.y - e0.y) * (e1.y - e0.y) + (e1.z - e0.z) * (e1.z - e0.z));
    ck(moved > 50.0f, "the eye moves along the track", std::to_string(moved));
    float sep = std::sqrt((t0.x - e0.x) * (t0.x - e0.x) + (t0.y - e0.y) * (t0.y - e0.y) + (t0.z - e0.z) * (t0.z - e0.z));
    ck(sep > 10.0f, "target is distinct from the eye", std::to_string(sep));
    Vec3 e2, t2;
    ck(ct.sample(ct.durationMs + 99999, e2, t2), "sampling past the end clamps");

    // every camera animation a script asks for ships and loads
    std::set<std::string> files;
    std::string dir = root + "/levelnew_01/cinematics";
    DIR* d = opendir(dir.c_str());
    while (dirent* en = readdir(d)) {
        std::string n = en->d_name;
        if (n.size() < 5 || n.compare(n.size() - 4, 4, ".cff") != 0) continue;
        Cinematic c; if (!c.load(dir + "/" + n, e)) continue;
        for (auto& a : c.daeAnims()) {
            std::string f = a.file;
            while (f.rfind("./", 0) == 0 || f.rfind("../", 0) == 0) f = f.substr(f.find('/') + 1);
            files.insert(f);
        }
    }
    closedir(d);
    int loadable = 0, cameras = 0; std::string firstBad;
    for (auto& f : files) {
        CameraTrack t2s; std::string le;
        std::string p = resolveCaseInsensitive(root + "/levelnew_01/" + f);
        Model probe; std::string pe;
        bool anim = probe.loadAnimation(p, pe);
        if (anim) ++loadable; else if (firstBad.empty()) firstBad = f;
        if (f.find("amera") != std::string::npos && t2s.load(p, le)) ++cameras;
    }
    std::printf("  PlayDAEAnim files: %zu distinct, %d load, %d are camera tracks\n", files.size(), loadable, cameras);
    ck(!files.empty() && loadable == (int)files.size(), "every PlayDAEAnim file loads", firstBad);
    // the standalone camera tracks the level ships (referenced by the engine, not by PlayDAEAnim)
    int shipped = 0;
    for (const char* f : {"Camera_Lv1_End.bdae", "Camera_Lv1_Gameover.bdae"}) {
        CameraTrack t3; std::string le;
        if (t3.load(resolveCaseInsensitive(root + "/levelnew_01/meshes_bin/" + f), le)) ++shipped;
    }
    ck(shipped == 2, "both shipped Level 1 camera tracks decode");
    std::printf("\n%s (%d failures)\n", fails ? "CAMERA TRACK FAILED" : "CAMERA TRACK PASSED", fails);
    return fails ? 1 : 0;
}
