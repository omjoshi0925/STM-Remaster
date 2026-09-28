#include "CineActor.hpp"
#include <cmath>

namespace bdae {

const char* enemyMeshForType(const std::string& t) {
    struct Row { const char* prefix; const char* mesh; };
    static const Row rows[] = {
        {"MeleeThugEnemy_bat",   "thug_bat_mesh.bdae"},
        {"MeleeThugEnemy_knife", "thug_knife_mesh.bdae"},
        {"RangeThug_molotov",    "thug_molotov_mesh.bdae"},
        {"MeleeThug_gun",        "thug_gun_mesh.bdae"},
        {"RangeThug_hammer",     "thug_hammer_mesh.bdae"},
        {"RangeThug_big",        "thug_big_mesh.bdae"},
        {"Boss_Sandman",         "sandman_mesh.bdae"},
        {"Boss_Rhino",           "rhino_mesh.bdae"},
    };
    for (const Row& r : rows) if (t.rfind(r.prefix, 0) == 0) return r.mesh;
    return nullptr;
}

std::string resolveActorMesh(const LevelRoom& room, int objectId) {
    if (const LevelRoom::PropSpawn* p = room.propById(objectId)) return p->meshFile;
    int ei = room.enemyIndexById(objectId);
    if (ei >= 0)
        if (const char* m = enemyMeshForType(room.enemies[(size_t)ei].type))
            return std::string("entities/meshes_bin/") + m;
    return std::string();
}

uint32_t actorClipTime(uint32_t scriptMs, uint32_t startMs, uint32_t clipStartMs, uint32_t clipEndMs) {
    if (clipEndMs <= clipStartMs) return clipStartMs;
    if (scriptMs <= startMs) return clipStartMs;
    uint32_t local = scriptMs - startMs, len = clipEndMs - clipStartMs;
    return clipStartMs + (local < len ? local : len - 1);
}


bool CineActor::load(const std::string& meshPath, const std::string& animPath, std::string& err) {
    ok = false;
    model = Model();
    std::string e;
    if (!model.loadMesh(meshPath, e)) { err = "mesh " + meshPath + ": " + e; return false; }
    e.clear();
    if (!model.loadAnimation(animPath, e)) { err = "animation " + animPath + ": " + e; return false; }
    if (model.clips.empty()) { err = "no clip in " + animPath; return false; }
    meshFile = meshPath; animFile = animPath;
    skinned = model.skin.valid && !model.meshes.empty();
    ok = true;
    poseAt(0);
    return true;
}

uint32_t CineActor::durationMs() const {
    if (!ok) return 0;
    const Clip& c = model.clips.front();
    return c.endMs > c.startMs ? c.endMs - c.startMs : 0;
}

void CineActor::poseAt(uint32_t scriptMs) {
    if (!ok) return;
    const Clip& c = model.clips.front();
    model.poseAtTime(actorClipTime(scriptMs, startMs, c.startMs, c.endMs));
}

Vec3 CineActor::anchor() {
    if (!ok) return Vec3{};
    if (skinned) {
        // feet: XY centroid + min Z of the skinned vertices at the current pose
        const std::vector<Mat4>& sk = model.skinningMatrices();
        const Mesh& m = model.meshes.front();
        double sx = 0, sy = 0; float minZ = 1e30f; size_t n = 0;
        for (size_t i = 0; i < m.vertices.size(); i += 4) {
            const Vertex& v = m.vertices[i];
            float x = 0, y = 0, z = 0;
            for (int k = 0; k < 4; ++k) {
                if (v.weight[k] <= 0 || v.bone[k] >= sk.size()) continue;
                const Mat4& M = sk[v.bone[k]];
                x += v.weight[k] * (M.m[0] * v.px + M.m[4] * v.py + M.m[8]  * v.pz + M.m[12]);
                y += v.weight[k] * (M.m[1] * v.px + M.m[5] * v.py + M.m[9]  * v.pz + M.m[13]);
                z += v.weight[k] * (M.m[2] * v.px + M.m[6] * v.py + M.m[10] * v.pz + M.m[14]);
            }
            sx += x; sy += y; if (z < minZ) minZ = z; ++n;
        }
        if (!n) return Vec3{};
        return Vec3{ (float)(sx / n), (float)(sy / n), minZ };
    }
    const std::vector<Mat4>& W = model.worldTransforms();
    for (const Channel& ch : model.channels)
        if (!ch.isRotation && ch.node >= 0 && ch.node < (int)W.size())
            return Vec3{ W[ch.node].m[12], W[ch.node].m[13], W[ch.node].m[14] };
    return W.empty() ? Vec3{} : Vec3{ W[0].m[12], W[0].m[13], W[0].m[14] };
}

bool CineActor::motionYaw(uint32_t scriptMs, float& yaw) {
    if (!ok) return false;
    uint32_t back = scriptMs > 400 ? scriptMs - 400 : 0;
    poseAt(back); Vec3 a = anchor();
    poseAt(scriptMs); Vec3 b = anchor();
    float dx = b.x - a.x, dy = b.y - a.y;
    if (dx * dx + dy * dy < 20.0f * 20.0f) return false;
    yaw = std::atan2(dy, dx);
    return true;
}

std::vector<CineActor::Piece> CineActor::pieces() const {
    std::vector<Piece> out;
    if (!ok) return out;
    const std::vector<Mat4>& W = model.worldTransforms();
    auto worldOf = [&](int node) { return (node >= 0 && node < (int)W.size()) ? W[(size_t)node] : Mat4::identity(); };
    if (!model.instances.empty()) {
        for (const Instance& in : model.instances)
            if (in.mesh >= 0 && in.mesh < (int)model.meshes.size()) out.push_back({ in.mesh, worldOf(in.node) });
        return out;
    }
    for (size_t mi = 0; mi < model.meshes.size(); ++mi) {
        const Mesh& m = model.meshes[mi];
        if (m.name.rfind("bbox", 0) == 0 || m.name == "_" || m.id.find("morpher") != std::string::npos) continue;
        int node = model.nodes.empty() ? -1 : 0;
        for (size_t ni = 0; ni < model.nodes.size(); ++ni)
            if (model.nodes[ni].name == m.name) { node = (int)ni; break; }
        out.push_back({ (int)mi, worldOf(node) });
    }
    return out;
}

} // namespace bdae
