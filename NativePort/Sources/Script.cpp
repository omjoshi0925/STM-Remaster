#include "Script.hpp"

namespace bdae {

std::vector<ScriptEvent> TriggerRuntime::update(const Vec3& hero) {
    std::vector<ScriptEvent> out;
    if (!level) return out;
    for (size_t i = 0; i < level->triggers.size() && i < fired.size(); ++i) {
        if (fired[i]) continue;
        const LevelRoom::TriggerVolume& t = level->triggers[i];
        if (!t.contains(hero)) continue;
        fired[i] = 1;
        out.push_back({ (int)i, t.tag, t.cinematic });
    }
    return out;
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
