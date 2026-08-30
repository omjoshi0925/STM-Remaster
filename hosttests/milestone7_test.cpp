// Milestone 7 headless verification: the original UI data layer.
#include "UIKitData.hpp"
#include <cstdio>
#include <dirent.h>
#include <string>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;

    BSprite ui;
    ck(ui.load(root + "/sprites/interface.bsprite", e), "interface.bsprite module table parses", e);
    std::printf("  interface: %zu atlas modules\n", ui.modules.size());
    ck(ui.modules.size() >= 32, "enough modules for a HUD");
    bool inb = true;
    for (auto& m : ui.modules)
        if (m.x + m.w > 512 || m.y + m.h > 512) inb = false;
    ck(inb, "every module rect sits inside the 512x512 atlas");

    BSprite menu;
    ck(menu.load(root + "/sprites/mainmenu.bsprite", e), "mainmenu.bsprite parses too", e);
    std::printf("  mainmenu: %zu modules\n", menu.modules.size());

    int screens = 0, parsed = 0, buttons = 0;
    std::string dir = root + "/configs";
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* en = readdir(d)) {
            std::string n = en->d_name;
            if (n.rfind("GS_", 0) != 0) continue;
            ++screens;
            GameScreen gs;
            if (gs.load(dir + "/" + n, e)) { ++parsed; buttons += gs.buttonCount; }
            else std::printf("  ! %s: %s\n", n.c_str(), e.c_str());
        }
        closedir(d);
    }
    std::printf("  game screens: %d/%d parse cleanly, %d buttons total\n", parsed, screens, buttons);
    ck(screens >= 10 && parsed == screens, "every GS_*.json screen parses after comment strip");

    std::printf("\n%s (%d failures)\n", fails ? "MILESTONE 7 DATA FAILED" : "MILESTONE 7 DATA PASSED", fails);
    return fails ? 1 : 0;
}
