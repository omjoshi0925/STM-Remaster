#pragma once
#include <cstdio>
#include <string>
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
#define UNIT_END(name) do { std::printf("\n%s (%d failures)\n", fails ? name " FAILED" : name " PASSED", fails); return fails ? 1 : 0; } while (0)
