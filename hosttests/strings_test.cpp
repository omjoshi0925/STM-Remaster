// String table: the offset-table .data decodes, every level has a name, and
// the per-level subtitle tables load and merge.
#include "GameFlow.hpp"
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
    StringTable t;
    ck(t.load(root + "/xlsStrings/MAIN.map", root + "/xlsStrings/MAIN_EN.data", e), "MAIN.map pairs with MAIN_EN.data", e);
    ck(t.byKey.size() >= 500, "the table is the full localisation set", std::to_string(t.byKey.size()));
    int named = 0;
    for (int i = 1; i <= 12; ++i)
        if (!t.get("STR_LEVELNEW_" + std::to_string(i) + "_NAME", "").empty()) ++named;
    ck(named == 12, "all 12 levels have a name string", std::to_string(named));
    ck(!t.get("STR_GAME_NAME", "").empty(), "STR_GAME_NAME present");
    int empty = 0;
    for (auto& kv : t.byKey) if (kv.second.empty()) ++empty;
    std::printf("  %d keys with empty EN values\n", empty);
    // the .data is a u32 offset table of UTF-16 strings, not line-paired text
    ck(t.decodedBinary == 591, "MAIN_EN.data decodes as an offset table of 591 strings", std::to_string(t.decodedBinary));
    ck(t.get("STR_GAME_NAME", "") == "Ultimate Spider-Man: Total Mayhem", "STR_GAME_NAME reads back verbatim", t.get("STR_GAME_NAME", ""));
    ck(t.get("STR_LEVELNEW_1_NAME", "") == "SAND IN YOUR FACE", "Level 1 chapter name", t.get("STR_LEVELNEW_1_NAME", ""));
    ck(t.get("STR_LEVELNEW_2_NAME", "") == "RHINO-SERIOUS RAMPAGE", "Level 2 chapter name", t.get("STR_LEVELNEW_2_NAME", ""));
    ck(empty <= 4, "almost every key has an English value once decoded properly", std::to_string(empty));
    // per-level subtitle tables use the same layout
    StringTable l1, l2;
    bool ok1 = l1.load(root + "/xlsStrings/levelnew_01.map", root + "/xlsStrings/levelnew_01_EN.data", e);
    ck(ok1 && l1.decodedBinary == 18, "levelnew_01 subtitle table: 18 lines", std::to_string(l1.decodedBinary));
    ck(l1.get("STR_PROLOGUE_CINEMATIC_GIRL_01", "") == "Freak!", "a prologue line reads back", l1.get("STR_PROLOGUE_CINEMATIC_GIRL_01", ""));
    bool ok2 = l2.load(root + "/xlsStrings/levelnew_02.map", root + "/xlsStrings/levelnew_02_EN.data", e);
    ck(ok2 && l2.decodedBinary == 10, "levelnew_02 subtitle table: 10 lines", std::to_string(l2.decodedBinary));
    size_t before = t.byKey.size(); t.merge(l1);
    ck(t.byKey.size() == before + l1.byKey.size() && t.get("STR_PROLOGUE_SPIDERMAN_01", "").rfind("My spider-sense", 0) == 0,
       "level strings merge on top of MAIN", std::to_string(t.byKey.size()));
    std::printf("\n%s (%d failures)\n", fails ? "STRINGS TEST FAILED" : "STRINGS TEST PASSED", fails);
    return fails ? 1 : 0;
}
