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
#include <utility>
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
    // Every SoundControl with Play2D/Play3D in (t0, t1], as Vox event names.
    std::vector<std::string> soundsBetween(uint32_t t0, uint32_t t1) const;
    // Object threads whose AI is disabled at t (by thread name).
    std::vector<std::string> aiDisabledAt(uint32_t t) const;

    static float yawFromQuat(float x, float y, float z, float w);

    // Any thread by scene object id: pose from its MoveObject keyframes.
    bool objectPoseAt(int objectId, uint32_t t, Vec3& pos, float& yaw) const;
    // Objects whose SetVisible is false at t: object threads by their own id,
    // plus Basic-thread SetVisible commands that carry an ObjectID.
    std::vector<int> hiddenObjectsAt(uint32_t t) const;
    // PlayDAEAnim commands: which BDAE animation a thread plays from when.
    struct DaeAnim { int objectId; std::string file; int clip; uint32_t stampMs; };
    std::vector<DaeAnim> daeAnims() const;
    // StartQTE: the QTE id and the cinematic ids to branch to.
    struct Qte { uint32_t stampMs; int id; int successCinematic; int failCinematic; };
    std::vector<Qte> qtes() const;
    // The next StartQTE at or after t, if any.
    bool nextQteAfter(uint32_t t, Qte& out) const;
    // The PlayDAEAnim in effect on a thread (by object id) at t, if any.
    bool daeAnimAt(int objectId, uint32_t t, DaeAnim& out) const;

    // PlayDAECamera on the Basic thread: the authored camera track for the
    // whole script (camera_lv1_start.bdae), the script to chain into when this
    // one ends (^ID^Cinematic^Next, -1 for none), the far plane it asks for and
    // whether it marks the level end.
    struct CameraRequest { std::string file; uint32_t stampMs = 0; int nextCinematic = -1; float farPlane = 0; bool levelEnd = false; };
    bool cameraRequest(CameraRequest& out) const;

    // ShowMessage on the Basic thread: subtitle lines. stringId is a key in
    // the level's xlsStrings table (STR_PROLOGUE_SPIDERMAN_01), face the
    // speaker portrait id (1 Spider-Man), timerMs how long it stays up.
    struct Message { uint32_t stampMs = 0; uint32_t timerMs = 0; std::string stringId; int face = 0; };
    std::vector<Message> messages() const;
    // The message on screen at t, if any (latest started, still within its timer).
    bool messageAt(uint32_t t, Message& out) const;

    // ---- Milestone 22: control flow -------------------------------------
    // If* gates: a script only runs once every condition holds. IfObjectDestroyed
    // names a prop, IfEnemyDead an enemy spawn, IfHealthTo an enemy at or below
    // a health percentage. The caller evaluates them against its world.
    struct Condition { enum Kind { OBJECT_DESTROYED, ENEMY_DEAD, HEALTH_AT_MOST } kind; int id; float value; };
    std::vector<Condition> conditions() const;
    // StartCinematic in (t0, t1]: Cinematic node ids to hand over to.
    std::vector<int> startsBetween(uint32_t t0, uint32_t t1) const;
    // EnableTrigger / DisableTrigger in (t0, t1]: (trigger id, enabled).
    std::vector<std::pair<int, bool>> triggerTogglesBetween(uint32_t t0, uint32_t t1) const;
    // EnableCameraArea in (t0, t1]: (camera area id, enabled).
    std::vector<std::pair<int, bool>> cameraAreaTogglesBetween(uint32_t t0, uint32_t t1) const;

    // World effects in (t0, t1]:
    std::vector<int> savesBetween(uint32_t t0, uint32_t t1) const;        // Save: CheckPoint ids
    float damageBetween(uint32_t t0, uint32_t t1) const;                  // GetDamage on the player thread, summed
    std::vector<int> killsBetween(uint32_t t0, uint32_t t1) const;        // KillObject: the thread's object id
    std::vector<int> showHealthBetween(uint32_t t0, uint32_t t1) const;   // ShowHealth: enemy ids whose bar appears
    bool levelEndBetween(uint32_t t0, uint32_t t1) const;                 // LevelEnd or GameEnd
    std::vector<std::string> unlocksBetween(uint32_t t0, uint32_t t1) const;   // Unlock $SkillID ("1 sense")

    // Presentation:
    // InterfaceControl in effect at t (103 uses): whether the player keeps
    // control, the screen is black, SKIP is offered. Defaults when a script
    // never sets it: control off, not black, skip on (a cut scene).
    struct Interface { bool control = false; bool black = false; bool skip = true; bool arrow = false; bool attribution = false; bool set = false; };
    Interface interfaceAt(uint32_t t) const;
    // ShakeCamera in (t0, t1]: amplitude in units and length in frames.
    struct Shake { float maxOff; int frames; float xRate, yRate, zRate; };
    std::vector<Shake> shakesBetween(uint32_t t0, uint32_t t1) const;
    // Tutorial card in effect at t: string ids from Tutorial.map, blackScreen,
    // Timer (-1 = until dismissed), $TutorialButton (-1 = none).
    struct Tutorial { uint32_t stampMs = 0; std::string titleId, contentId; bool blackScreen = false; int timerMs = -1; int button = -1; };
    std::vector<Tutorial> tutorials() const;
    bool tutorialAt(uint32_t t, Tutorial& out) const;
    // SetSlowMotion in effect at t: time divisor (1 = normal speed).
    float slowMotionAt(uint32_t t) const;
    // Tutorial strings carry button glyph codes (^J jump, ^K attack, ^L web,
    // ^D stick, ^S sense, ^I interact, ^T web icon) and colour codes (^2, ^0).
    // Spell the buttons out and drop the colours for the outlined font.
    static std::string expandTutorialMarkup(const std::string& text);
};

// PlayDAEAnim names files the pack does not always ship under that exact
// name: the script asks for web_rope_ci_lv1_start.bdae and the pack holds
// web_rope_ci_0_lv1_start.bdae; car_plice_1107_lv1_Start.bdae sits beside
// car_plice_lv1_Start.bdae. Resolve exact (case-insensitive) first, then the
// same name with every "_<digits>_" token collapsed. Returns "" when nothing
// in `dir` matches, so a missing file (woman_lv1_start.bdae) is reported, not
// guessed.
std::string resolveAnimVariant(const std::string& dir, const std::string& requestedBasename);

// A camera path from a PlayDAEAnim animation file (camera_lv1_*.bdae):
// two animated nodes, Camera01 (eye) and Camera01_Target (look-at).
struct CameraTrack {
    Model model;
    int eyeNode = -1, targetNode = -1;
    uint32_t durationMs = 0;
    bool load(const std::string& bdaePath, std::string& err);
    // Eye and target at t (clamped to the clip); false if not loaded.
    bool sample(uint32_t t, Vec3& eye, Vec3& target);
};

} // namespace bdae
