#include "Script.hpp"

namespace bdae {

void TriggerRuntime::bind(const LevelRoom& lvl) {
    level = &lvl;
    fired.assign(lvl.triggers.size(), 0);
    inside.assign(lvl.triggers.size(), 0);
    enabled.assign(lvl.triggers.size(), 1);
    for (size_t i = 0; i < lvl.triggers.size(); ++i) enabled[i] = lvl.triggers[i].enabled ? 1 : 0;
}

std::vector<ScriptEvent> TriggerRuntime::update(const Vec3& hero) {
    std::vector<ScriptEvent> out;
    if (!level) return out;
    auto path = [&](int id) {
        auto it = level->cinematicById.find(id);
        return (id >= 0 && it != level->cinematicById.end()) ? it->second : std::string();
    };
    for (size_t i = 0; i < level->triggers.size() && i < inside.size(); ++i) {
        const LevelRoom::TriggerVolume& t = level->triggers[i];
        bool in = t.contains(hero), was = inside[i] != 0;
        inside[i] = in ? 1 : 0;
        if (!enabled[i]) continue;
        if (in && !was) {
            fired[i] = 1;
            out.push_back({ (int)i, ScriptEvent::ENTER, t.tag, t.enterCinematic, path(t.enterCinematic) });
        } else if (!in && was && t.exitCinematic >= 0) {
            out.push_back({ (int)i, ScriptEvent::EXIT, t.tag, t.exitCinematic, path(t.exitCinematic) });
        }
        if (in && t.whileInCinematic >= 0)
            out.push_back({ (int)i, ScriptEvent::WHILE_IN, t.tag, t.whileInCinematic, path(t.whileInCinematic) });
        if (!in && t.whileOutCinematic >= 0)
            out.push_back({ (int)i, ScriptEvent::WHILE_OUT, t.tag, t.whileOutCinematic, path(t.whileOutCinematic) });
    }
    return out;
}

void TriggerRuntime::consume(int index) {
    if (!level || index < 0 || (size_t)index >= level->triggers.size()) return;
    if (level->triggers[(size_t)index].autoDisable) enabled[(size_t)index] = 0;
}

int TriggerRuntime::indexOfId(int triggerId) const {
    if (!level || triggerId < 0) return -1;
    for (size_t i = 0; i < level->triggers.size(); ++i) if (level->triggers[i].id == triggerId) return (int)i;
    return -1;
}

bool TriggerRuntime::setEnabled(int triggerId, bool on) {
    int i = indexOfId(triggerId);
    if (i < 0) return false;
    enabled[(size_t)i] = on ? 1 : 0;
    if (on) inside[(size_t)i] = 0;   // arming re-arms the enter edge
    return true;
}

int TriggerRuntime::enabledCount() const {
    int n = 0;
    for (uint8_t e : enabled) if (e) ++n;
    return n;
}

bool TriggerRuntime::hasFired(const std::string& tag) const {
    if (!level) return false;
    for (size_t i = 0; i < level->triggers.size() && i < fired.size(); ++i)
        if (fired[i] && level->triggers[i].tag == tag) return true;
    return false;
}

int TriggerRuntime::firedCount() const {
    int n = 0;
    for (uint8_t f : fired) if (f) ++n;
    return n;
}

bool TriggerRuntime::completionTriggered() const {
    if (!level) return false;
    for (size_t i = 0; i < level->triggers.size() && i < fired.size(); ++i)
        if (fired[i] && isCompletionTag(level->triggers[i].tag)) return true;
    return false;
}

bool TriggerRuntime::isCompletionTag(const std::string& tag) {
    return tag == "if_boss_die" || tag == "ifboss_die" || tag == "boss_die" ||
           tag == "lv_end" || tag == "end" || tag == "win";
}

} // namespace bdae
