// Script.hpp - the level scripting runtime.
//
// The original levels are driven by named Trigger volumes ("Trigger_3thugs",
// "Trigger_Lv1_Boss"), most of which name a .cff cinematic through a
// same-named Cinematic node. This runtime reports the moment the hero first
// enters each volume; what a tag *means* is the caller's business.
#pragma once
#include "Level.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace bdae {

struct ScriptEvent {
    int index = -1;            // index into LevelRoom::triggers
    std::string tag;           // "3thugs", "lv1_boss", "opendoor"
    std::string cinematic;     // paired .cff path, empty when none
};

struct TriggerRuntime {
    const LevelRoom* level = nullptr;
    std::vector<uint8_t> fired;

    void bind(const LevelRoom& lvl) {
        level = &lvl;
        fired.assign(lvl.triggers.size(), 0);
    }
    // Newly entered volumes since the last call. Each fires once per level.
    std::vector<ScriptEvent> update(const Vec3& hero);
    bool hasFired(const std::string& tag) const;
    int firedCount() const;

    // Tags the original uses to end a level. Completion is still confirmed by
    // the caller (the boss must actually be down); this only recognises the
    // vocabulary.
    static bool isCompletionTag(const std::string& tag);

};

} // namespace bdae
