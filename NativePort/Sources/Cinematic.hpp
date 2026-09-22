// Cinematic.hpp - Milestone 17: the .cff cinematic scripts.
//
// A .cff is UTF-16 XML: <cinematicThread type name object> blocks, each a
// list of <time stamp> entries holding <command name id> with typed
// attributes (int, float, bool, string, vector3d, quaternion). Thread types:
// 0 object, 1 basic, 2 camera, 3 player. This parses the script into a
// timeline and answers the questions playback needs at a time t.
#pragma once
#include "BDAEModel.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace bdae {

struct CineAttr {
    std::string type, name, value;   // as written in the file
    float f[4] = {0, 0, 0, 0};       // parsed numbers for float/vector3d/quaternion
    int   i = 0;                     // parsed int / bool
};

struct CineCommand {
    uint32_t stampMs = 0;
    std::string name;                // MoveObject, SetAnim, SoundControl ...
    int id = 0;
    std::vector<CineAttr> attrs;
    const CineAttr* attr(const std::string& n) const {
        for (const CineAttr& a : attrs) if (a.name == n) return &a;
        return nullptr;
    }
    bool vec3(const std::string& n, Vec3& out) const {
        const CineAttr* a = attr(n);
        if (!a) return false;
        out = Vec3{ a->f[0], a->f[1], a->f[2] };
        return true;
    }
    std::string str(const std::string& n) const { const CineAttr* a = attr(n); return a ? a->value : std::string(); }
    float num(const std::string& n, float def = 0) const { const CineAttr* a = attr(n); return a ? a->f[0] : def; }
    bool flag(const std::string& n) const { const CineAttr* a = attr(n); return a && a->i != 0; }
};

struct CineThread {
    int type = 0;                    // 0 object, 1 basic, 2 camera, 3 player
    std::string name;
    int objectId = 0;
    std::vector<CineCommand> commands;   // in file order, stamps ascending
};

struct CineCamera { Vec3 target{}; Vec3 dir{}; float distance = 800; };

struct Cinematic {
    std::vector<CineThread> threads;
    uint32_t durationMs = 0;

    bool load(const std::string& path, std::string& err);
    const CineThread* thread(int type) const;
    const CineThread* threadNamed(const std::string& name) const;

    // Player thread: pose from MoveObject keyframes (linear between keys).
    bool playerPoseAt(uint32_t t, Vec3& pos, float& yaw) const;
    // Player thread: the SetAnim clip in effect at t ("" if none yet).
    std::string playerAnimAt(uint32_t t) const;
    // Camera thread: the ChangeCamera in effect at t.
    bool cameraAt(uint32_t t, CineCamera& out) const;

    static float yawFromQuat(float x, float y, float z, float w);
};

} // namespace bdae
