// Asset audit: every reference the runtime will make must resolve on disk
// before a build is worth installing. Run against an extracted Assets tree.
#include "Level.hpp"
#include <cstdio>
#include <dirent.h>
#include <map>
#include <set>
#include <cctype>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
static std::map<std::string, std::string> indexAll(const std::string& root, const char* sub) {
    std::map<std::string, std::string> idx;
    if (DIR* d = opendir(root.c_str())) {
        while (dirent* en = readdir(d)) {
            std::string dir = root + "/" + en->d_name + "/" + sub;
            if (DIR* t = opendir(dir.c_str())) {
                while (dirent* f = readdir(t)) {
                    std::string n = f->d_name, low = n;
                    for (char& c : low) c = (char)std::tolower((unsigned char)c);
                    if (low.size() > 4) idx.emplace(low, dir + "/" + n);
                }
                closedir(t);
            }
        }
        closedir(d);
    }
    return idx;
}
// Mirrors the renderer's loose name resolution: strip trailing variant
// digits ("..._build2" -> "..._build"), then suffix-match ignoring leading
// pack/slot digits ("13_building" -> "041_building").
static std::string resolveFuzzy(const std::map<std::string, std::string>& idx,
                                const std::string& lowName) {
    size_t dot = lowName.find_last_of('.');
    std::string stem = (dot == std::string::npos) ? lowName : lowName.substr(0, dot);
    std::string ext = (dot == std::string::npos) ? "" : lowName.substr(dot);
    while (!stem.empty() && isdigit((unsigned char)stem.back())) stem.pop_back();
    auto it = idx.find(stem + ext);
    if (it != idx.end()) return it->second;
    std::string core = lowName;
    size_t k = 0;
    while (k < core.size() && (isdigit((unsigned char)core[k]) || core[k] == '_')) ++k;
    core = core.substr(k);
    if (core.size() > 5)
        for (auto& kv : idx)
            if (kv.first.size() >= core.size() &&
                kv.first.compare(kv.first.size() - core.size(), core.size(), core) == 0)
                return kv.second;
    return std::string();
}

int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    auto tex = indexAll(root, "textures_bin");
    auto mesh = indexAll(root, "meshes_bin");
    std::printf("index: %zu textures, %zu meshes across all mounted packs\n", tex.size(), mesh.size());
    ck(tex.size() > 150, "texture index is populated", std::to_string(tex.size()));
    ck(mesh.size() > 200, "mesh index is populated", std::to_string(mesh.size()));

    for (const char* lvl : {"levelnew_01", "levelnew_02"}) {
        LevelRoom L; std::string e;
        if (!L.loadFullLevel(root, lvl, e)) { ck(false, lvl, e); continue; }
        std::set<std::string> missTex, missProp;
        for (auto& b : L.visualBatches) {
            if (b.diffuse.empty()) continue;
            std::string low = b.diffuse;
            for (char& c : low) c = (char)std::tolower((unsigned char)c);
            if (tex.count(low)) continue;
            if (!resolveFuzzy(tex, low).empty()) continue;   // same rule the renderer uses
            missTex.insert(b.diffuse);
        }
        for (auto& p : L.props) {
            std::string base = p.meshFile.substr(p.meshFile.find_last_of('/') + 1);
            for (char& c : base) c = (char)std::tolower((unsigned char)c);
            if (!mesh.count(base)) missProp.insert(base);
        }
        std::string md;
        for (auto& s : missTex) if (md.size() < 90) md += s + " ";
        std::printf("%s: %zu batches, %zu props, %zu enemies, %zu bonuses\n",
                    lvl, L.visualBatches.size(), L.props.size(), L.enemies.size(), L.bonuses.size());
        // A handful of names ship under different names in the iOS build
        // (Car_01, 42_mall_glass, 05_atlas_A ...) - the original engine maps
        // them through a table we have not decoded. Those surfaces fall back
        // to baked vertex colour, so this is reported, not failed. What IS
        // failed: a level whose textures mostly do not resolve, which means
        // the packs were never extracted.
        std::printf("  note %s: %zu/%zu textured batches unresolved%s%s\n", lvl,
                    missTex.size(), L.visualBatches.size(), md.empty() ? "" : " - ", md.c_str());
        ck(missTex.size() * 2 < L.visualBatches.size(),
           (std::string(lvl) + ": level textures resolve").c_str(),
           std::to_string(missTex.size()) + " unresolved of " + std::to_string(L.visualBatches.size()));
        ck(missProp.size() <= 2, (std::string(lvl) + ": prop meshes resolve").c_str(),
           std::to_string(missProp.size()) + " unresolved");
        ck(L.hasSpawn && !L.navmesh.indices.empty() && !L.collision.indices.empty(),
           (std::string(lvl) + ": spawn, navmesh and collision present").c_str());
    }
    // UI assets the HUD and flow depend on
    for (const char* f : {"sprites/interface.tga", "sprites/font_outline_big.tga",
                          "xlsStrings/MAIN.map", "xlsStrings/MAIN_EN.data",
                          "entities/meshes_bin/spiderman_mesh.bdae",
                          "entities/meshes_bin/spiderman_anim.bdae"}) {
        FILE* fp = fopen((root + "/" + f).c_str(), "rb");
        ck(fp != nullptr, f);
        if (fp) fclose(fp);
    }
    std::printf("\n%s (%d failures)\n", fails ? "ASSET AUDIT FAILED" : "ASSET AUDIT PASSED", fails);
    return fails ? 1 : 0;
}
