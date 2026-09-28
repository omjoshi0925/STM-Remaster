#include "CineActor.hpp"

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

} // namespace bdae
