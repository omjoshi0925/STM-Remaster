// Camera tracks: the camera BDAEs decode to a moving path, and every
// PlayDAECamera a script issues resolves to one.
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

    // Scripts ask for their camera with PlayDAECamera on the Basic thread
    // (not PlayDAEAnim): every requested camera file ships, decodes as a
    // track, and every chained ^ID^Cinematic^Next resolves to a Cinematic node.
    int scripts = 0, withCamera = 0, tracksLoad = 0, chained = 0, chainsResolve = 0, levelEnds = 0;
    float farMin = 1e9f, farMax = 0;
    for (const char* lv : { "levelnew_01", "levelnew_02" }) {
        LevelRoom room; std::string le;
        if (!room.loadFullLevel(root, lv, le)) { ck(false, "level loads", le); continue; }
        std::string dir = root + "/" + lv + "/cinematics";
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        while (dirent* en = readdir(d)) {
            std::string n = en->d_name;
            if (n.size() < 5 || n.compare(n.size() - 4, 4, ".cff") != 0) continue;
            Cinematic c; if (!c.load(dir + "/" + n, e)) continue;
            ++scripts;
            Cinematic::CameraRequest cr;
            if (!c.cameraRequest(cr)) continue;
            ++withCamera;
            std::string f = cr.file;
            while (f.rfind("./", 0) == 0 || f.rfind("../", 0) == 0) f = f.substr(f.find('/') + 1);
            CameraTrack t2s; std::string te;
            std::string p = resolveCaseInsensitive(root + "/" + lv + "/" + f);
            bool ok = t2s.load(p, te);
            if (ok) ++tracksLoad; else std::printf("    %s: %s (%s)\n", n.c_str(), f.c_str(), te.c_str());
            std::printf("    %-34s %-36s %s %5.1fs far %.0f next %d%s\n", n.c_str(), f.c_str(), ok ? "track" : "FAIL ",
                        t2s.durationMs / 1000.0, cr.farPlane, cr.nextCinematic, cr.levelEnd ? "  [level end]" : "");
            if (cr.farPlane > 0) { farMin = std::fmin(farMin, cr.farPlane); farMax = std::fmax(farMax, cr.farPlane); }
            if (cr.levelEnd) ++levelEnds;
            if (cr.nextCinematic >= 0) { ++chained; if (room.cinematicById.count(cr.nextCinematic)) ++chainsResolve; }
        }
        closedir(d);
    }
    char b[160];
    snprintf(b, sizeof b, "%d scripts, %d with PlayDAECamera, %d tracks load, %d chained (%d resolve), %d level ends, far %.0f..%.0f",
             scripts, withCamera, tracksLoad, chained, chainsResolve, levelEnds, farMin, farMax);
    ck(withCamera >= 5, "PlayDAECamera appears in the set-piece scripts of both levels", b);
    ck(tracksLoad == withCamera, "every requested camera file ships and decodes as a track");
    ck(chained >= 1 && chainsResolve == chained, "every ^ID^Cinematic^Next names a Cinematic node the level knows");
    ck(levelEnds >= 2, "the level-end scripts are flagged (Level 1 end, Level 2 end)");
    // no PlayDAEAnim names a camera file: the old scan for one found nothing
    int camByAnim = 0;
    std::string dir1 = root + "/levelnew_01/cinematics";
    if (DIR* d = opendir(dir1.c_str())) {
        while (dirent* en = readdir(d)) {
            std::string n = en->d_name;
            if (n.size() < 5 || n.compare(n.size() - 4, 4, ".cff") != 0) continue;
            Cinematic c; if (!c.load(dir1 + "/" + n, e)) continue;
            for (auto& a : c.daeAnims()) { std::string f = a.file; for (char& ch : f) ch = (char)tolower(ch);
                if (f.find("/camera_") != std::string::npos) ++camByAnim; }
        }
        closedir(d);
    }
    ck(camByAnim == 0, "camera files are never requested through PlayDAEAnim", std::to_string(camByAnim));
    std::printf("\n%s (%d failures)\n", fails ? "CAMERA TRACK FAILED" : "CAMERA TRACK PASSED", fails);
    return fails ? 1 : 0;
}
