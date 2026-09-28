// Every PlayDAEAnim an object thread issues in Levels 1 and 2 resolves to a
// scene node, a mesh and a shipped animation file, loads, and animates in
// world space beside the level. Reports any file that does not ship.
#include "CineActor.hpp"
#include "Cinematic.hpp"
#include "Level.hpp"
#include <cmath>
#include <cstdio>
#include <dirent.h>
#include <string>
#include <vector>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
static std::string basenameOf(std::string f) {
    while (f.rfind("./", 0) == 0 || f.rfind("../", 0) == 0) f = f.substr(f.find('/') + 1);
    size_t s = f.find_last_of('/');
    return s == std::string::npos ? f : f.substr(s + 1);
}
struct Found { std::string cff, file; int objectId; bool skinned; float moved; float dur; Vec3 start; };
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    int requests = 0, resolvedNode = 0, loaded = 0, moving = 0, missing = 0, rigid = 0;
    std::vector<std::string> missingFiles;
    std::vector<Found> found;
    for (const char* lv : { "levelnew_01", "levelnew_02" }) {
        LevelRoom room; std::string e;
        if (!room.loadFullLevel(root, lv, e)) { ck(false, "level loads", e); continue; }
        std::string dir = root + "/" + lv + "/cinematics";
        DIR* dd = opendir(dir.c_str());
        if (!dd) { ck(false, "cinematics directory", dir); continue; }
        while (dirent* en = readdir(dd)) {
            std::string n = en->d_name;
            if (n.size() < 5 || n.compare(n.size() - 4, 4, ".cff") != 0) continue;
            Cinematic c;
            if (!c.load(dir + "/" + n, e)) continue;
            const CineThread* player = c.thread(3);
            for (const Cinematic::DaeAnim& da : c.daeAnims()) {
                if (player && da.objectId == player->objectId) continue;
                std::string base = basenameOf(da.file), low = base;
                for (char& ch : low) ch = (char)tolower(ch);
                if (low.find("camera") != std::string::npos && !room.propById(da.objectId)) continue;   // camera tracks
                ++requests;
                std::string mesh = resolveActorMesh(room, da.objectId);
                if (mesh.empty()) { std::printf("    %s object %d: no scene node\n", n.c_str(), da.objectId); continue; }
                ++resolvedNode;
                std::string anim = resolveAnimVariant(root + "/" + lv + "/meshes_bin", base);
                if (anim.empty()) { ++missing; missingFiles.push_back(std::string(lv) + "/" + base); continue; }
                CineActor a; a.objectId = da.objectId; a.startMs = da.stampMs;
                std::string ae;
                std::string meshPath = resolveCaseInsensitive(root + "/" + mesh);
                if (!a.load(meshPath, anim, ae)) { std::printf("    %s: %s\n", n.c_str(), ae.c_str()); continue; }
                ++loaded;
                if (!a.skinned) ++rigid;
                a.poseAt(a.startMs); Vec3 s = a.anchor();
                a.poseAt(a.startMs + a.durationMs()); Vec3 f = a.anchor();
                float moved = std::sqrt((f.x - s.x) * (f.x - s.x) + (f.y - s.y) * (f.y - s.y) + (f.z - s.z) * (f.z - s.z));
                if (moved > 50.0f) ++moving;
                found.push_back({ n, base, da.objectId, a.skinned, moved, a.durationMs() / 1000.0f, s });
                std::printf("    %-34s obj %-5d %-36s %s %5.1fs moved %6.0f  start (%.0f, %.0f, %.0f)\n", n.c_str(), da.objectId,
                            base.c_str(), a.skinned ? "skin " : "rigid", a.durationMs() / 1000.0, moved, s.x, s.y, s.z);
            }
        }
        closedir(dd);
        // world-space authoring: the intro thugs start beside the Level 1 spawn
        if (std::string(lv) == "levelnew_01") {
            bool near = false;
            for (const Found& f : found)
                if (f.file.rfind("thug_bat01", 0) == 0) {
                    float d = std::hypot(f.start.x - room.spawn.x, f.start.y - room.spawn.y);
                    near = d < 800.0f;
                    char b[96]; snprintf(b, sizeof b, "%.0f units from the spawn", d);
                    ck(near, "thug_bat01_lv1_start is authored in world space beside the spawn", b);
                }
        }
    }
    char b[160];
    snprintf(b, sizeof b, "%d requests, %d with a scene node, %d loaded (%d rigid), %d move, %d files missing",
             requests, resolvedNode, loaded, rigid, moving, missing);
    ck(requests >= 18, "object threads request PlayDAEAnim across both levels", b);
    ck(resolvedNode == requests, "every request names a scene node the level knows");
    ck(loaded == requests, "every request loads onto its mesh");
    ck(rigid >= 4, "rigid actors (cars, web rope, camera prop) bind by node");
    ck(moving >= 14, "most actors travel during their clip");
    for (const std::string& m : missingFiles) std::printf("    missing: %s\n", m.c_str());
    ck(missing == 0, "every requested animation file ships (variant spellings included)");
    auto has = [&](const char* f) { for (const Found& x : found) if (x.file.rfind(f, 0) == 0) return true; return false; };
    ck(has("thug_gun_lv1_start") && has("thug_bat01_lv1_start") && has("thug_bat02_lv1_start"), "the three intro thugs");
    ck(has("cop_lv1_start") && has("car_plice_lv1_start"), "the intro cop and the police car");
    ck(has("sandman_lv1_end") && has("rhino_lv1_end"), "the Level 1 end actors");
    ck(has("rhino_lv2_end") && has("cop01_582_lv2_end"), "the Level 2 end actors (rhino is the boss spawn itself)");
    std::printf("\n%s (%d failures)\n", fails ? "CINE ACTOR FAILED" : "CINE ACTOR PASSED", fails);
    return fails ? 1 : 0;
}
