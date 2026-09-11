#include "Level.hpp"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <dirent.h>

namespace bdae {

namespace {

std::string utf16leToUtf8(const std::vector<uint8_t>& raw) {
    size_t i = 0;
    if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) i = 2;   // strip BOM
    std::string out;
    out.reserve(raw.size() / 2);
    for (; i + 1 < raw.size(); i += 2) {
        uint32_t c = static_cast<uint32_t>(raw[i]) | (static_cast<uint32_t>(raw[i + 1]) << 8);
        if (c >= 0xD800 && c < 0xDC00 && i + 3 < raw.size()) {       // surrogate pair
            uint32_t lo = static_cast<uint32_t>(raw[i + 2]) | (static_cast<uint32_t>(raw[i + 3]) << 8);
            if (lo >= 0xDC00 && lo < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            }
        }
        if (c < 0x80) out.push_back(static_cast<char>(c));
        else if (c < 0x800) { out.push_back(char(0xC0 | (c >> 6)));  out.push_back(char(0x80 | (c & 0x3F))); }
        else if (c < 0x10000) { out.push_back(char(0xE0 | (c >> 12))); out.push_back(char(0x80 | ((c >> 6) & 0x3F))); out.push_back(char(0x80 | (c & 0x3F))); }
        else { out.push_back(char(0xF0 | (c >> 18))); out.push_back(char(0x80 | ((c >> 12) & 0x3F))); out.push_back(char(0x80 | ((c >> 6) & 0x3F))); out.push_back(char(0x80 | (c & 0x3F))); }
    }
    return out;
}

std::vector<float> parseFloats(const std::string& s) {
    std::vector<float> v;
    const char* p = s.c_str();
    while (*p) {
        char* e = nullptr;
        float f = std::strtof(p, &e);
        if (e == p) { ++p; continue; }
        v.push_back(f);
        p = e;
        while (*p == ',' || *p == ' ') ++p;
    }
    return v;
}

// Normalise ".\meshes_bin\collision01.bdae" -> "meshes_bin/collision01.bdae"
std::string normPath(std::string s) {
    for (char& c : s) if (c == '\\') c = '/';
    if (s.rfind("./", 0) == 0) s = s.substr(2);
    return s;
}

std::string lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

} // anonymous helpers

// The .irr files reference "meshes_bin/nav01.bdae" while the pack actually
// contains "Nav01.bdae".  macOS hides this; a case-sensitive filesystem does
// not, so resolve the real spelling by scanning the directory once.
std::string resolveCaseInsensitive(const std::string& path) {
    if (FILE* f = std::fopen(path.c_str(), "rb")) { std::fclose(f); return path; }
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return path;
    std::string dir = path.substr(0, slash), want = lower(path.substr(slash + 1));
    DIR* d = opendir(dir.c_str());
    if (!d) return path;
    std::string found;
    while (struct dirent* e = readdir(d))
        if (lower(e->d_name) == want) { found = dir + "/" + e->d_name; break; }
    closedir(d);
    return found.empty() ? path : found;
}

// ============================================================== irr parsing ===
bool IrrScene::load(const std::string& path, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "cannot open " + path; return false; }
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string xml = utf16leToUtf8(raw);
    if (xml.find("<node") == std::string::npos) { err = "not an Irrlicht scene: " + path; return false; }

    size_t pos = 0;
    while (true) {
        size_t ns = xml.find("<node", pos);
        if (ns == std::string::npos) break;
        size_t ne = xml.find("<node", ns + 5);
        std::string blk = xml.substr(ns, (ne == std::string::npos ? xml.size() : ne) - ns);
        pos = (ne == std::string::npos) ? xml.size() : ne;

        IrrNode n;
        size_t p = 0;
        while (true) {
            size_t a = blk.find("name=\"", p);
            if (a == std::string::npos) break;
            a += 6;
            size_t b = blk.find('"', a);
            if (b == std::string::npos) break;
            std::string key = blk.substr(a, b - a);
            size_t vq = blk.find("value=\"", b);
            if (vq == std::string::npos) { p = b + 1; continue; }
            vq += 7;
            size_t ve = blk.find('"', vq);
            if (ve == std::string::npos) break;
            std::string val = blk.substr(vq, ve - vq);
            p = ve + 1;

            if      (key == "Id")                     n.id = std::atoi(val.c_str());
            else if (key == "#ParentID")              n.parentId = std::atoi(val.c_str());
            else if (key == "Name")                   n.name = val;
            else if (key == "!GameType")              n.gameType = val;
            else if (key == "MeshFile" || key == "#MeshFile") { if (n.meshFile.empty()) n.meshFile = normPath(val); }
            else if (key == "Visible")                n.visible = (val == "true" || val == "1");
            else if (key == "Position") { auto v = parseFloats(val); if (v.size() >= 3) n.position = { v[0], v[1], v[2] }; }
            else if (key == "Scale")    { auto v = parseFloats(val); if (v.size() >= 3) n.scale    = { v[0], v[1], v[2] }; }
            else if (key == "Rotation") { auto v = parseFloats(val); if (v.size() >= 4) n.rotation = { v[0], v[1], v[2], v[3] }; }
            else if (key == "AbsoluteTransformation") {
                auto v = parseFloats(val);
                if (v.size() >= 16) std::memcpy(n.absolute.m, v.data(), 64);
            }
            else n.attrs[key] = val;
        }
        if (n.id >= 0 || !n.name.empty()) nodes.push_back(std::move(n));
    }
    return !nodes.empty();
}

const IrrNode* IrrScene::firstOfType(const std::string& t) const {
    for (const auto& n : nodes) if (n.gameType == t) return &n;
    return nullptr;
}

std::vector<const IrrNode*> IrrScene::allOfType(const std::string& t) const {
    std::vector<const IrrNode*> r;
    for (const auto& n : nodes) if (n.gameType == t) r.push_back(&n);
    return r;
}

// ================================================================= trimesh ===
void TriMesh::appendTransformed(const Mesh& src, const Mat4& xform) {
    if (vertices.size() + src.vertices.size() > 60000) return;   // uint16 index budget
    uint16_t base = static_cast<uint16_t>(vertices.size());
    for (const Vertex& v : src.vertices) {
        Vertex o = v;
        Vec3 p = transformPoint(xform, Vec3{ v.px, v.py, v.pz });
        o.px = p.x; o.py = p.y; o.pz = p.z;
        // rotate the normal (no non-uniform-scale correction; level xforms are rigid)
        o.nx = xform.m[0]*v.nx + xform.m[4]*v.ny + xform.m[8]*v.nz;
        o.ny = xform.m[1]*v.nx + xform.m[5]*v.ny + xform.m[9]*v.nz;
        o.nz = xform.m[2]*v.nx + xform.m[6]*v.ny + xform.m[10]*v.nz;
        float l = std::sqrt(o.nx*o.nx + o.ny*o.ny + o.nz*o.nz);
        if (l > 1e-6f) { o.nx /= l; o.ny /= l; o.nz /= l; }
        vertices.push_back(o);
        bboxMin.x = std::min(bboxMin.x, p.x); bboxMin.y = std::min(bboxMin.y, p.y); bboxMin.z = std::min(bboxMin.z, p.z);
        bboxMax.x = std::max(bboxMax.x, p.x); bboxMax.y = std::max(bboxMax.y, p.y); bboxMax.z = std::max(bboxMax.z, p.z);
    }
    for (uint16_t i : src.indices) indices.push_back(static_cast<uint16_t>(base + i));
}

void TriMesh::appendSubMesh(const Mesh& src, const SubMesh& sm, const Mat4& xform) {
    std::vector<int32_t> remap(src.vertices.size(), -1);
    std::vector<uint16_t> local;
    local.reserve(sm.indexCount);
    for (uint32_t i = sm.firstIndex; i < sm.firstIndex + sm.indexCount && i < src.indices.size(); ++i) {
        uint16_t vi = src.indices[i];
        if (remap[vi] < 0) {
            if (vertices.size() >= 60000) return;   // uint16 index budget
            const Vertex& v = src.vertices[vi];
            Vertex o = v;
            Vec3 p = transformPoint(xform, Vec3{ v.px, v.py, v.pz });
            o.px = p.x; o.py = p.y; o.pz = p.z;
            o.nx = xform.m[0]*v.nx + xform.m[4]*v.ny + xform.m[8]*v.nz;
            o.ny = xform.m[1]*v.nx + xform.m[5]*v.ny + xform.m[9]*v.nz;
            o.nz = xform.m[2]*v.nx + xform.m[6]*v.ny + xform.m[10]*v.nz;
            float l = std::sqrt(o.nx*o.nx + o.ny*o.ny + o.nz*o.nz);
            if (l > 1e-6f) { o.nx /= l; o.ny /= l; o.nz /= l; }
            remap[vi] = (int32_t)vertices.size();
            vertices.push_back(o);
            bboxMin.x = std::min(bboxMin.x, p.x); bboxMin.y = std::min(bboxMin.y, p.y); bboxMin.z = std::min(bboxMin.z, p.z);
            bboxMax.x = std::max(bboxMax.x, p.x); bboxMax.y = std::max(bboxMax.y, p.y); bboxMax.z = std::max(bboxMax.z, p.z);
        }
        local.push_back((uint16_t)remap[vi]);
    }
    indices.insert(indices.end(), local.begin(), local.end());
}

GroundQuery groundBelow(const TriMesh& m, float x, float y, float fromZ) {
    GroundQuery g;
    float best = -1e30f;
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const Vertex& a = m.vertices[m.indices[i]];
        const Vertex& b = m.vertices[m.indices[i + 1]];
        const Vertex& c = m.vertices[m.indices[i + 2]];
        // 2D barycentric containment in the XY plane
        float d = (b.py - c.py) * (a.px - c.px) + (c.px - b.px) * (a.py - c.py);
        if (std::fabs(d) < 1e-9f) continue;
        float w0 = ((b.py - c.py) * (x - c.px) + (c.px - b.px) * (y - c.py)) / d;
        float w1 = ((c.py - a.py) * (x - c.px) + (a.px - c.px) * (y - c.py)) / d;
        float w2 = 1.0f - w0 - w1;
        const float eps = -1e-4f;
        if (w0 < eps || w1 < eps || w2 < eps) continue;
        float z = w0 * a.pz + w1 * b.pz + w2 * c.pz;
        if (z <= fromZ + 1.0f && z > best) {
            best = z;
            float ux = b.px - a.px, uy = b.py - a.py, uz = b.pz - a.pz;
            float vx = c.px - a.px, vy = c.py - a.py, vz = c.pz - a.pz;
            Vec3 n{ uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx };
            float l = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
            if (l > 1e-6f) { n.x /= l; n.y /= l; n.z /= l; }
            if (n.z < 0) { n.x = -n.x; n.y = -n.y; n.z = -n.z; }
            g.normal = n;
        }
    }
    if (best > -1e29f) { g.hit = true; g.z = best; }
    return g;
}

// ============================================================== level room ===
bool LevelRoom::load(const std::string& assetRoot, const std::string& levelDir,
                     const std::vector<std::string>& irrFiles, std::string& err) {
    IrrScene scene;
    for (const std::string& irrFile : irrFiles) {
        IrrScene one;
        std::string e;
        if (!one.load(resolveCaseInsensitive(assetRoot + "/" + levelDir + "/" + irrFile), e)) { err = e; continue; }
        for (IrrNode& n : one.nodes) scene.nodes.push_back(std::move(n));
        if (name.empty()) name = irrFile;
    }
    if (scene.nodes.empty()) return false;

    auto loadInto = [&](const IrrNode& n, TriMesh& dst) {
        if (n.meshFile.empty()) return;
        Model m;
        std::string e;
        std::string full = resolveCaseInsensitive(assetRoot + "/" + levelDir + "/" + n.meshFile);
        if (!m.loadMesh(full, e)) return;
        // Each sub-geometry is placed by the scene node that instances it; the
        // .irr node transform then places the whole room in the level.
        if (!m.instances.empty()) {
            const std::vector<Mat4>& W = m.worldTransforms();
            for (const Instance& inst : m.instances) {
                if (inst.mesh < 0 || inst.mesh >= (int)m.meshes.size()) continue;
                Mat4 x = (inst.node >= 0 && inst.node < (int)W.size())
                       ? mul(n.absolute, W[inst.node]) : n.absolute;
                dst.appendTransformed(m.meshes[inst.mesh], x);
            }
        } else {
            for (const Mesh& mesh : m.meshes) dst.appendTransformed(mesh, n.absolute);
        }
    };

    // Visual geometry is batched by (diffuse, lightmap) texture key so the
    // renderer binds each original texture once per batch.
    auto batchFor = [&](const std::string& diff, const std::string& lm, int duv) -> TriMesh& {
        for (auto it = visualBatches.rbegin(); it != visualBatches.rend(); ++it)
            if (it->diffuse == diff && it->lightmap == lm && it->diffuseUv == duv &&
                it->vertices.size() < 48000) return *it;
        visualBatches.emplace_back();
        visualBatches.back().diffuse = diff; visualBatches.back().lightmap = lm;
        visualBatches.back().diffuseUv = duv;
        return visualBatches.back();
    };
    auto loadVisual = [&](const IrrNode& n) {
        std::string full = resolveCaseInsensitive(assetRoot + "/" + levelDir + "/" + n.meshFile);
        Model m; std::string e;
        if (!m.loadMesh(full, e)) return;
        auto appendMesh = [&](const Mesh& mesh, const Mat4& x) {
            if (mesh.subMeshes.empty()) { batchFor("", "", 0).appendTransformed(mesh, x); return; }
            for (const SubMesh& sm : mesh.subMeshes)
                batchFor(sm.diffuse, sm.lightmap, sm.diffuseUv).appendSubMesh(mesh, sm, x);
        };
        if (!m.instances.empty()) {
            const std::vector<Mat4>& W = m.worldTransforms();
            for (const Instance& inst : m.instances) {
                if (inst.mesh < 0 || inst.mesh >= (int)m.meshes.size()) continue;
                Mat4 x = (inst.node >= 0 && inst.node < (int)W.size())
                       ? mul(n.absolute, W[inst.node]) : n.absolute;
                appendMesh(m.meshes[inst.mesh], x);
            }
        } else {
            for (const Mesh& mesh : m.meshes) appendMesh(mesh, n.absolute);
        }
    };
    for (const IrrNode& n : scene.nodes) {
        if      (n.gameType == "Geometry")   loadVisual(n);
        else if (n.gameType == "Collisions") loadInto(n, collision);
        else if (n.gameType == "NavMesh")    loadInto(n, navmesh);
        else if (n.gameType == "DestroyableObject" || n.gameType == "StaticObject" ||
                 n.gameType == "Car" || n.gameType == "AnimatedObject" ||
                 n.gameType == "Hostage") {
            std::string mf = n.meshFile;
            for (char& ch : mf) if (ch == '\\') ch = '/';
            size_t sp = mf.find("entities/");
            if (sp != std::string::npos) mf = mf.substr(sp);
            else {
                while (mf.rfind("../", 0) == 0 || mf.rfind("./", 0) == 0)
                    mf = mf.substr(mf.find('/') + 1);
                if (!mf.empty()) mf = levelDir + "/" + mf;   // level-local props (billboards etc.)
            }
            if (!mf.empty()) props.push_back({ n.gameType, mf, n.name, n.absolute });
        }
        else if (n.gameType == "Bonus")
            bonuses.push_back(Vec3{ n.absolute.m[12], n.absolute.m[13], n.absolute.m[14] });
        else if (n.gameType == "SpiderMan" || n.gameType == "SpawnPoint") {
            if (!hasSpawn) {
                hasSpawn = true;
                spawn = { n.absolute.m[12], n.absolute.m[13], n.absolute.m[14] };
                spawnYaw = std::atan2(2.0f * (n.rotation.w * n.rotation.z + n.rotation.x * n.rotation.y),
                                      1.0f - 2.0f * (n.rotation.y * n.rotation.y + n.rotation.z * n.rotation.z));
            }
        }
        else if (n.gameType.rfind("MeleeThug", 0) == 0 || n.gameType.rfind("RangeThug", 0) == 0 ||
                 n.gameType.rfind("Boss_", 0) == 0 || n.gameType.rfind("Symbiote", 0) == 0 ||
                 n.gameType.rfind("Robot_", 0) == 0 || n.gameType == "FlyingGoblin")
        {
            float yw = std::atan2(2.0f * (n.rotation.w * n.rotation.z + n.rotation.x * n.rotation.y),
                                  1.0f - 2.0f * (n.rotation.y * n.rotation.y + n.rotation.z * n.rotation.z));
            enemies.push_back({ n.gameType, Vec3{ n.absolute.m[12], n.absolute.m[13], n.absolute.m[14] }, yw });
        }
        else if (n.gameType == "CheckPoint" || n.gameType == "WayPoint" ||
                 n.gameType == "WebGrabPoint" || n.gameType == "Comic")
            markers.push_back({ n.gameType, Vec3{ n.absolute.m[12], n.absolute.m[13], n.absolute.m[14] } });
    }
    if (visualBatches.empty() && collision.empty()) { err = "level " + name + " produced no geometry"; return false; }
    return true;
}

size_t LevelRoom::visualVertexCount() const {
    size_t n = 0;
    for (const TriMesh& b : visualBatches) n += b.vertices.size();
    return n;
}
size_t LevelRoom::visualTriangleCount() const {
    size_t n = 0;
    for (const TriMesh& b : visualBatches) n += b.indices.size() / 3;
    return n;
}

bool LevelRoom::loadFullLevel(const std::string& assetRoot, const std::string& levelDir,
                              std::string& err) {
    std::vector<std::string> irrs;
    std::string dir = assetRoot + "/" + levelDir;
    DIR* d = opendir(dir.c_str());
    if (!d) { err = "cannot open " + dir; return false; }
    while (struct dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.size() > 4 && n.rfind(levelDir, 0) == 0 &&
            lower(n.substr(n.size() - 4)) == ".irr")
            irrs.push_back(n);
    }
    closedir(d);
    std::sort(irrs.begin(), irrs.end(), [&](const std::string& a, const std::string& b) {
        // the level root scene (shortest name) first, so the spawn wins
        if (a.size() != b.size() && (a == levelDir + ".irr" || b == levelDir + ".irr"))
            return a == levelDir + ".irr";
        return a < b;
    });
    if (irrs.empty()) { err = "no .irr scenes in " + dir; return false; }
    return load(assetRoot, levelDir, irrs, err);
}

bool LevelRoom::canStandAt(float x, float y, float& outZ) const {
    GroundQuery g = groundBelow(navmesh, x, y, 1e9f);
    if (!g.hit) {
        g = groundBelow(collision, x, y, 1e9f);       // navmesh gaps fall back to collision
        if (!g.hit || g.normal.z < 0.5f) return false;
    }
    outZ = g.z;
    return true;
}

bool LevelRoom::nearestWalkable(float x, float y, float radius, Vec3& out) const {
    float z;
    if (canStandAt(x, y, z)) { out = { x, y, z }; return true; }
    for (float r = radius * 0.05f; r <= radius; r += radius * 0.05f) {
        for (int a = 0; a < 24; ++a) {
            float th = a * (6.28318531f / 24.0f);
            float px = x + std::cos(th) * r, py = y + std::sin(th) * r;
            if (canStandAt(px, py, z)) { out = { px, py, z }; return true; }
        }
    }
    return false;
}

} // namespace bdae
