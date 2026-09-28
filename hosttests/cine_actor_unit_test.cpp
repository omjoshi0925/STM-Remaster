// Cinematic actor helpers with no assets: the script clock, the enemy mesh
// table, scene-id lookups on a synthetic level, and animation-file variant
// resolution against a temporary directory.
#include "CineActor.hpp"
#include "Cinematic.hpp"
#include "unit_common.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
using namespace bdae;
static void touch(const std::string& p) { if (FILE* f = std::fopen(p.c_str(), "wb")) std::fclose(f); }
int main() {
    // clock: hold first frame, play once, hold last frame
    ck(actorClipTime(0,     16200, 1000, 3000) == 1000, "before the stamp holds the first frame");
    ck(actorClipTime(16200, 16200, 1000, 3000) == 1000, "at the stamp starts the clip");
    ck(actorClipTime(16700, 16200, 1000, 3000) == 1500, "500 ms in is 500 ms into the clip");
    ck(actorClipTime(99999, 16200, 1000, 3000) == 2999, "past the end holds the last frame");
    ck(actorClipTime(500,   0,     700,  700)  == 700,  "empty clip is safe");

    // enemy meshes follow the !GameType prefixes the renderer uses
    ck(std::string(enemyMeshForType("MeleeThugEnemy_bat")) == "thug_bat_mesh.bdae", "bat thug mesh");
    ck(std::string(enemyMeshForType("Boss_Rhino")) == "rhino_mesh.bdae", "rhino mesh");
    ck(std::string(enemyMeshForType("RangeThug_molotov")) == "thug_molotov_mesh.bdae", "molotov thug mesh");
    ck(enemyMeshForType("Hostage") == nullptr, "non-enemy types have no archetype mesh");

    // scene-id lookups on a synthetic level
    LevelRoom room;
    room.props.push_back({ "AnimatedObject", "entities/meshes_bin/thug_bat_mesh.bdae", "CI_thug1", Mat4::identity(), 1258 });
    room.enemies.push_back({ "Boss_Rhino", Vec3{1, 2, 3}, 0.0f, 20055 });
    ck(room.propById(1258) && room.propById(1258)->name == "CI_thug1", "propById finds the CI_ actor");
    ck(room.propById(42) == nullptr, "propById misses an unknown id");
    ck(room.enemyIndexById(20055) == 0, "enemyIndexById finds the boss spawn");
    ck(room.enemyIndexById(-1) == -1 && room.enemyIndexById(7) == -1, "enemyIndexById misses");
    ck(resolveActorMesh(room, 1258) == "entities/meshes_bin/thug_bat_mesh.bdae", "prop actor uses the scene MeshFile");
    ck(resolveActorMesh(room, 20055) == "entities/meshes_bin/rhino_mesh.bdae", "enemy actor uses the archetype mesh");
    ck(resolveActorMesh(room, 5).empty(), "unknown id has no mesh");

    // variant resolution in a scratch directory
    char dir[] = "/tmp/stmactXXXXXX";
    if (!mkdtemp(dir)) { std::printf("mkdtemp failed\n"); return 1; }
    std::string d = dir;
    touch(d + "/web_rope_ci_0_lv1_start.bdae");
    touch(d + "/car_plice_lv1_Start.bdae");
    touch(d + "/car_plice_1107_lv1_Start.bdae");
    touch(d + "/thug_bat01_lv1_start.bdae");
    touch(d + "/thug_bat02_lv1_start.bdae");
    touch(d + "/cop01_1287_lv1_Start.bdae");
    touch(d + "/cop02_1287_lv1_Start.bdae");
    ck(resolveAnimVariant(d, "Car_Plice_LV1_START.bdae") == d + "/car_plice_lv1_Start.bdae", "exact name wins case-insensitively");
    ck(resolveAnimVariant(d, "web_rope_ci_lv1_start.bdae") == d + "/web_rope_ci_0_lv1_start.bdae", "_0_ token variant resolves");
    ck(resolveAnimVariant(d, "thug_bat01_lv1_start.bdae") == d + "/thug_bat01_lv1_start.bdae", "digits inside a token are not collapsed");
    ck(resolveAnimVariant(d, "cop_lv1_start.bdae").empty(), "two candidates stay unresolved");
    ck(resolveAnimVariant(d, "woman_lv1_start.bdae").empty(), "a file that does not ship is reported missing");
    ck(resolveAnimVariant(d + "/nope", "x.bdae").empty(), "missing directory is safe");
    UNIT_END("CINE ACTOR UNIT");
}
