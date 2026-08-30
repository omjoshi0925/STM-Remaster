#include "GameFlow.hpp"
#include <cmath>
#include <fstream>

namespace bdae {

bool StringTable::load(const std::string& mapPath, const std::string& dataPath,
                       std::string& err) {
    std::ifstream km(mapPath), kv(dataPath);
    if (!km) { err = "cannot read " + mapPath; return false; }
    if (!kv) { err = "cannot read " + dataPath; return false; }
    std::string k, v;
    while (std::getline(km, k)) {
        if (!std::getline(kv, v)) v.clear();
        while (!k.empty() && (k.back() == '\r' || k.back() == '\n')) k.pop_back();
        while (!v.empty() && (v.back() == '\r' || v.back() == '\n')) v.pop_back();
        if (!k.empty()) byKey[k] = v;
    }
    if (byKey.empty()) { err = "no strings in " + mapPath; return false; }
    return true;
}

void GameFlow::beginLevel(const LevelRoom& lvl, int index, uint32_t nowMs) {
    levelIndex = index;
    phase = TITLE;
    phaseStartMs = nowMs;
    checkpoint = lvl.spawn;
    checkpointYaw = lvl.spawnYaw;
    checkpointsAll.clear();
    for (auto& mk : lvl.markers)
        if (mk.first == "CheckPoint") checkpointsAll.push_back(mk.second);
    visited.assign(checkpointsAll.size(), false);
}

bool GameFlow::updatePlaying(const Vec3& hero, uint32_t) {
    if (phase != PLAYING) return false;
    bool all = !checkpointsAll.empty();
    for (size_t i = 0; i < checkpointsAll.size(); ++i) {
        if (!visited[i]) {
            float dx = checkpointsAll[i].x - hero.x, dy = checkpointsAll[i].y - hero.y;
            float dz = checkpointsAll[i].z - hero.z;
            if (dx * dx + dy * dy < visitRadius * visitRadius && std::fabs(dz) < 400.0f) {
                visited[i] = true;
                checkpoint = checkpointsAll[i];
            }
        }
        if (!visited[i]) all = false;
    }
    if (all && phase == PLAYING) { phase = COMPLETE; return true; }
    return false;
}

int GameFlow::visitedCount() const {
    int n = 0;
    for (bool b : visited) if (b) ++n;
    return n;
}

} // namespace bdae
