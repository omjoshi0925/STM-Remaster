// The authored script graph of Levels 1 and 2: trigger links, StartCinematic
// hand-overs, Enable/DisableTrigger targets, Save checkpoints, If* gate
// subjects and Tutorial strings all resolve against the level's own data.
#include "Cinematic.hpp"
#include "GameFlow.hpp"
#include "Script.hpp"
#include <cstdio>
#include <map>
#include <set>
#include <string>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;
    StringTable tut;
    bool tutOk = tut.load(root + "/xlsStrings/Tutorial.map", root + "/xlsStrings/Tutorial_EN.data", e);
    ck(tutOk && tut.decodedBinary == 17, "Tutorial string table decodes (17 prompts)", tutOk ? std::to_string(tut.decodedBinary) : e);
    int starts = 0, startsOk = 0, toggles = 0, togglesOk = 0, saves = 0, savesOk = 0;
    int gates = 0, gatesOk = 0, gatedScripts = 0, tutorials = 0, tutorialsOk = 0, camToggles = 0, camOk = 0;
    int zeroLength = 0, scripts = 0, armed = 0, armedStartDisabled = 0, gatesOnEnemies = 0;
    for (const char* lv : { "levelnew_01", "levelnew_02" }) {
        LevelRoom L;
        if (!L.loadFullLevel(root, lv, e)) { ck(false, "level loads", e); continue; }
        std::set<int> triggerIds, enemyIds, propIds;
        std::map<int, bool> triggerEnabled;
        for (auto& t : L.triggers) { triggerIds.insert(t.id); triggerEnabled[t.id] = t.enabled; }
        for (auto& en : L.enemies) enemyIds.insert(en.nodeId);
        for (auto& p : L.props) propIds.insert(p.nodeId);
        for (auto& kv : L.cinematicById) {
            Cinematic c;
            if (!c.load(root + "/" + lv + "/" + kv.second, e)) continue;
            ++scripts;
            if (c.durationMs == 0) ++zeroLength;
            for (int id : c.startsBetween(0, c.durationMs + 1)) { ++starts; if (L.cinematicById.count(id)) ++startsOk; else std::printf("    %s: StartCinematic %d unknown\n", kv.second.c_str(), id); }
            for (auto& tg : c.triggerTogglesBetween(0, c.durationMs + 1)) {
                ++toggles;
                if (triggerIds.count(tg.first)) ++togglesOk; else std::printf("    %s: trigger %d unknown\n", kv.second.c_str(), tg.first);
                if (tg.second) { ++armed; if (!triggerEnabled[tg.first]) ++armedStartDisabled; }
            }
            for (int id : c.savesBetween(0, c.durationMs + 1)) { ++saves; if (L.checkpointById.count(id)) ++savesOk; else std::printf("    %s: checkpoint %d unknown\n", kv.second.c_str(), id); }
            for (auto& ca : c.cameraAreaTogglesBetween(0, c.durationMs + 1)) { ++camToggles; if (L.cameraVolumeIndexById(ca.first) >= 0) ++camOk; }
            auto conds = c.conditions();
            if (!conds.empty()) ++gatedScripts;
            for (auto& cd : conds) {
                ++gates;
                // IfObjectDestroyed names enemies as often as props ("object" is any scene node)
                bool ok = propIds.count(cd.id) > 0 || enemyIds.count(cd.id) > 0;
                if (ok) ++gatesOk; else std::printf("    %s: gate on %d (%s) names no prop or enemy\n", kv.second.c_str(), cd.id,
                    cd.kind == Cinematic::Condition::OBJECT_DESTROYED ? "IfObjectDestroyed" : cd.kind == Cinematic::Condition::ENEMY_DEAD ? "IfEnemyDead" : "IfHealthTo");
                if (cd.kind == Cinematic::Condition::OBJECT_DESTROYED && enemyIds.count(cd.id)) ++gatesOnEnemies;
            }
            for (auto& tu : c.tutorials()) { ++tutorials; if (!tut.get(tu.contentId, "").empty()) ++tutorialsOk; else std::printf("    %s: tutorial %s has no text\n", kv.second.c_str(), tu.contentId.c_str()); }
        }
        if (std::string(lv) == "levelnew_01") {
            // the level start: the SpiderMan node's ^Link^Cinematic is 1265 (the prologue),
            // which chains into 1266 through PlayDAECamera; ^EndGame^Cinematic is 1267
            ck(L.startCinematic == 1265 && L.cinematicById.count(1265), "Level 1 starts with the prologue script named on the SpiderMan node", std::to_string(L.startCinematic));
            ck(L.endGameCinematic == 1267 && L.cinematicById.count(1267), "Level 1 ends with the epilogue script named on the SpiderMan node", std::to_string(L.endGameCinematic));
            Cinematic c2; Cinematic::CameraRequest cr;
            bool ok2 = L.cinematicById.count(1265) && c2.load(root + "/levelnew_01/" + L.cinematicById[1265], e) && c2.cameraRequest(cr);
            ck(ok2 && cr.nextCinematic == 1266 && L.cinematicById.count(1266), "the prologue chains into 1266 through PlayDAECamera");
            // Trigger_Lv1_Start (WhileIn 1264 -> StartCinematic 1265) is authored disabled: the node link is the real start
            int idx = -1; for (size_t i = 0; i < L.triggers.size(); ++i) if (L.triggers[i].name == "Trigger_Lv1_Start") idx = (int)i;
            ck(idx >= 0 && !L.triggers[(size_t)idx].enabled && L.triggers[(size_t)idx].whileInCinematic == 1264, "Trigger_Lv1_Start is a disabled WhileIn trigger, not the start");
        } else {
            ck(L.startCinematic == -1 && L.endGameCinematic == -1, "Level 2 names no start or epilogue script on its SpiderMan node");
        }
    }
    char b[200];
    snprintf(b, sizeof b, "%d scripts (%d zero-length), %d StartCinematic, %d trigger toggles (%d arm a trigger that starts disabled), %d saves, %d gates in %d scripts (%d on enemies), %d tutorials, %d camera toggles",
             scripts, zeroLength, starts, toggles, armedStartDisabled, saves, gates, gatedScripts, gatesOnEnemies, tutorials, camToggles);
    ck(scripts >= 130 && zeroLength >= 30, "many scripts are zero-length control beats", b);
    ck(starts >= 20 && startsOk == starts, "every StartCinematic names a Cinematic node the level knows");
    ck(toggles >= 30 && togglesOk == toggles, "every Enable/DisableTrigger names a trigger");
    ck(armed >= 8 && armedStartDisabled * 2 >= armed, "EnableTrigger mostly arms triggers that start disabled");
    ck(saves >= 8 && savesOk == saves, "every Save names a CheckPoint");
    ck(gates >= 40 && gatesOk == gates, "every If* gate names a prop or an enemy spawn");
    ck(gatesOnEnemies >= 20, "IfObjectDestroyed mostly gates on enemy spawns (kill-all beats)");
    ck(tutorials >= 15 && tutorialsOk >= tutorials - 2, "every Tutorial card has its text (Level 2 names STR_TUTORIAL_01 twice, which no table ships)");
    ck(camToggles >= 2 && camOk == camToggles, "every EnableCameraArea names a camera area");
    std::printf("\n%s (%d failures)\n", fails ? "SCRIPT FLOW FAILED" : "SCRIPT FLOW PASSED", fails);
    return fails ? 1 : 0;
}
