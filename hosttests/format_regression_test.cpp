// Format regression: locks the byte-level facts documented in
// FORMAT_BDAE324.md / FORMAT_TEXTURES.md so a loader change cannot silently
// break them.
#include "BDAEModel.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
static bool readAll(const std::string& p, std::vector<uint8_t>& out) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize((size_t)n);
    size_t got = fread(out.data(), 1, out.size(), f);
    fclose(f);
    return got == out.size();
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;

    // --- BDAE container invariants
    std::vector<uint8_t> b;
    ck(readAll(root + "/entities/meshes_bin/spiderman_mesh.bdae", b), "hero mesh readable");
    ck(b.size() > 32 && std::memcmp(b.data(), "BRES", 4) == 0, "BDAE magic is BRES");
    uint32_t headerSize, dataOff;
    std::memcpy(&headerSize, &b[8], 4); std::memcpy(&dataOff, &b[28], 4);
    ck(headerSize == 32, "header size is 32", std::to_string(headerSize));
    ck(dataOff > headerSize && dataOff < b.size(), "data region follows the string region");

    // --- model facts that the documentation states
    Model hero;
    ck(hero.loadMesh(root + "/entities/meshes_bin/spiderman_mesh.bdae", e), "hero mesh parses", e);
    size_t verts = 0, tris = 0;
    for (auto& m : hero.meshes) { verts += m.vertices.size(); tris += m.indices.size() / 3; }
    ck(verts == 641 && tris == 920, "hero is 641 verts / 920 triangles",
       std::to_string(verts) + "/" + std::to_string(tris));
    ck(hero.skin.valid && hero.skin.jointNode.size() == 38, "hero skin binds 38 joints",
       std::to_string(hero.skin.jointNode.size()));
    ck(hero.loadAnimation(root + "/entities/meshes_bin/spiderman_anim.bdae", e), "hero animation parses", e);
    ck(hero.clips.size() == 242, "242 animation clips", std::to_string(hero.clips.size()));

    // --- texture container invariants
    struct { const char* path; uint32_t fmt; uint32_t bpp; } tex[] = {
        {"/sprites/interface.tga", 0x10, 16},
        {"/sprites/font_outline_big.tga", 0x10, 16},
    };
    for (auto& t : tex) {
        std::vector<uint8_t> tb;
        if (!readAll(root + t.path, tb) || tb.size() < 40) { ck(false, t.path, "unreadable"); continue; }
        bool btex = std::memcmp(tb.data(), "BTEX", 4) == 0;
        uint32_t w, h, flags, bpp;
        std::memcpy(&w, &tb[12], 4); std::memcpy(&h, &tb[16], 4);
        std::memcpy(&flags, &tb[24], 4); std::memcpy(&bpp, &tb[32], 4);
        ck(btex && (flags & 0xff) == t.fmt && bpp == t.bpp && w == 512 && h == 512,
           t.path, "512x512 RGBA4444 expected");
    }
    // comic pages are plain TGA despite the extension
    std::vector<uint8_t> cb;
    if (readAll(root + "/comic1/comic_1.tga", cb) && cb.size() > 18) {
        ck(std::memcmp(cb.data(), "BTEX", 4) != 0 && (cb[2] == 2 || cb[2] == 10),
           "comic pages are plain Truevision TGA (type 2/10)",
           "type " + std::to_string(cb[2]));
    } else {
        std::printf("  [skip] comic1 not extracted\n");
    }
    std::printf("\n%s (%d failures)\n", fails ? "FORMAT REGRESSION FAILED" : "FORMAT REGRESSION PASSED", fails);
    return fails ? 1 : 0;
}
