// GameFlow.hpp - Milestone 8: level flow. TITLE -> PLAYING -> DEAD/respawn ->
// COMPLETE -> next level, with in-memory checkpoints from the original
// CheckPoint markers. Completion = every original checkpoint visited (the
// checkpoints trace the level path; the original trigger/cinematic-driven
// completion is not yet decoded - documented limitation).
#pragma once
#include "Level.hpp"
#include <map>
#include <string>
#include <vector>

namespace bdae {

// xlsStrings: MAIN.map = newline-separated keys, MAIN_<LANG>.data = values,
// line-paired by index.
struct StringTable {
    std::map<std::string, std::string> byKey;
    bool load(const std::string& mapPath, const std::string& dataPath, std::string& err);
    std::string get(const std::string& key, const std::string& fallback) const {
        auto it = byKey.find(key);
        return it == byKey.end() || it->second.empty() ? fallback : it->second;
    }
};

struct GameFlow {
    enum Phase { TITLE, PLAYING, DEAD, COMPLETE };
    Phase phase = TITLE;
    int levelIndex = 0;
    uint32_t phaseStartMs = 0;

    Vec3 checkpoint{};
    float checkpointYaw = 0;
    std::vector<Vec3> checkpointsAll;
    std::vector<bool> visited;
    float visitRadius = 260.0f;

    void beginLevel(const LevelRoom& lvl, int index, uint32_t nowMs);
    void startPlay(uint32_t nowMs) { phase = PLAYING; phaseStartMs = nowMs; }
    // Marks newly reached checkpoints, moves the respawn point; returns true
    // if this update completed the level.
    bool updatePlaying(const Vec3& hero, uint32_t nowMs);
    int visitedCount() const;
    void onDeath(uint32_t nowMs) { phase = DEAD; phaseStartMs = nowMs; }
    void respawn(uint32_t nowMs) { phase = PLAYING; phaseStartMs = nowMs; }
};

} // namespace bdae
