// Cinematic scripts: every .cff in Levels 1 and 2 parses, the documented
// sample decodes to the documented values, and every sound they name exists.
#include "Cinematic.hpp"
#include "Audio.hpp"
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
    VoxTable vox; vox.load(root + "/configs", e);

    for (const char* lvl : {"levelnew_01", "levelnew_02"}) {
        std::string dir = root + "/" + lvl + "/cinematics";
        DIR* d = opendir(dir.c_str());
        if (!d) { ck(false, lvl, "no cinematics dir"); continue; }
        int files = 0, ok = 0, cmds = 0; uint32_t longest = 0;
        std::set<std::string> names, sounds;
        std::string firstErr;
        while (dirent* en = readdir(d)) {
            std::string n = en->d_name;
            if (n.size() < 5 || n.compare(n.size() - 4, 4, ".cff") != 0) continue;
            ++files;
            Cinematic c; std::string ce;
            if (!c.load(dir + "/" + n, ce)) { if (firstErr.empty()) firstErr = n + ": " + ce; continue; }
            ++ok;
            for (auto& th : c.threads) for (auto& cm : th.commands) { ++cmds; names.insert(cm.name); }
            for (auto& s : c.soundsBetween(0, c.durationMs + 1)) sounds.insert(s);
            longest = std::max(longest, c.durationMs);
        }
        closedir(d);
        std::printf("%s: %d scripts, %d parse, %d commands, %zu command kinds, longest %.1f s\n",
                    lvl, files, ok, cmds, names.size(), longest / 1000.0);
        ck(files > 0 && ok == files, (std::string(lvl) + ": every cinematic script parses").c_str(), firstErr);
        ck(names.count("MoveObject") && names.count("SetAnim") && names.count("SoundControl"),
           (std::string(lvl) + ": core command vocabulary present").c_str());
        int missing = 0; std::string firstMissing;
        for (auto& s : sounds) if (!s.empty() && !vox.find(s)) { ++missing; if (firstMissing.empty()) firstMissing = s; }
        ck(missing == 0, (std::string(lvl) + ": every sound a script names exists in VoxSounds").c_str(),
           std::to_string(sounds.size()) + " distinct, missing " + firstMissing);
    }

    // the documented sample: player thread transported then posed at 1100 ms
    Cinematic c;
    ck(c.load(root + "/levelnew_01/cinematics/levelnew_01_1003_cinematic.cff", e), "sample script loads", e);
    const CineThread* pt = c.thread(3);
    ck(pt && pt->name == "Player Thread" && pt->objectId == 288, "player thread identified");
    Vec3 p; float yaw;
    bool posed = c.playerPoseAt(1100, p, yaw);
    ck(posed && std::fabs(p.x + 7347.06f) < 0.1f && std::fabs(p.y - 57044.1f) < 0.1f,
       "MoveObject pose decodes at its stamp", std::to_string(p.x) + "," + std::to_string(p.y));
    ck(c.playerAnimAt(1100) == "idle_stand" && c.playerAnimAt(500).empty(), "SetAnim clip in effect by time");
    ck(c.durationMs == 1100, "duration is the last stamp", std::to_string(c.durationMs));
    // rot (0,0,-0.668,0.744): yaw about Z
    float expect = Cinematic::yawFromQuat(0, 0, -0.668130f, 0.744045f);
    ck(std::fabs(yaw - expect) < 1e-4f && std::fabs(expect) > 1.0f, "quaternion converts to a yaw");

    // a script with a camera thread
    Cinematic cam; bool anyCam = false; CineCamera cc;
    DIR* d = opendir((root + "/levelnew_01/cinematics").c_str());
    while (dirent* en = readdir(d)) {
        std::string n = en->d_name;
        if (n.size() < 5 || n.compare(n.size() - 4, 4, ".cff") != 0) continue;
        if (cam.load(root + "/levelnew_01/cinematics/" + n, e) && cam.cameraAt(cam.durationMs, cc)) { anyCam = true; break; }
    }
    closedir(d);
    ck(anyCam && cc.distance > 0 && (std::fabs(cc.dir.x) + std::fabs(cc.dir.y) + std::fabs(cc.dir.z)) > 0.5f,
       "ChangeCamera decodes target, direction and distance", std::to_string(cc.distance));

    // triggers resolve to scripts that parse
    LevelRoom L; int linked = 0, parsed = 0;
    if (L.loadFullLevel(root, "levelnew_01", e))
        for (auto& t : L.triggers) if (!t.cinematic.empty()) {
            ++linked; Cinematic x; if (x.load(root + "/levelnew_01/" + t.cinematic, e)) ++parsed;
        }
    ck(linked > 0 && parsed == linked, "every trigger-linked cinematic parses", std::to_string(parsed) + "/" + std::to_string(linked));

    // object threads and QTE branches link back to the level
    {
        std::set<int> enemyIds; for (auto& en : L.enemies) enemyIds.insert(en.nodeId);
        int objThreads = 0, enemyThreads = 0, dae = 0, qte = 0, qteResolved = 0;
        DIR* d2 = opendir((root + "/levelnew_01/cinematics").c_str());
        while (dirent* en = readdir(d2)) {
            std::string n = en->d_name;
            if (n.size() < 5 || n.compare(n.size() - 4, 4, ".cff") != 0) continue;
            Cinematic x; if (!x.load(root + "/levelnew_01/cinematics/" + n, e)) continue;
            for (auto& th : x.threads) if (th.type == 0) { ++objThreads; if (enemyIds.count(th.objectId)) ++enemyThreads; }
            dae += (int)x.daeAnims().size();
            for (auto& q : x.qtes()) { ++qte; if (L.cinematicById.count(q.successCinematic) && L.cinematicById.count(q.failCinematic)) ++qteResolved; }
        }
        closedir(d2);
        std::printf("  object threads %d (placed enemies %d), PlayDAEAnim %d, QTEs %d (%d fully resolve)\n",
                    objThreads, enemyThreads, dae, qte, qteResolved);
        ck(enemyThreads >= 40, "object threads name placed enemies by scene node id");
        ck(L.cinematicById.size() >= 70 && qteResolved >= qte - 1, "QTE branch ids resolve through the cinematic id map");
        ck(dae > 0, "PlayDAEAnim camera animations are exposed");
    }

    std::printf("\n%s (%d failures)\n", fails ? "CINEMATIC TEST FAILED" : "CINEMATIC TEST PASSED", fails);
    return fails ? 1 : 0;
}
