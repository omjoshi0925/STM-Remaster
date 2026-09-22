// Flow phases and checkpoint bookkeeping on a synthetic level, no assets.
#include "GameFlow.hpp"
#include "unit_common.hpp"
using namespace bdae;
int main() {
    LevelRoom L; L.hasSpawn = true; L.spawn = {0, 0, 0};
    L.markers = {{"CheckPoint", {100, 0, 0}}, {"CheckPoint", {1000, 0, 0}}, {"Comic", {500, 0, 0}}};
    GameFlow f; f.beginLevel(L, 0, 1000);
    ck(f.phase == GameFlow::TITLE && f.checkpointsAll.size() == 2 && f.comicNodes.size() == 1, "level opens on the title with its markers");
    f.startPlay(1500);
    ck(!f.updatePlaying({100, 0, 0}, 2000) && f.visitedCount() == 1 && f.takeCheckpointReached(), "first checkpoint visited and reported once");
    ck(!f.takeCheckpointReached(), "the report clears");
    ck(f.checkpoint.x == 100, "respawn point moved to the checkpoint");
    ck(f.comicNodeReached({500, 0, 0}) == 0 && f.comicNodeReached({500, 0, 0}) == -1, "comic node fires once");
    f.onDeath(3000); ck(f.phase == GameFlow::DEAD, "death phase");
    f.respawn(3500); ck(f.phase == GameFlow::PLAYING, "respawn resumes play");
    ck(f.updatePlaying({1000, 0, 0}, 4000) && f.phase == GameFlow::COMPLETE, "last checkpoint completes the level");
    UNIT_END("GAMEFLOW UNIT");
}
