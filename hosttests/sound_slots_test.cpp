// The original sound slot tables: every index lands on a VoxSounds row, the
// enemy matrix names the archetypes, and the hero slots resolve to variants.
#include "Audio.hpp"
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
    VoxTable vox;
    ck(vox.load(root + "/configs", e) && vox.order.size() == vox.byName.size(), "VoxSounds keeps its row order", e);

    BehaviorSoundMap bm;
    bool bl = bm.load(root + "/configs", vox, e);
    ck(bl && bm.slots.size() == 63, "BehaviorSoundMapList: 63 slots", e);
    int cells = 0, valid = 0;
    for (auto& r : bm.rows) for (uint32_t v : r) { if (v == 0xffffffffu) continue; ++cells; if (v < vox.order.size()) ++valid; }
    std::printf("  %d filled cells, %d valid row indices, columns:", cells, valid);
    for (auto& c : bm.columnArchetype) std::printf(" %s", c.empty() ? "-" : c.c_str());
    std::printf("\n");
    ck(cells > 100 && valid == cells, "every filled cell indexes a VoxSounds row");
    ck(bm.columnFor("THUG_KNIFE") == 0 && bm.columnFor("THUG_BAT") == 1 && bm.columnFor("THUG_GUN") == 3,
       "columns identify the thug archetypes in order");
    ck(bm.event("hurt1", "THUG_KNIFE", vox) == "SFX_THUG_KNIFE_HURT_1" &&
       bm.event("dies", "THUG_GUN", vox) == "SFX_THUG_GUN_DIES" &&
       bm.event("gun_shoot", "THUG_GUN", vox) == "SFX_THUG_GUN_SHOOT" &&
       bm.event("attack_swoosh", "THUG_BAT", vox) == "SFX_THUG_SWOOSH",
       "slot cells resolve to the original events");
    ck(bm.event("gun_shoot", "THUG_KNIFE", vox).empty(), "empty cells stay empty (knife thugs do not shoot)");
    ck(bm.event("dies", "THUG_HAMMER", vox) == "SFX_SLEDGER_DIES", "hammer thug maps to the SLEDGER column");
    EnemySounds es = vox.soundsFor("THUG_MOLOTOV", bm);
    ck(es.dies == "SFX_THUG_MOLOTOV_DIES" && es.voice == "SFX_THUG_VOICE_1", "soundsFor prefers the table");

    HeroSoundMap hm;
    bool hl = hm.load(root + "/configs", e);
    ck(hl && hm.variants.size() == 38, "MC_SOUND: 38 hero slots", std::to_string(hm.variants.size()));
    int hv = 0, hvalid = 0;
    for (auto& kv : hm.variants) for (uint32_t v : kv.second) { ++hv; if (v < vox.order.size()) ++hvalid; }
    ck(hv > 30 && hvalid == hv, "every hero variant indexes a VoxSounds row", std::to_string(hv));
    ck(hm.event("k_mc_sfx_swoosh_punch", vox, 0) == "SFX_PUNCH_SWOOSH_1" &&
       hm.event("k_mc_sfx_swoosh_punch", vox, 1) == "SFX_PUNCH_SWOOSH_2" &&
       hm.event("k_mc_sfx_swoosh_kick", vox, 0) == "SFX_KICK_SWOOSH_1" &&
       hm.event("k_mc_sfx_land", vox, 0) == "SFX_LAND" &&
       hm.event("k_mc_sfx_hurt", vox, 2) == "SFX_HURT_3",
       "hero slots resolve to their variants in order");
    std::printf("\n%s (%d failures)\n", fails ? "SOUND SLOTS FAILED" : "SOUND SLOTS PASSED", fails);
    return fails ? 1 : 0;
}
