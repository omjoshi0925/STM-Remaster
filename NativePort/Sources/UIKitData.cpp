#include "UIKitData.hpp"
#include <cstring>
#include <fstream>

namespace bdae {

static bool readAll(const std::string& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end); out.resize((size_t)f.tellg()); f.seekg(0);
    f.read((char*)out.data(), out.size());
    return true;
}

bool BSprite::load(const std::string& path, std::string& err) {
    std::vector<uint8_t> b;
    if (!readAll(path, b)) { err = "cannot read " + path; return false; }
    // longest run of plausible rects, 8-byte stride, any 2-byte phase
    size_t bestOff = 0, bestRun = 0;
    for (size_t phase = 0; phase < 8; phase += 2) {
        size_t off = phase, run = 0, s0 = 0;
        while (off + 8 <= b.size()) {
            uint16_t r[4]; std::memcpy(r, &b[off], 8);
            bool ok = r[2] >= 2 && r[3] >= 2 && r[2] <= atlasW && r[3] <= atlasH &&
                      r[0] + r[2] <= atlasW && r[1] + r[3] <= atlasH;
            if (ok) { if (!run) s0 = off; ++run; off += 8; }
            else {
                if (run > bestRun) { bestRun = run; bestOff = s0; }
                run = 0; off += 2;
            }
        }
        if (run > bestRun) { bestRun = run; bestOff = s0; }
    }
    if (bestRun < 8) { err = "no module table found in " + path; return false; }
    modules.resize(bestRun);
    for (size_t i = 0; i < bestRun; ++i)
        std::memcpy(&modules[i], &b[bestOff + i * 8], 8);
    return true;
}

bool GameScreen::load(const std::string& path, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot read " + path; return false; }
    std::string line, clean;
    while (std::getline(f, line)) {
        size_t c = line.find("//");
        clean += (c == std::string::npos ? line : line.substr(0, c));
        clean += '\n';
    }
    // structural sanity + widget counting without a full JSON dependency
    long depth = 0;
    for (char ch : clean) { if (ch == '{' || ch == '[') ++depth; if (ch == '}' || ch == ']') --depth; }
    if (depth != 0) { err = "unbalanced braces after comment strip"; return false; }
    auto count = [&](const char* key) {
        int n = 0; size_t p = 0;
        while ((p = clean.find(key, p)) != std::string::npos) { ++n; p += std::strlen(key); }
        return n;
    };
    buttonCount = count("\"buttonID\"") + count("\"ButtonID\"");
    spriteCount = count("\"spriteID\"") + count("\"SpriteID\"");
    textCount   = count("\"textID\"")   + count("\"TextID\"");
    size_t sl = path.find_last_of('/');
    name = (sl == std::string::npos ? path : path.substr(sl + 1));
    return true;
}

} // namespace bdae
