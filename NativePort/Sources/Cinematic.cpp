#include "Cinematic.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>

namespace bdae {
namespace {

bool readUtf16(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> b((size_t)std::max(0L, n));
    size_t got = b.empty() ? 0 : std::fread(b.data(), 1, b.size(), f);
    std::fclose(f);
    if (got != b.size()) return false;
    size_t p = (b.size() >= 2 && b[0] == 0xff && b[1] == 0xfe) ? 2 : 0;
    out.clear(); out.reserve(b.size() / 2);
    for (; p + 1 < b.size(); p += 2) {
        unsigned c = b[p] | (b[p + 1] << 8);
        out.push_back(c < 128 ? (char)c : '?');
    }
    return true;
}

// value of attribute `key` inside a tag starting at `tag` (up to '>')
std::string tagAttr(const std::string& s, size_t tag, const char* key) {
    size_t end = s.find('>', tag);
    std::string k = std::string(key) + "=\"";
    size_t a = s.find(k, tag);
    if (a == std::string::npos || a > end) return std::string();
    a += k.size();
    size_t b = s.find('"', a);
    return s.substr(a, b - a);
}

void parseNumbers(CineAttr& a) {
    const char* p = a.value.c_str();
    for (int i = 0; i < 4; ++i) {
        char* e = nullptr;
        a.f[i] = std::strtof(p, &e);
        if (e == p) break;
        p = e;
        while (*p == ',' || *p == ' ') ++p;
    }
    if (a.type == "int") a.i = std::atoi(a.value.c_str());
    else if (a.type == "bool") a.i = (a.value == "true") ? 1 : 0;
    else a.i = (int)a.f[0];
}

} // namespace

bool Cinematic::load(const std::string& path, std::string& err) {
    std::string s;
    if (!readUtf16(path, s)) { err = "cannot read " + path; return false; }
    threads.clear(); durationMs = 0;
    size_t pos = 0;
    while ((pos = s.find("<cinematicThread", pos)) != std::string::npos) {
        CineThread th;
        th.type = std::atoi(tagAttr(s, pos, "type").c_str());
        th.name = tagAttr(s, pos, "name");
        th.objectId = std::atoi(tagAttr(s, pos, "object").c_str());
        size_t tend = s.find("</cinematicThread>", pos);
        if (tend == std::string::npos) tend = s.size();
        size_t tp = pos;
        while ((tp = s.find("<time stamp=", tp)) != std::string::npos && tp < tend) {
            uint32_t stamp = (uint32_t)std::atol(tagAttr(s, tp, "stamp").c_str());
            size_t timeEnd = s.find("</time>", tp);
            if (timeEnd == std::string::npos || timeEnd > tend) timeEnd = tend;
            size_t cp = tp;
            while ((cp = s.find("<command name=", cp)) != std::string::npos && cp < timeEnd) {
                CineCommand c;
                c.stampMs = stamp;
                c.name = tagAttr(s, cp, "name");
                c.id = std::atoi(tagAttr(s, cp, "id").c_str());
                size_t cend = s.find("</command>", cp);
                if (cend == std::string::npos || cend > timeEnd) cend = timeEnd;
                size_t ap = cp;
                while ((ap = s.find("<", ap + 1)) != std::string::npos && ap < cend) {
                    size_t sp = s.find(' ', ap);
                    if (sp == std::string::npos || sp > cend) break;
                    std::string tag = s.substr(ap + 1, sp - ap - 1);
                    if (tag == "int" || tag == "float" || tag == "bool" || tag == "string" ||
                        tag == "vector3d" || tag == "quaternion") {
                        CineAttr a;
                        a.type = tag;
                        a.name = tagAttr(s, ap, "name");
                        a.value = tagAttr(s, ap, "value");
                        parseNumbers(a);
                        c.attrs.push_back(a);
                    }
                }
                th.commands.push_back(c);
                if (stamp > durationMs) durationMs = stamp;
                cp = cend;
            }
            tp = timeEnd;
        }
        threads.push_back(th);
        pos = tend;
    }
    if (threads.empty()) { err = "no cinematic threads in " + path; return false; }
    return true;
}

const CineThread* Cinematic::thread(int type) const {
    for (const CineThread& t : threads) if (t.type == type) return &t;
    return nullptr;
}

const CineThread* Cinematic::threadNamed(const std::string& name) const {
    for (const CineThread& t : threads) if (t.name == name) return &t;
    return nullptr;
}

static bool poseFromThread(const CineThread& th, uint32_t t, Vec3& pos, float& yaw) {
    const CineCommand* before = nullptr; const CineCommand* after = nullptr;
    for (const CineCommand& c : th.commands) {
        if (c.name != "MoveObject") continue;
        if (c.stampMs <= t) before = &c; else { after = &c; break; }
    }
    if (!before && !after) return false;
    const CineCommand* a = before ? before : after;
    Vec3 p0, p1;
    if (!a->vec3("pos", p0)) return false;
    const CineAttr* r = a->attr("rot");
    yaw = r ? Cinematic::yawFromQuat(r->f[0], r->f[1], r->f[2], r->f[3]) : 0.0f;
    pos = p0;
    if (before && after && after->vec3("pos", p1) && after->stampMs > before->stampMs) {
        float u = (float)(t - before->stampMs) / (float)(after->stampMs - before->stampMs);
        if (u < 0) u = 0;
        if (u > 1) u = 1;
        pos = Vec3{ p0.x + (p1.x - p0.x) * u, p0.y + (p1.y - p0.y) * u, p0.z + (p1.z - p0.z) * u };
    }
    return true;
}

bool Cinematic::objectPoseAt(int objectId, uint32_t t, Vec3& pos, float& yaw) const {
    for (const CineThread& th : threads)
        if (th.objectId == objectId && th.type != 2) return poseFromThread(th, t, pos, yaw);
    return false;
}

std::vector<int> Cinematic::hiddenObjectsAt(uint32_t t) const {
    std::vector<int> out;
    std::vector<std::pair<int, bool>> byId;   // Basic-thread SetVisible ObjectID -> latest state
    for (const CineThread& th : threads) {
        bool hidden = false;
        for (const CineCommand& c : th.commands) {
            if (c.stampMs > t) break;
            if (c.name != "SetVisible") continue;
            if (const CineAttr* id = c.attr("ObjectID")) {
                bool found = false;
                for (auto& e : byId) if (e.first == id->i) { e.second = !c.flag("Visible"); found = true; }
                if (!found) byId.push_back({ id->i, !c.flag("Visible") });
            } else hidden = !c.flag("Visible");
        }
        if (hidden) out.push_back(th.objectId);
    }
    for (const auto& e : byId) if (e.second) out.push_back(e.first);
    return out;
}

std::vector<Cinematic::Message> Cinematic::messages() const {
    std::vector<Message> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "ShowMessage") {
                Message m;
                m.stampMs = c.stampMs;
                m.timerMs = (uint32_t)std::max(0.0f, c.num("Timer", 0));
                m.stringId = c.str("$LEVEL_STRINGID");
                m.face = (int)c.num("$MessageFace", 0);
                out.push_back(m);
            }
    return out;
}

bool Cinematic::messageAt(uint32_t t, Message& out) const {
    bool found = false;
    for (const Message& m : messages())
        if (m.stampMs <= t && t < m.stampMs + m.timerMs && (!found || m.stampMs >= out.stampMs)) { out = m; found = true; }
    return found;
}

std::vector<Cinematic::Condition> Cinematic::conditions() const {
    std::vector<Condition> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands) {
            if (c.name == "IfObjectDestroyed") out.push_back({ Condition::OBJECT_DESTROYED, (int)c.num("ObjectID", -1), 0 });
            else if (c.name == "IfEnemyDead")  out.push_back({ Condition::ENEMY_DEAD, (int)c.num("IDEnemy", -1), 0 });
            else if (c.name == "IfHealthTo")   out.push_back({ Condition::HEALTH_AT_MOST, (int)c.num("IDEnemy", -1), c.num("Health", 0) });
        }
    return out;
}

std::vector<int> Cinematic::startsBetween(uint32_t t0, uint32_t t1) const {
    std::vector<int> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "StartCinematic" && c.stampMs >= t0 && c.stampMs < t1)
                out.push_back((int)c.num("CinematicID", -1));
    return out;
}

std::vector<std::pair<int, bool>> Cinematic::triggerTogglesBetween(uint32_t t0, uint32_t t1) const {
    std::vector<std::pair<int, bool>> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if ((c.name == "EnableTrigger" || c.name == "DisableTrigger") && c.stampMs >= t0 && c.stampMs < t1)
                out.push_back({ (int)c.num("^ID^Trigger", -1), c.name == "EnableTrigger" });
    return out;
}

std::vector<std::pair<int, bool>> Cinematic::cameraAreaTogglesBetween(uint32_t t0, uint32_t t1) const {
    std::vector<std::pair<int, bool>> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "EnableCameraArea" && c.stampMs >= t0 && c.stampMs < t1)
                out.push_back({ (int)c.num("^ID^CameraArea", -1), c.flag("enable") });
    return out;
}

std::vector<int> Cinematic::savesBetween(uint32_t t0, uint32_t t1) const {
    std::vector<int> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "Save" && c.stampMs >= t0 && c.stampMs < t1) out.push_back((int)c.num("^ID^CheckPoint", -1));
    return out;
}

float Cinematic::damageBetween(uint32_t t0, uint32_t t1) const {
    float total = 0;
    for (const CineThread& th : threads) {
        if (th.type != 3) continue;
        for (const CineCommand& c : th.commands)
            if (c.name == "GetDamage" && c.stampMs >= t0 && c.stampMs < t1) total += c.num("DamageValue", 0);
    }
    return total;
}

std::vector<int> Cinematic::killsBetween(uint32_t t0, uint32_t t1) const {
    std::vector<int> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "KillObject" && c.stampMs >= t0 && c.stampMs < t1) out.push_back(th.objectId);
    return out;
}

std::vector<int> Cinematic::showHealthBetween(uint32_t t0, uint32_t t1) const {
    std::vector<int> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "ShowHealth" && c.stampMs >= t0 && c.stampMs < t1) {
                int id = (int)c.num("ObjectID", -1);
                out.push_back(id >= 0 ? id : th.objectId);
            }
    return out;
}

bool Cinematic::levelEndBetween(uint32_t t0, uint32_t t1) const {
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if ((c.name == "LevelEnd" || c.name == "GameEnd") && c.stampMs >= t0 && c.stampMs < t1) return true;
    return false;
}

bool Cinematic::endsLevel() const {
    CameraRequest cr;
    if (cameraRequest(cr) && cr.levelEnd) return true;
    return levelEndBetween(0, durationMs + 1);
}

std::vector<std::string> Cinematic::unlocksBetween(uint32_t t0, uint32_t t1) const {
    std::vector<std::string> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "Unlock" && c.stampMs >= t0 && c.stampMs < t1) out.push_back(c.str("$SkillID"));
    return out;
}

Cinematic::Interface Cinematic::interfaceAt(uint32_t t) const {
    Interface out; uint32_t best = 0;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "InterfaceControl" && c.stampMs <= t && (!out.set || c.stampMs >= best)) {
                out.set = true; best = c.stampMs;
                out.control = c.flag("ControlEnable");
                out.black = c.flag("BlackEnable");
                out.skip = c.flag("SkipEnable");
                out.arrow = c.flag("ArrowEnable");
                out.attribution = c.flag("AttributionEnable");
            }
    return out;
}

std::vector<Cinematic::Shake> Cinematic::shakesBetween(uint32_t t0, uint32_t t1) const {
    std::vector<Shake> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "ShakeCamera" && c.stampMs >= t0 && c.stampMs < t1)
                out.push_back({ c.num("MaxOff", 0), (int)c.num("ShakeFrame", 0), c.num("XRate", 1), c.num("YRate", 1), c.num("ZRate", 1) });
    return out;
}

std::vector<Cinematic::Tutorial> Cinematic::tutorials() const {
    std::vector<Tutorial> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "Tutorial") {
                Tutorial tu;
                tu.stampMs = c.stampMs;
                tu.titleId = c.str("Title$Tutorial_STRINGID");
                tu.contentId = c.str("Content$Tutorial_STRINGID");
                tu.blackScreen = c.flag("blackScreen");
                tu.timerMs = c.attr("Timer") ? (int)c.num("Timer", -1) : -1;
                tu.button = (int)c.num("$TutorialButton", -1);
                out.push_back(tu);
            }
    return out;
}

bool Cinematic::tutorialAt(uint32_t t, Tutorial& out) const {
    bool found = false;
    for (const Tutorial& tu : tutorials()) {
        if (tu.stampMs > t) continue;
        if (tu.timerMs > 0 && t >= tu.stampMs + (uint32_t)tu.timerMs) continue;
        if (!found || tu.stampMs >= out.stampMs) { out = tu; found = true; }
    }
    return found;
}

float Cinematic::slowMotionAt(uint32_t t) const {
    float div = 1.0f;
    uint32_t best = 0; bool any = false;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "SetSlowMotion" && c.stampMs <= t && (!any || c.stampMs >= best)) {
                any = true; best = c.stampMs;
                float den = c.num("Denominator", 1), on = c.num("TimeOn", 0);
                bool enable = c.flag("Enable");
                div = (enable && den > 0 && (on <= 0 || t < c.stampMs + (uint32_t)on)) ? den : 1.0f;
            }
    return div;
}

std::string Cinematic::expandTutorialMarkup(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '^' && i + 1 < text.size()) {
            char k = (char)std::toupper((unsigned char)text[++i]);
            switch (k) {
                case 'J': out += "JUMP"; break;
                case 'K': out += "PUNCH"; break;
                case 'L': out += "WEB"; break;
                case 'D': out += "THE STICK"; break;
                case 'S': out += "DODGE"; break;
                case 'I': out += "GRAB"; break;
                case 'T': out += "THE WEB ICON"; break;
                default: break;   // ^0..^9 colour codes and anything unknown vanish
            }
            continue;
        }
        if (c == '\r') continue;
        out.push_back(c == '\n' ? ' ' : c);
    }
    return out;
}

bool Cinematic::cameraRequest(CameraRequest& out) const {
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "PlayDAECamera") {
                out.file = c.str("CameraAnimFile");
                for (char& ch : out.file) if (ch == '\\') ch = '/';
                out.stampMs = c.stampMs;
                out.nextCinematic = (int)c.num("^ID^Cinematic^Next", -1);
                out.farPlane = c.num("farPlane", 0);
                out.levelEnd = c.flag("level end");
                return true;
            }
    return false;
}

std::vector<Cinematic::DaeAnim> Cinematic::daeAnims() const {
    std::vector<DaeAnim> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "PlayDAEAnim") {
                std::string f = c.str("AnimFile");
                for (char& ch : f) if (ch == '\\') ch = '/';
                out.push_back({ th.objectId, f, (int)c.num("clipID", 0), c.stampMs });
            }
    return out;
}

std::vector<Cinematic::Qte> Cinematic::qtes() const {
    std::vector<Qte> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "StartQTE")
                out.push_back({ c.stampMs, (int)c.num("QTEID", 0), (int)c.num("^ID^Cinematic^Success", -1),
                                (int)c.num("^ID^Cinematic^Fail", -1) });
    return out;
}

bool Cinematic::daeAnimAt(int objectId, uint32_t t, DaeAnim& out) const {
    bool found = false;
    for (const DaeAnim& a : daeAnims())
        if (a.objectId == objectId && a.stampMs <= t && (!found || a.stampMs >= out.stampMs)) { out = a; found = true; }
    return found;
}

bool Cinematic::nextQteAfter(uint32_t t, Qte& out) const {
    bool found = false;
    for (const Qte& q : qtes())
        if (q.stampMs >= t && (!found || q.stampMs < out.stampMs)) { out = q; found = true; }
    return found;
}

namespace {
std::string lowerStr(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
// "web_rope_ci_0_lv1_start.bdae" -> "web_rope_ci_lv1_start.bdae"
std::string collapseNumericTokens(const std::string& name) {
    std::string out; size_t i = 0;
    while (i < name.size()) {
        if (name[i] == '_') {
            size_t j = i + 1;
            while (j < name.size() && std::isdigit((unsigned char)name[j])) ++j;
            if (j > i + 1 && j < name.size() && name[j] == '_') { i = j; continue; }   // drop "_123" before "_"
        }
        out.push_back(name[i]); ++i;
    }
    return out;
}
} // namespace

std::string resolveAnimVariant(const std::string& dir, const std::string& requestedBasename) {
    std::string want = lowerStr(requestedBasename);
    DIR* d = opendir(dir.c_str());
    if (!d) return std::string();
    std::string exact, variant; int variants = 0;
    while (struct dirent* e = readdir(d)) {
        std::string n = lowerStr(e->d_name);
        if (n == want) { exact = e->d_name; break; }
        if (collapseNumericTokens(n) == collapseNumericTokens(want)) { variant = e->d_name; ++variants; }
    }
    closedir(d);
    if (!exact.empty()) return dir + "/" + exact;
    if (variants == 1) return dir + "/" + variant;    // ambiguous variants stay unresolved
    return std::string();
}

bool CameraTrack::load(const std::string& bdaePath, std::string& err) {
    std::string e;
    model = Model();
    // the scene graph (node names) is read by the mesh pass; a camera file has
    // no renderable geometry, so that pass reports failure but still fills nodes
    model.loadMesh(bdaePath, e);
    e.clear();
    if (!model.loadAnimation(bdaePath, e) || model.clips.empty()) { err = e.empty() ? "no clips in " + bdaePath : e; return false; }
    eyeNode = targetNode = -1;
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        const std::string& n = model.nodes[i].name;
        if (n.find("Target") != std::string::npos) { if (targetNode < 0) targetNode = (int)i; }
        else if (n.rfind("Camera", 0) == 0 && eyeNode < 0) eyeNode = (int)i;
    }
    if (eyeNode < 0 && !model.nodes.empty()) eyeNode = 0;
    if (targetNode < 0 && model.nodes.size() > 1) targetNode = 1;
    durationMs = model.clips[0].endMs > model.clips[0].startMs ? model.clips[0].endMs - model.clips[0].startMs : 0;
    return eyeNode >= 0;
}

bool CameraTrack::sample(uint32_t t, Vec3& eye, Vec3& target) {
    if (eyeNode < 0 || model.clips.empty()) return false;
    if (t > durationMs) t = durationMs;
    model.poseAtTime(model.clips[0].startMs + t);
    const std::vector<Mat4>& W = model.worldTransforms();
    if (eyeNode >= (int)W.size()) return false;
    eye = Vec3{ W[eyeNode].m[12], W[eyeNode].m[13], W[eyeNode].m[14] };
    if (targetNode >= 0 && targetNode < (int)W.size())
        target = Vec3{ W[targetNode].m[12], W[targetNode].m[13], W[targetNode].m[14] };
    else target = Vec3{ eye.x, eye.y + 100.0f, eye.z };
    return true;
}

float Cinematic::yawFromQuat(float x, float y, float z, float w) {
    return std::atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
}

bool Cinematic::playerPoseAt(uint32_t t, Vec3& pos, float& yaw) const {
    const CineThread* th = thread(3);
    if (!th) return false;
    const CineCommand* before = nullptr; const CineCommand* after = nullptr;
    for (const CineCommand& c : th->commands) {
        if (c.name != "MoveObject") continue;
        if (c.stampMs <= t) before = &c;
        else { after = &c; break; }
    }
    if (!before && !after) return false;
    Vec3 p0, p1;
    const CineCommand* a = before ? before : after;
    if (!a->vec3("pos", p0)) return false;
    const CineAttr* r = a->attr("rot");
    yaw = r ? yawFromQuat(r->f[0], r->f[1], r->f[2], r->f[3]) : 0.0f;
    pos = p0;
    if (before && after && after->vec3("pos", p1) && after->stampMs > before->stampMs) {
        float u = (float)(t - before->stampMs) / (float)(after->stampMs - before->stampMs);
        if (u < 0) u = 0;
        if (u > 1) u = 1;
        pos = Vec3{ p0.x + (p1.x - p0.x) * u, p0.y + (p1.y - p0.y) * u, p0.z + (p1.z - p0.z) * u };
    }
    return true;
}

std::string Cinematic::playerAnimAt(uint32_t t) const {
    const CineThread* th = thread(3);
    std::string clip;
    if (!th) return clip;
    for (const CineCommand& c : th->commands)
        if (c.name == "SetAnim" && c.stampMs <= t) clip = c.str("$Anim");
    return clip;
}

bool Cinematic::cameraAt(uint32_t t, CineCamera& out) const {
    const CineThread* th = thread(2);
    if (!th) return false;
    bool found = false;
    for (const CineCommand& c : th->commands) {
        if (c.name != "ChangeCamera" || c.stampMs > t) continue;
        c.vec3("target", out.target);
        c.vec3("dir", out.dir);
        out.distance = c.num("Distance", 800.0f);
        found = true;
    }
    return found;
}

std::vector<std::string> Cinematic::soundsBetween(uint32_t t0, uint32_t t1) const {
    std::vector<std::string> out;
    for (const CineThread& th : threads)
        for (const CineCommand& c : th.commands)
            if (c.name == "SoundControl" && c.stampMs >= t0 && c.stampMs < t1 &&
                (c.flag("Play2D") || c.flag("Play3D")) && !c.flag("Stop"))
                out.push_back(c.str("$VoxSounds"));
    return out;
}

std::vector<std::string> Cinematic::aiDisabledAt(uint32_t t) const {
    std::vector<std::string> out;
    for (const CineThread& th : threads) {
        if (th.type != 0) continue;
        bool disabled = false;
        for (const CineCommand& c : th.commands) {
            if (c.stampMs > t) break;
            if (c.name == "DisableAI") disabled = true;
            else if (c.name == "EnableAI") disabled = false;
        }
        if (disabled) out.push_back(th.name);
    }
    return out;
}

} // namespace bdae
