// BDAEModel.hpp - Gameloft BDAE "0,0,0,324" loader: geometry, skeleton, skin, animation.
//
// Format facts below were verified byte-for-byte against the original
// Spider-Man: Total Mayhem iOS assets (see FORMAT_BDAE324.md).  Nothing here is
// guessed: every offset has a validation test in bdae_selftest.cpp.
//
// Pure C++17. No Metal, no Objective-C, no platform headers -> it compiles and
// runs on the host as well as on-device, which is how it gets tested.
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>

namespace bdae {

// ---------------------------------------------------------------- math (tiny)
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };

// Column-major 4x4, same memory order as simd_float4x4 / GLKMatrix4.
// m[col*4 + row]
struct Mat4 {
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    static Mat4 identity() { return Mat4(); }
    float&       operator()(int r, int c)       { return m[c * 4 + r]; }
    const float& operator()(int r, int c) const { return m[c * 4 + r]; }
};
Mat4 mul(const Mat4& a, const Mat4& b);
Vec3 transformPoint(const Mat4& m, const Vec3& p);
Mat4 trs(const Vec3& t, const Quat& q, const Vec3& s);
Quat slerp(const Quat& a, const Quat& b, float u);

// ------------------------------------------------------------------ resources
struct Node {
    std::string id;          // "Bip01_Pelvis-node"
    std::string name;        // "Bip01_Pelvis"
    std::string sid;         // "Bone19"  (empty for non-skinning nodes)
    Vec3 t;                  // local translation
    Quat q;                  // local rotation, stored (x,y,z,w)
    Vec3 s{1, 1, 1};         // local scale
    int  parent = -1;
    std::vector<int> children;
};

struct Vertex {
    float px = 0, py = 0, pz = 0;
    float nx = 0, ny = 0, nz = 1;
    float u = 0, v = 0;
    float u2 = 0, v2 = 0;                    // second UV set (lightmaps)
    uint8_t color[4]{255, 255, 255, 255};   // baked vertex lighting on level geometry
    uint8_t  bone[4]{0, 0, 0, 0};   // indices into Skin::joint
    float    weight[4]{0, 0, 0, 0};
};

struct SubMesh {
    uint32_t firstIndex = 0, indexCount = 0;
    std::string material;    // material name (submesh +4)
    // Resolved through material -> effect -> image-index arrays (effect +84/+88),
    // validated against the Android engine's CMaterial::prepareMaterial.
    std::string diffuse;     // image file name ("" = untextured / colour material)
    std::string lightmap;    // second layer whose name contains "lightmap", if any
    int diffuseUv = 0, lightmapUv = 1;
};

struct Mesh {
    std::string id;          // "Box01-mesh" -- instance URLs refer to this
    std::string name;
    std::vector<Vertex>   vertices;
    std::vector<uint16_t> indices;
    std::vector<SubMesh>  subMeshes;
    Vec3 bboxMin, bboxMax;
    uint32_t sourceStride = 0;
    bool hadNormals = false; // false -> normals were generated from faces
    bool hadUVs = false;
};

// <node><instance_geometry url="#Box01-mesh"/></node>
struct Instance { int node = -1; int mesh = -1; };

struct Skin {
    bool  valid = false;
    Mat4  bindShape;
    std::vector<std::string> jointSid;  // "Bone1".."BoneN"
    std::vector<int>         jointNode; // index into Model::nodes
    std::vector<Mat4>        inverseBind;
    uint32_t maxInfluences = 0;
};

// One animated scalar/vector/quaternion track for one node.
struct Channel {
    std::string targetNodeId;      // "Bip01_Pelvis-node"
    int         node = -1;         // resolved index into Model::nodes
    bool        isRotation = false;
    uint32_t    components = 0;    // 1, 3 or 4  (derived: floatCount / keyCount)
    std::vector<uint32_t> timeMs;
    std::vector<float>    values;  // keyCount * components
};

struct Clip { std::string name; uint32_t startMs = 0, endMs = 0; };

// ---------------------------------------------------------------------- model
class Model {
public:
    bool loadMesh(const std::string& path, std::string& err);
    // Merge animation channels + clips from a separate *_anim.bdae, binding the
    // channels onto this model's nodes by node id.
    bool loadAnimation(const std::string& path, std::string& err);

    // Pose the skeleton. timeMs is on the shared animation timeline.
    void poseAtTime(uint32_t timeMs);
    // Cross-fade between two points on the shared timeline (w: 0 = a, 1 = b).
    void poseBlend(uint32_t aMs, uint32_t bMs, float w);
    void poseBind();               // reset to the file's rest transforms

    // world[i] for every node after the last pose call.
    const std::vector<Mat4>& worldTransforms() const { return world_; }
    // skinning matrices: world(joint) * inverseBind(joint) * bindShape
    const std::vector<Mat4>& skinningMatrices() const { return skinMat_; }

    const Clip* findClip(const std::string& name) const;

    std::vector<Node>     nodes;
    std::vector<Mesh>     meshes;
    std::vector<Instance> instances;
    Skin                skin;
    std::vector<Channel> channels;
    std::vector<Clip>    clips;
    std::vector<std::string> textureNames;   // image *filename* field (character textures)
    std::vector<std::string> imageFiles;     // image *path* basename (level textures on disk)
    std::string version;

private:
    void applyPose(const std::vector<Vec3>& T, const std::vector<Quat>& Q);
    void rebuildWorld();
    void rebuildSkinning();
    std::vector<Vec3> restT_;
    std::vector<Quat> restQ_;
    std::vector<Vec3> restS_;
    std::vector<Mat4> world_;
    std::vector<Mat4> skinMat_;
};

// Skinned-mesh anchor for placing characters in the world: the XY centroid and
// minimum Z of the skinned vertices at the given timeline time.  Enemy clips are
// authored at arbitrary offsets in their source Max scenes (thug idle floats at
// Z~320), so world placement must subtract this.
Vec3 skinnedAnchor(Model& m, uint32_t timeMs);

// Clip playback helper: maps a local clip time onto the shared timeline.
struct ClipPlayer {
    const Clip* clip = nullptr;
    float localMs = 0;
    bool  loop = true;
    void  play(const Clip* c) { clip = c; localMs = 0; }
    void  advance(float dtSeconds);
    uint32_t timelineMs() const;
    bool  finished() const;
};

} // namespace bdae
