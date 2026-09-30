// Script.hpp - the level scripting runtime.
//
// The original levels are driven by Trigger volumes. Each carries an Enabled
// flag (about half start disabled and are armed by a script's EnableTrigger),
// AutoDisabled (fire once) and four Cinematic links: ^OutToIn^ (enter),
// ^InToOut^ (exit), ^WhileIn^ and ^WhileOut^ (every frame). This runtime
// tracks the hero against every volume and reports those edges; the caller
// starts the scripts, checks their If* gates, and tells the runtime when a
// trigger's script actually ran (consume) so AutoDisabled ones switch off.
#pragma once
#include "Level.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace bdae {

struct ScriptEvent {
    enum Kind { ENTER, EXIT, WHILE_IN, WHILE_OUT };
    int index = -1;            // index into LevelRoom::triggers
    Kind kind = ENTER;
    std::string tag;           // "3thugs", "lv1_boss", "opendoor" (from the node name)
    int cinematicId = -1;      // the Cinematic node this edge names, -1 when none
    std::string cinematic;     // its .cff path, empty when none
};

struct TriggerRuntime {
    const LevelRoom* level = nullptr;
    std::vector<uint8_t> fired;     // an ENTER has been reported at least once
    std::vector<uint8_t> enabled;   // authored Enabled, toggled by scripts
    std::vector<uint8_t> inside;    // the hero was inside on the previous update

    void bind(const LevelRoom& lvl);
    // Edges since the last call for enabled volumes: ENTER/EXIT once per
    // crossing, WHILE_IN/WHILE_OUT every call while a link exists.
    std::vector<ScriptEvent> update(const Vec3& hero);
    // The caller ran this trigger's script: AutoDisabled triggers switch off.
    void consume(int index);
    // Scripts' EnableTrigger/DisableTrigger, by authored node id.
    bool setEnabled(int triggerId, bool on);
    int indexOfId(int triggerId) const;
    int enabledCount() const;

    bool hasFired(const std::string& tag) const;
    int firedCount() const;

    // Tags the original uses to end a level (legacy recognition; the authored
    // end is PlayDAECamera "level end" / LevelEnd since Milestone 21-22).
    static bool isCompletionTag(const std::string& tag);
    bool completionTriggered() const;
    bool levelComplete(bool bossDown) const { return completionTriggered() && bossDown; }
};

} // namespace bdae
