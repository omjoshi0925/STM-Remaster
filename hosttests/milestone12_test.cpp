// Milestone 12/13 headless verification: presentation-era systems.
#include "GameFlow.hpp"
#include "Combat.hpp"
#include <cstdio>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;
    LevelRoom l1;
    if (!l1.loadFullLevel(root, "levelnew_01", e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }

    // Comic trigger nodes come through as markers
    int comics = 0;
    for (auto& mk : l1.markers) if (mk.first == "Comic") ++comics;
    ck(comics == 18, "Level 1 carries its 18 original Comic trigger nodes", std::to_string(comics));

    // mid-level beat: reaching a node pops one page and returns to play
    GameFlow flow; uint32_t t = 1000;
    flow.beginLevel(l1, 0, t);
    ck(flow.comicNodes.size() == 18, "flow ingests the comic nodes");
    flow.startPlay(t += 500);
    int idx = flow.comicNodeReached(flow.comicNodes[0]);
    ck(idx == 0, "reaching a Comic node reports it once");
    ck(flow.comicNodeReached(flow.comicNodes[0]) == -1, "a seen node never re-triggers");
    flow.comicResumesPlay = true; flow.comicIndex = 0; flow.comicCount = 1;
    flow.phase = GameFlow::COMIC;
    flow.advanceComic(t += 500);
    ck(flow.phase == GameFlow::PLAYING, "mid-level beat resumes play, not the title");

    // checkpoint feedback flag
    flow.updatePlaying(flow.checkpointsAll[0], t += 500);
    ck(flow.takeCheckpointReached() && !flow.takeCheckpointReached(),
       "checkpoint reach reports exactly once");

    // combat: per-enemy damage field and corpse timing
    EnemyStats st;
    ck(st.damage == 5.0f, "EnemyStats carries per-enemy damage (default 5)");
    EnemyActor a; a.hp = 1; a.state = EnemyActor::IDLE;
    a.takeHit(5, 10000, 0, 0);
    ck(!a.alive() && a.corpseSink(12000) == 0.0f, "corpse rests untouched for 3 s");
    ck(a.corpseSink(13500) == 0.5f && a.corpseSink(14200) == 1.0f,
       "corpse sinks between 3 s and 4 s after death");

    std::printf("\n%s (%d failures)\n", fails ? "MILESTONE 12 SIM FAILED" : "MILESTONE 12 SIM PASSED", fails);
    return fails ? 1 : 0;
}
