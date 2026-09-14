// Level scripting: the original Trigger / Cinematic / CameraArea authoring.
#include "Script.hpp"
#include <cstdio>
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
    LevelRoom L;
    if (!L.loadFullLevel(root, "levelnew_01", e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }

    std::printf("scripting: %zu trigger volumes, %zu camera volumes, %zu restore points\n",
                L.triggers.size(), L.cameraVolumes.size(), L.restorePoints.size());
    ck(L.triggers.size() >= 60, "Trigger and TriggerRestore volumes parse",
       std::to_string(L.triggers.size()));
    ck(L.cameraVolumes.size() == 44, "all 44 authored camera areas parse",
       std::to_string(L.cameraVolumes.size()));
    ck(L.restorePoints.size() == 20, "all 20 restore points parse",
       std::to_string(L.restorePoints.size()));

    int withCine = 0, withPts = 0;
    for (auto& t : L.triggers) if (!t.cinematic.empty()) ++withCine;
    for (auto& c : L.cameraVolumes) if (!c.controlPoints.empty()) ++withPts;
    std::printf("  %d triggers name a cinematic; %zu camera volumes carry control points\n",
                withCine, (size_t)withPts);
    ck(withCine >= 30, "cinematics link to their same-named triggers", std::to_string(withCine));
    ck(withPts == (int)L.cameraVolumes.size(), "every camera volume has control points");

    // the tags the level's own script vocabulary must contain
    std::set<std::string> tags;
    for (auto& t : L.triggers) tags.insert(t.tag);
    std::string missing;
    for (const char* want : {"lv1_start", "3thugs", "lv1_boss", "if_boss_die", "opendoor"})
        if (!tags.count(want)) missing += std::string(want) + " ";
    ck(missing.empty(), "the authored trigger vocabulary is recovered", missing);

    // camera volumes cover the played path
    int covered = 0, cps = 0;
    for (auto& mk : L.markers) if (mk.first == "CheckPoint") {
        ++cps;
        if (L.cameraVolumeAt(mk.second) >= 0) ++covered;
    }
    std::printf("  camera volumes cover %d of %d checkpoints\n", covered, cps);
    ck(L.cameraVolumeAt(L.spawn) >= 0, "the spawn sits inside an authored camera volume");
    ck(covered * 2 >= cps, "camera volumes cover most of the checkpoint path",
       std::to_string(covered) + "/" + std::to_string(cps));

    // runtime: each volume fires exactly once
    TriggerRuntime tr;
    tr.bind(L);
    ck(tr.firedCount() == 0, "nothing has fired before play");
    auto first = tr.update(L.triggers[0].center);
    ck(first.size() == 1 && first[0].tag == L.triggers[0].tag,
       "entering a volume reports it once");
    ck(tr.update(L.triggers[0].center).empty(), "staying inside does not re-fire");
    ck(tr.hasFired(L.triggers[0].tag), "the runtime remembers what has fired");

    int total = 0;
    for (auto& t : L.triggers) { auto ev = tr.update(t.center); total += (int)ev.size(); }
    std::printf("  walking every volume fired %d more\n", total);
    ck(tr.firedCount() >= (int)L.triggers.size() - 4,
       "walking the level fires essentially every volume",
       std::to_string(tr.firedCount()) + "/" + std::to_string(L.triggers.size()));

    ck(TriggerRuntime::isCompletionTag("if_boss_die") && !TriggerRuntime::isCompletionTag("3thugs"),
       "completion vocabulary is recognised");

    // Level 2 parses its own scripting
    LevelRoom L2;
    if (L2.loadFullLevel(root, "levelnew_02", e)) {
        std::printf("level 2: %zu triggers, %zu camera volumes\n",
                    L2.triggers.size(), L2.cameraVolumes.size());
        ck(!L2.triggers.empty() && !L2.cameraVolumes.empty(), "Level 2 scripting parses");
    } else ck(false, "Level 2 loads", e);

    // completion is driven by the authored boss trigger, not a guess
    TriggerRuntime tr2;
    tr2.bind(L);
    ck(!tr2.completionTriggered(), "a fresh level is not complete");
    ck(!tr2.levelComplete(true), "a downed boss alone does not complete the level");
    int bossTrig = -1;
    for (size_t i = 0; i < L.triggers.size(); ++i)
        if (TriggerRuntime::isCompletionTag(L.triggers[i].tag)) { bossTrig = (int)i; break; }
    ck(bossTrig >= 0, "Level 1 authors a completion trigger",
       bossTrig >= 0 ? L.triggers[bossTrig].name : "");
    if (bossTrig >= 0) {
        tr2.update(L.triggers[bossTrig].center);
        ck(tr2.completionTriggered(), "entering it arms completion");
        ck(!tr2.levelComplete(false) && tr2.levelComplete(true),
           "completion still requires the boss to be down");
    }

    // effect and hint markers the level authors
    int fx = 0, hints = 0;
    for (auto& mk : L.markers) { if (mk.first == "Effect") ++fx; else if (mk.first == "Hint") ++hints; }
    std::printf("  effect markers %d, hint markers %d\n", fx, hints);
    ck(fx >= 30, "Effect markers parse", std::to_string(fx));

    // every cinematic a trigger names must actually ship in the level pack
    int cineOk = 0, cineMissing = 0;
    std::string firstMissing;
    for (auto& t : L.triggers) {
        if (t.cinematic.empty()) continue;
        FILE* f = fopen((root + "/levelnew_01/" + t.cinematic).c_str(), "rb");
        if (f) { ++cineOk; fclose(f); }
        else { ++cineMissing; if (firstMissing.empty()) firstMissing = t.cinematic; }
    }
    std::printf("  cinematic scripts on disk: %d present, %d missing\n", cineOk, cineMissing);
    ck(cineMissing == 0, "every referenced .cff cinematic ships in the level pack", firstMissing);

    std::printf("\n%s (%d failures)\n", fails ? "SCRIPT TEST FAILED" : "SCRIPT TEST PASSED", fails);
    return fails ? 1 : 0;
}
