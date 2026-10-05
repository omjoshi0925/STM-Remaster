// Trigger volumes and runtime on a synthetic level, no assets.
#include "Script.hpp"
#include "unit_common.hpp"
using namespace bdae;
int main() {
    LevelRoom L;
    LevelRoom::TriggerVolume a; a.name = "Trigger_3thugs"; a.tag = "3thugs"; a.center = {0, 0, 0}; a.half = {100, 100, 200};
    LevelRoom::TriggerVolume b; b.name = "Trigger_if_BOSS_DIE"; b.tag = "if_boss_die"; b.center = {1000, 0, 0}; b.half = {50, 50, 50};
    b.id = 77; b.enterCinematic = 5; b.autoDisable = true;
    LevelRoom::TriggerVolume c; c.name = "Trigger_begin"; c.tag = "begin"; c.center = {2000, 0, 0}; c.half = {50, 50, 50};
    c.id = 78; c.enabled = false; c.whileInCinematic = 6; c.autoDisable = false;
    L.cinematicById[5] = "x.cff"; L.cinematicById[6] = "w.cff";
    L.triggers = {a, b, c};
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
    // authored model: Enabled, AutoDisabled, WhileIn, EnableTrigger by id
    ck(tr.enabledCount() == 2 && tr.update({2000, 0, 0}).empty(), "a trigger that starts disabled reports nothing");
    ck(tr.setEnabled(78, true) && !tr.setEnabled(99, false), "EnableTrigger by authored id");
    ev = tr.update({2000, 0, 0});
    int whileIn = 0; for (auto& x : ev) if (x.kind == ScriptEvent::WHILE_IN && x.cinematic == "w.cff") ++whileIn;
    ck(ev.size() == 2 && whileIn == 1, "an armed WhileIn trigger reports ENTER plus WHILE_IN with its script");
    ck(tr.update({2000, 0, 0}).size() == 1, "WHILE_IN repeats every update, ENTER does not");
    tr.update({5000, 0, 0});
    ck(tr.update({1000, 0, 0}).size() == 1, "an un-consumed AutoDisabled trigger re-fires on re-entry");
    tr.consume(1);
    tr.update({5000, 0, 0});
    ck(tr.update({1000, 0, 0}).empty() && tr.enabledCount() == 2, "consume switches an AutoDisabled trigger off");
    tr.consume(2);
    ck(tr.enabledCount() == 2, "consume leaves a non-AutoDisabled trigger armed");
    // boss phases: two thirds and one third, each once, none lost to a big hit
    BossStageTracker bs; bs.bind(20065, 20087);
    ck(bs.update(1.0f) == -1 && bs.update(0.7f) == -1, "no stage above two thirds");
    ck(bs.update(0.66f) == 20065 && bs.update(0.5f) == -1, "stage 2 fires once at two thirds");
    ck(bs.update(0.33f) == 20087 && bs.update(0.1f) == -1 && bs.done(), "stage 3 fires once at one third");
    BossStageTracker big; big.bind(20065, 20087);
    ck(big.update(0.1f) == 20065 && big.update(0.1f) == 20087 && big.update(0.0f) == -1, "a hit past both reports stage 2 then stage 3");
    BossStageTracker none; none.bind(-1, -1);
    ck(none.update(0.0f) == -1 && none.done(), "a boss without stage links never fires");
    UNIT_END("TRIGGER UNIT");
}
