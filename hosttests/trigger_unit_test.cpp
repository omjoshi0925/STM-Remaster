// Trigger volumes and runtime on a synthetic level, no assets.
#include "Script.hpp"
#include "unit_common.hpp"
using namespace bdae;
int main() {
    LevelRoom L;
    LevelRoom::TriggerVolume a; a.name = "Trigger_3thugs"; a.tag = "3thugs"; a.center = {0, 0, 0}; a.half = {100, 100, 200};
    LevelRoom::TriggerVolume b; b.name = "Trigger_if_BOSS_DIE"; b.tag = "if_boss_die"; b.center = {1000, 0, 0}; b.half = {50, 50, 50}; b.cinematic = "x.cff";
    L.triggers = {a, b};
    ck(a.contains({50, -50, 100}) && !a.contains({150, 0, 0}) && !a.contains({0, 0, 250}), "box containment on every axis");
    TriggerRuntime tr; tr.bind(L);
    auto ev = tr.update({0, 0, 0});
    ck(ev.size() == 1 && ev[0].tag == "3thugs" && ev[0].cinematic.empty(), "entering reports tag and empty cinematic");
    ck(tr.update({0, 0, 0}).empty() && tr.update({120, 0, 0}).empty(), "no repeat while inside, none outside");
    ck(!tr.completionTriggered() && !tr.levelComplete(true), "no completion before the boss volume");
    ev = tr.update({1000, 0, 0});
    ck(ev.size() == 1 && ev[0].cinematic == "x.cff", "the boss volume carries its cinematic");
    ck(tr.completionTriggered() && tr.levelComplete(true) && !tr.levelComplete(false), "completion needs the boss down");
    ck(tr.firedCount() == 2 && tr.hasFired("3thugs") && !tr.hasFired("nope"), "counts and lookups");
    UNIT_END("TRIGGER UNIT");
}
