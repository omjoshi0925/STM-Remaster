// Milestone 10 headless verification: the texture binding, validated on data.
#include "Level.hpp"
#include <cstdio>
#include <dirent.h>
#include <map>
#include <set>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
// every textures_bin directory under the asset root (original app mounts all packs)
static std::map<std::string, std::string> textureIndex(const std::string& root) {
    std::map<std::string, std::string> idx;
    if (DIR* d = opendir(root.c_str())) {
        while (dirent* en = readdir(d)) {
            std::string sub = root + "/" + en->d_name + "/textures_bin";
            if (DIR* t = opendir(sub.c_str())) {
                while (dirent* f = readdir(t)) {
                    std::string n = f->d_name; std::string low = n;
                    for (char& c : low) c = (char)std::tolower((unsigned char)c);
                    if (low.size() > 4) idx.emplace(low, sub + "/" + n);
                }
                closedir(t);
            }
        }
        closedir(d);
    }
    return idx;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;
    auto idx = textureIndex(root);
    std::printf("texture index: %zu files across all mounted packs\n", idx.size());

    Model thug;
    thug.loadMesh(root + "/entities/meshes_bin/thug_bat_mesh.bdae", e);
    ck(!thug.meshes.empty() && thug.meshes[0].subMeshes.size() &&
       thug.meshes[0].subMeshes[0].diffuse == "thug.tga",
       "character binding: thug submesh -> thug.tga",
       thug.meshes.empty() ? e : thug.meshes[0].subMeshes[0].diffuse);

    LevelRoom l1;
    if (!l1.loadFullLevel(root, "levelnew_01", e)) { std::printf("FATAL %s\n", e.c_str()); return 1; }
    std::set<std::string> diff, lm; size_t textured = 0, untextured = 0, lmv = 0, tris = 0;
    int resolved = 0, missing = 0; std::map<std::string, int> missNames;
    for (auto& b : l1.visualBatches) {
        tris += b.indices.size() / 3;
        if (b.diffuse.empty()) { ++untextured; continue; }
        ++textured; diff.insert(b.diffuse);
        std::string low = b.diffuse; for (char& c : low) c = (char)std::tolower((unsigned char)c);
        if (idx.count(low)) ++resolved; else { ++missing; ++missNames[b.diffuse]; }
        if (!b.lightmap.empty()) { lm.insert(b.lightmap); lmv += b.vertices.size(); }
    }
    std::printf("LEVEL 1: %zu batches (%zu textured, %zu untextured), %zu tris, %zu distinct diffuse, %zu lightmaps, %zu lightmapped verts\n",
                l1.visualBatches.size(), textured, untextured, tris, diff.size(), lm.size(), lmv);
    std::printf("  diffuse files on disk: %d / %d batches; missing names: %zu\n", resolved, resolved + missing, missNames.size());
    ck(tris > 25000, "full level triangle count preserved through re-batching", std::to_string(tris));
    ck(diff.size() >= 30, "dozens of distinct original textures bound", std::to_string(diff.size()));
    ck(lm.size() >= 1 && lmv > 100, "lightmap layer detected on the second UV set", std::to_string(lmv) + " verts");
    ck(resolved * 100 >= (resolved + missing) * 50,
       "at least half the diffuse textures resolve within mounted packs (all packs mounted -> more)",
       std::to_string(resolved) + " resolved");
    bool uv2ok = false;
    for (auto& b : l1.visualBatches) if (!b.lightmap.empty())
        for (auto& v : b.vertices) if (v.u2 != v.u || v.v2 != v.v) { uv2ok = true; break; }
    ck(uv2ok, "lightmapped geometry carries a distinct second UV set");
    std::printf("\n%s (%d failures)\n", fails ? "MILESTONE 10 DATA FAILED" : "MILESTONE 10 DATA PASSED", fails);
    return fails ? 1 : 0;
}
