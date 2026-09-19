#!/bin/bash
# Scaffold a host test suite. usage: Tools/new_test.sh <name>
set -e
n="${1:?usage: new_test.sh <name>}"
f="hosttests/${n}_test.cpp"
[ -e "$f" ] && { echo "$f exists"; exit 1; }
cat > "$f" <<CPP
#include "Level.hpp"
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
    LevelRoom L;
    if (!L.loadFullLevel(root, "levelnew_01", e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }
    ck(true, "replace me");
    std::printf("\n%s (%d failures)\n", fails ? "${n} FAILED" : "${n} PASSED", fails);
    return fails ? 1 : 0;
}
CPP
echo "created $f; add it to hosttests/run_host_tests.sh"
