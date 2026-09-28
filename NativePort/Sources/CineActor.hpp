// CineActor.hpp - Milestone 21: the characters and vehicles a cinematic animates.
//
// A .cff object thread names a scene node by id and plays a BDAE animation
// file on it (PlayDAEAnim). In the original scenes those nodes are
// AnimatedObject "CI_*" placements (CI_thug1, CI_Cop, CI_Car, fake_sandman,
// CI_Rhino ...) whose MeshFile is the character or vehicle mesh; in Level 2
// the end script animates the Boss_Rhino spawn itself. The animation files
// are authored in WORLD space: thug_bat01_lv1_start.bdae puts Bip01 at
// (14568, -10002) beside the Level 1 spawn, so an actor draws with an
// identity model matrix and the scene node's own transform is ignored.
#pragma once
#include "BDAEModel.hpp"
#include "Level.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace bdae {

// The mesh an enemy !GameType draws with (shared with the renderer's table).
const char* enemyMeshForType(const std::string& gameType);

// The mesh file (relative to the asset root) a cinematic object id draws with:
// the scene prop's MeshFile, or the archetype mesh of the enemy spawn with that
// id. Returns "" when the level has no node with that id.
std::string resolveActorMesh(const LevelRoom& room, int objectId);

// Script-clock to clip-time mapping shared by every actor: hold the first
// frame before the PlayDAEAnim stamp, play once, hold the last frame after.
uint32_t actorClipTime(uint32_t scriptMs, uint32_t startMs, uint32_t clipStartMs, uint32_t clipEndMs);

// One animated object: its mesh with the script's animation file bound.
struct CineActor {
    int objectId = -1;
    std::string meshFile, animFile;      // paths as resolved on disk
    uint32_t startMs = 0;                // PlayDAEAnim stamp on the script clock
    Model model;
    bool skinned = false;
    bool ok = false;

    // loadMesh(meshPath) then loadAnimation(animPath); requires one clip.
    bool load(const std::string& meshPath, const std::string& animPath, std::string& err);
    uint32_t durationMs() const;
    bool finished(uint32_t scriptMs) const { return scriptMs >= startMs + durationMs(); }
    // Pose the model for a script time (see actorClipTime).
    void poseAt(uint32_t scriptMs);
    // Feet position of a skinned actor / root node of a rigid one, after poseAt.
    Vec3 anchor();
    // Facing from root motion over the previous 400 ms; false when standing still.
    bool motionYaw(uint32_t scriptMs, float& yaw);

    // Rigid actors draw their meshes on animated nodes: one piece per
    // instance, or by matching mesh name to node name when the file has no
    // instance table (car_plice: 6 meshes, 2 nodes). Collision boxes ("bbox*"),
    // unnamed "_" duplicates and morph targets are skipped.
    struct Piece { int mesh = -1; Mat4 world; };
    std::vector<Piece> pieces() const;
};

} // namespace bdae
