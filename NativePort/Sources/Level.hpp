// Level.hpp - reads the original .irr room scenes shipped in levelnew_XX.pack
// and turns them into placed geometry + collision + navmesh + spawn points.
//
// The .irr files are Irrlicht scene XML stored as UTF-16LE.  Each <node> has an
// <attributes> block (Id / Position / Rotation / Scale / AbsoluteTransformation /
// Visible) and a <userData> block carrying the game-specific fields
// (Name, !GameType, MeshFile, #ParentID, ...).  Verified against every .irr in
// levelnew_01.pack.
#pragma once
#include "BDAEModel.hpp"
#include <map>
#include <string>
#include <vector>
#include <cstdio>

namespace bdae {

// Resolves a path against the filesystem ignoring case (original data mixes cases).
std::string resolveCaseInsensitive(const std::string& path);

struct IrrNode {
    int id = -1;
    int parentId = -1;
    std::string name;       // "Collisions_01"
    std::string gameType;   // "Geometry" | "Collisions" | "NavMesh" | "SpiderMan" | ...
    std::string meshFile;   // ".\meshes_bin\collision01.bdae" -> normalised below
    bool visible = true;
    Mat4 absolute;          // AbsoluteTransformation, already column-major
    Vec3 position;
    Quat rotation;
    Vec3 scale{1, 1, 1};
    std::map<std::string, std::string> attrs; // everything else, verbatim
};

struct IrrScene {
    std::vector<IrrNode> nodes;
    bool load(const std::string& path, std::string& err);
    const IrrNode* firstOfType(const std::string& gameType) const;
    std::vector<const IrrNode*> allOfType(const std::string& gameType) const;
};

// A triangle soup in world space, used for both rendering and collision.
struct TriMesh {
    std::vector<Vertex>   vertices;
    std::vector<uint16_t> indices;
    Vec3 bboxMin{ 1e30f, 1e30f, 1e30f };
    Vec3 bboxMax{ -1e30f, -1e30f, -1e30f };
    void appendTransformed(const Mesh& src, const Mat4& xform);
    // Appends one submesh (its own vertex subset, remapped) - used to build
    // texture-keyed visual batches.
    void appendSubMesh(const Mesh& src, const SubMesh& sm, const Mat4& xform);
    std::string diffuse, lightmap;   // texture key for visual batches
    int diffuseUv = 0;               // UV set the diffuse layer declares (1 = second set)
    bool empty() const { return indices.empty(); }
};

// Downward ray test used for ground snapping.  The game's world is Z-up.
struct GroundQuery {
    bool  hit = false;
    float z = 0;
    Vec3  normal{0, 0, 1};
};
GroundQuery groundBelow(const TriMesh& m, float x, float y, float fromZ);

struct LevelRoom {
    std::string name;
    // Visual geometry is split into batches so every batch stays inside the
    // uint16 index budget even with all 23 rooms loaded (~130k vertices total).
    std::vector<TriMesh> visualBatches;
    TriMesh collision;   // !GameType == "Collisions", merged across rooms
    TriMesh navmesh;     // !GameType == "NavMesh", merged across rooms
    size_t visualVertexCount() const;
    size_t visualTriangleCount() const;
    bool  hasSpawn = false;
    Vec3  spawn;
    float spawnYaw = 0;
    struct EnemySpawn { std::string type; Vec3 pos; float yaw = 0; };
    std::vector<EnemySpawn> enemies;

    // World props placed by the original scenes (lampposts, cars, hostages...).
    struct PropSpawn {
        std::string type;      // !GameType
        std::string meshFile;  // normalized relative path (entities/meshes_bin/x.bdae)
        std::string name;
        Mat4 transform;        // absolute world transform from the scene
    };
    std::vector<PropSpawn> props;
    std::vector<Vec3> bonuses;   // Bonus pickups carry no mesh, position only
    std::vector<std::pair<std::string, Vec3>> markers;   // checkpoints, waypoints, web points

    // assetRoot is the directory that contains "levelnew_01/...".
    // Several .irr files are merged: the level root scene carries the SpiderMan
    // spawn while the per-room scenes carry geometry, collision and navmesh.
    bool load(const std::string& assetRoot, const std::string& levelDir,
              const std::vector<std::string>& irrFiles, std::string& err);

    // Load the level root scene plus every room scene found in the level
    // directory (levelnew_01.irr, levelnew_01_0_Room1.irr, ...).
    bool loadFullLevel(const std::string& assetRoot, const std::string& levelDir,
                       std::string& err);

    // Walkable test used for movement: the navmesh defines where the player may
    // stand, so it doubles as the room's collision boundary.
    bool  canStandAt(float x, float y, float& outZ) const;
    // The level's SpiderMan node sits at the room entrance, which can be a few
    // units outside the navmesh edge.  Project it onto the nearest walkable spot.
    bool  nearestWalkable(float x, float y, float radius, Vec3& out) const;
};

} // namespace bdae
