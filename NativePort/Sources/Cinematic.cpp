#include "Cinematic.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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



} // namespace bdae
