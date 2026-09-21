// String table: keys and values pair by line, and every level has a name.
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
    std::printf("\n%s (%d failures)\n", fails ? "STRINGS TEST FAILED" : "STRINGS TEST PASSED", fails);
    return fails ? 1 : 0;
}
