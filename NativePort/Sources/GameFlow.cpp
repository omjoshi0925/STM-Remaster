#include "GameFlow.hpp"
#include <cmath>
#include <fstream>
#include <iterator>
#include <vector>

namespace bdae {

namespace {
uint32_t rd32(const std::vector<unsigned char>& b, size_t at) {
    return (uint32_t)b[at] | ((uint32_t)b[at + 1] << 8) | ((uint32_t)b[at + 2] << 16) | ((uint32_t)b[at + 3] << 24);
}
// UTF-16LE -> UTF-8 (the outlined UI font only draws ASCII; other code
// points are still carried so nothing is silently lost)
std::string utf16ToUtf8(const std::vector<unsigned char>& b, size_t from, size_t to) {
    std::string out;
    for (size_t p = from; p + 1 < to; p += 2) {
        uint32_t c = (uint32_t)b[p] | ((uint32_t)b[p + 1] << 8);
        if (c == 0) break;
        if (c < 0x80) out.push_back((char)c);
        else if (c < 0x800) { out.push_back((char)(0xC0 | (c >> 6))); out.push_back((char)(0x80 | (c & 0x3F))); }
        else { out.push_back((char)(0xE0 | (c >> 12))); out.push_back((char)(0x80 | ((c >> 6) & 0x3F))); out.push_back((char)(0x80 | (c & 0x3F))); }
    }
    return out;
}
// The shipped .data: u32 count, u32 offsets[count] (ascending, relative to the
// end of the table), then the UTF-16LE strings. Returns false when the bytes
// do not have that shape (a text file).
bool decodeOffsetTable(const std::vector<unsigned char>& b, std::vector<std::string>& out) {
    if (b.size() < 8) return false;
    uint32_t n = rd32(b, 0);
    if (n == 0 || n > 100000 || 4 + 4 * (size_t)n > b.size()) return false;
    size_t base = 4 + 4 * (size_t)n;
    std::vector<uint32_t> off((size_t)n);
    for (uint32_t i = 0; i < n; ++i) {
        off[i] = rd32(b, 4 + 4 * (size_t)i);
        if (base + off[i] > b.size() || (i && off[i] < off[i - 1])) return false;
    }
    if (off[0] != 0) return false;
    out.clear();
    for (uint32_t i = 0; i < n; ++i) {
        size_t s = base + off[i], e = (i + 1 < n) ? base + off[i + 1] : b.size();
        out.push_back(utf16ToUtf8(b, s, e));
    }
    return true;
}
} // namespace

bool StringTable::load(const std::string& mapPath, const std::string& dataPath,
                       std::string& err) {
    std::ifstream km(mapPath);
    if (!km) { err = "cannot read " + mapPath; return false; }
    std::ifstream kd(dataPath, std::ios::binary);
    if (!kd) { err = "cannot read " + dataPath; return false; }
    std::vector<unsigned char> raw((std::istreambuf_iterator<char>(kd)), std::istreambuf_iterator<char>());
    std::vector<std::string> keys;
    std::string k;
    while (std::getline(km, k)) {
        while (!k.empty() && (k.back() == '\r' || k.back() == '\n' || k.back() == ' ')) k.pop_back();
        keys.push_back(k);
    }
    std::vector<std::string> values;
    if (decodeOffsetTable(raw, values)) {
        decodedBinary = values.size();
    } else {   // plain text: one value per line, paired by index
        std::string v;
        for (size_t p = 0; p < raw.size();) {
            size_t e = p; while (e < raw.size() && raw[e] != '\n') ++e;
            v.assign(raw.begin() + (long)p, raw.begin() + (long)e);
            while (!v.empty() && v.back() == '\r') v.pop_back();
            values.push_back(v);
            p = e + 1;
        }
    }
    for (size_t i = 0; i < keys.size(); ++i)
        if (!keys[i].empty()) byKey[keys[i]] = i < values.size() ? values[i] : std::string();
    if (byKey.empty()) { err = "no strings in " + mapPath; return false; }
    return true;
}

void GameFlow::beginLevel(const LevelRoom& lvl, int index, uint32_t nowMs) {
    levelIndex = index;
    deaths = 0;
    phase = TITLE;            // boot videos (renderer) precede this on first launch
    phaseStartMs = nowMs;
    static const int kFirstPage[] = {1, 19, 41, 69, 99, 116, 131, 147, 162, 167, 175, 188};
    comicFirst = (index >= 0 && index < 12) ? kFirstPage[index] : 1;
    comicCount = 4;
    comicIndex = 0;
    comicPageStartMs = nowMs;
    checkpoint = lvl.spawn;
    checkpointYaw = lvl.spawnYaw;
    checkpointsAll.clear();
    comicNodes.clear();
    for (auto& mk : lvl.markers) {
        if (mk.first == "CheckPoint") checkpointsAll.push_back(mk.second);
        else if (mk.first == "Comic") comicNodes.push_back(mk.second);
    }
    visited.assign(checkpointsAll.size(), false);
    comicNodeSeen.assign(comicNodes.size(), false);
    comicPagesShown = 0;
    comicResumesPlay = false;
}

bool GameFlow::updatePlaying(const Vec3& hero, uint32_t) {
    if (phase != PLAYING) return false;
    bool all = !checkpointsAll.empty();
    for (size_t i = 0; i < checkpointsAll.size(); ++i) {
        if (!visited[i]) {
            float dx = checkpointsAll[i].x - hero.x, dy = checkpointsAll[i].y - hero.y;
            float dz = checkpointsAll[i].z - hero.z;
            if (dx * dx + dy * dy < visitRadius * visitRadius && std::fabs(dz) < 400.0f) {
                visited[i] = true;
                checkpoint = checkpointsAll[i];
                checkpointReached = true;
            }
        }
        if (!visited[i]) all = false;
    }
    if (all && phase == PLAYING && !authoredEnd) { phase = COMPLETE; return true; }
    return false;
}

bool GameFlow::advanceComic(uint32_t nowMs) {
    ++comicIndex;
    comicPageStartMs = nowMs;
    if (comicIndex >= comicCount) {
        if (comicResumesPlay) { comicResumesPlay = false; startPlay(nowMs); }
        else showTitle(nowMs);
        return true;
    }
    return false;
}

int GameFlow::comicNodeReached(const Vec3& hero) {
    for (size_t i = 0; i < comicNodes.size(); ++i) {
        if (comicNodeSeen[i]) continue;
        float dx = comicNodes[i].x - hero.x, dy = comicNodes[i].y - hero.y;
        if (dx * dx + dy * dy < 350.0f * 350.0f && std::fabs(comicNodes[i].z - hero.z) < 500.0f) {
            comicNodeSeen[i] = true;
            return (int)i;
        }
    }
    return -1;
}

int GameFlow::visitedCount() const {
    int n = 0;
    for (bool b : visited) if (b) ++n;
    return n;
}

} // namespace bdae
