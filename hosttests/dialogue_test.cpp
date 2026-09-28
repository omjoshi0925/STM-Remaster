// Cinematic subtitles: every ShowMessage in Levels 1 and 2 names a string the
// level's xlsStrings table (merged over MAIN) resolves to real text.
#include "Cinematic.hpp"
#include "GameFlow.hpp"
#include <cstdio>
#include <dirent.h>
#include <map>
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
    StringTable mainT;
    ck(mainT.load(root + "/xlsStrings/MAIN.map", root + "/xlsStrings/MAIN_EN.data", e), "MAIN strings load", e);
    int messages = 0, resolved = 0, timed = 0, scriptsWith = 0;
    std::map<int, int> faces;
    for (const char* lv : { "levelnew_01", "levelnew_02" }) {
        StringTable t = mainT, lt;
        std::string le;
        bool okL = lt.load(root + "/xlsStrings/" + lv + ".map", root + "/xlsStrings/" + lv + "_EN.data", le);
        ck(okL && lt.decodedBinary > 0, (std::string(lv) + " subtitle table loads").c_str(), okL ? std::to_string(lt.decodedBinary) + " lines" : le);
        t.merge(lt);
        std::string dir = root + "/" + lv + "/cinematics";
        DIR* d = opendir(dir.c_str());
        if (!d) { ck(false, "cinematics directory", dir); continue; }
        while (dirent* en = readdir(d)) {
            std::string n = en->d_name;
            if (n.size() < 5 || n.compare(n.size() - 4, 4, ".cff") != 0) continue;
            Cinematic c; if (!c.load(dir + "/" + n, e)) continue;
            std::vector<Cinematic::Message> ms = c.messages();
            if (!ms.empty()) ++scriptsWith;
            for (const Cinematic::Message& m : ms) {
                ++messages;
                std::string text = t.get(m.stringId, "");
                if (!text.empty()) ++resolved; else std::printf("    %s: %s has no text\n", n.c_str(), m.stringId.c_str());
                if (m.timerMs >= 1000) ++timed;
                ++faces[m.face];
                if (messages <= 6) std::printf("    %-34s %6u ms face %d %5.1fs  \"%s\"\n", n.c_str(), m.stampMs, m.face, m.timerMs / 1000.0, text.substr(0, 60).c_str());
            }
        }
        closedir(d);
    }
    std::string fd; for (auto& kv : faces) fd += "face " + std::to_string(kv.first) + " x" + std::to_string(kv.second) + "  ";
    ck(messages >= 20, "ShowMessage lines across both levels", std::to_string(messages) + " in " + std::to_string(scriptsWith) + " scripts");
    ck(resolved == messages, "every subtitle string id resolves to text", std::to_string(resolved) + "/" + std::to_string(messages));
    ck(timed == messages, "every line carries a display timer of at least a second");
    ck(faces.size() >= 2 && faces.count(1), "speaker faces include Spider-Man (1) and at least one other", fd);
    std::printf("\n%s (%d failures)\n", fails ? "DIALOGUE TEST FAILED" : "DIALOGUE TEST PASSED", fails);
    return fails ? 1 : 0;
}
