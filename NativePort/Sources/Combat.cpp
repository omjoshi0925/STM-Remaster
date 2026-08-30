#include "Combat.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <vector>

namespace bdae {

// ------------------------------------------------------ config table parsing
static bool readAll(const std::string& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end); out.resize((size_t)f.tellg()); f.seekg(0);
    f.read((char*)out.data(), out.size());
    return true;
}
static bool printable(const uint8_t* p, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) if (p[i] < 32 || p[i] > 126) return false;
    return true;
}

std::map<std::string, EnemyStats> loadEnemyStats(const std::string& configsDir,
                                                 std::string& err) {
    std::map<std::string, EnemyStats> out;
    std::vector<uint8_t> b;
    if (!readAll(configsDir + "/EnemysAttributeConfigs.bin", b)) {
        err = "cannot read EnemysAttributeConfigs.bin"; return out;
    }
    // Stream of [u16-len string | 4-byte number] fields; a record starts at an
    // UPPERCASE_NAME string.  Numeric slot k counts numbers since record start.
    size_t off = 4;
    std::string cur; int slot = 0;
    while (off + 1 < b.size()) {
        uint16_t L; std::memcpy(&L, &b[off], 2);
        if (L >= 2 && L <= 48 && off + 2 + L <= b.size() && printable(&b[off + 2], L)) {
            std::string s((char*)&b[off + 2], L);
            bool upper = true;
            for (char c : s) if (!(std::isupper((unsigned char)c) || c == '_' || std::isdigit((unsigned char)c))) { upper = false; break; }
            if (upper) { cur = s; slot = 0; out[cur] = EnemyStats{}; }
            off += 2 + L; continue;
        }
        if (off + 4 > b.size()) break;
        uint32_t u; float f;
        std::memcpy(&u, &b[off], 4); std::memcpy(&f, &b[off], 4);
        if (!cur.empty()) {
            EnemyStats& st = out[cur];
            if      (slot == 0) st.hp = (float)u;
            else if (slot == 1) st.moveSpeed = (float)u;
            else if (slot == 2) st.noticeRange = (float)u;
            else if (slot == 7) st.attackRange = (float)u;
            else if (slot == 9) st.rangedRange = (float)u;
            ++slot;
        }
        off += 4;
    }
    // Common melee attack interval (ENEMY_MELEE_ATTACK_COMMON -> 2000 ms).
    std::vector<uint8_t> b2;
    uint32_t interval = 2000;
    if (readAll(configsDir + "/AttackIntervalTimeConfigs.bin", b2) && b2.size() > 40) {
        // first record: [u16-len name][u32 ...][u32 interval...] - take the first
        // value in 500..10000 after the name as the common interval.
        size_t o = 4;
        uint16_t L; std::memcpy(&L, &b2[o], 2); o += 2 + L;
        for (int k = 0; k < 8 && o + 4 <= b2.size(); ++k, o += 4) {
            uint32_t v; std::memcpy(&v, &b2[o], 4);
            if (v >= 500 && v <= 10000) { interval = v; break; }
        }
    }
    for (auto& kv : out) kv.second.attackIntervalMs = interval;
    if (out.empty()) err = "no enemy records parsed";
    return out;
}

const Clip* pickClip(const Model& m, std::initializer_list<const char*> names) {
    for (const char* n : names) if (const Clip* c = m.findClip(n)) return c;
    return nullptr;
}

// ----------------------------------------------------------------- EnemyActor
void EnemyActor::bind(Model* m, const LevelRoom* lvl, const EnemyStats& s,
                      float px, float py, float pz, float pyaw) {
    model = m; level = lvl; stats = s; hp = s.hp;
    x = px; y = py; z = pz; yaw = pyaw;
    cIdle   = pickClip(*m, {"idle", "idle_cycle"});
    cWalk   = pickClip(*m, {"walk", "walk_cycle", "run", "move"});
    cAttack = pickClip(*m, {"idle_at1_idle", "idle_knife_at_idle", "idle_at2_idle",
                            "idle_attack_hammer_idle", "idlebaz_rush_attack_idlebaz",
                            "idle_gore_attack_idle"});
    cHurt   = pickClip(*m, {"idle_hurt_idle", "idle_hurt_left_idle",
                            "idle_hurt_right_idle", "idle_hurt_heavy_idle"});
    cDie    = pickClip(*m, {"die", "death", "dead", "idle_to_onground",
                            "air_to_onground", "air_attack_to_onground"});
    state = IDLE; stateStartMs = 0;
}

static uint32_t clipLen(const Clip* c) { return c ? (c->endMs - c->startMs) : 1; }

bool EnemyActor::update(uint32_t nowMs, uint32_t dtMs, const Vec3& hero) {
    if (state == DEAD) return false;
    float dx = hero.x - x, dy = hero.y - y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist > 1.0f) {
        float targetYaw = std::atan2(dy, dx);
        if (state == CHASE || state == ATTACK || state == COOLDOWN) yaw = targetYaw;
    }
    switch (state) {
        case IDLE:
            if (dist < stats.noticeRange) { state = CHASE; stateStartMs = nowMs; }
            break;
        case CHASE: {
            if (dist <= stats.attackRange * 0.9f) { state = ATTACK; stateStartMs = nowMs; break; }
            float step = stats.moveSpeed * (dtMs / 1000.0f);
            float nx = x + (dx / dist) * step, ny = y + (dy / dist) * step, nz;
            if (level && level->canStandAt(nx, ny, nz)) { x = nx; y = ny; z = nz; }
            else if (level && level->canStandAt(nx, y, nz)) { x = nx; z = nz; }
            else if (level && level->canStandAt(x, ny, nz)) { y = ny; z = nz; }
            break;
        }
        case ATTACK:
            if (nowMs - stateStartMs >= clipLen(cAttack)) {
                state = COOLDOWN; stateStartMs = nowMs;
                return dist <= stats.attackRange * 1.2f;   // strike lands at clip end
            }
            break;
        case COOLDOWN:
            if (nowMs - stateStartMs >= stats.attackIntervalMs) {
                state = (dist <= stats.attackRange * 0.9f) ? ATTACK : CHASE;
                stateStartMs = nowMs;
            }
            break;
        case HURT:
            if (nowMs - stateStartMs >= clipLen(cHurt)) { state = CHASE; stateStartMs = nowMs; }
            break;
        case DEAD: break;
    }
    return false;
}

void EnemyActor::takeHit(float dmg, uint32_t nowMs) {
    if (state == DEAD) return;
    hp -= dmg;
    if (hp <= 0) { hp = 0; state = DEAD; }
    else         { state = HURT; }
    stateStartMs = nowMs;
}

void EnemyActor::poseInfo(uint32_t nowMs, const Clip*& clip, uint32_t& tl) const {
    const Clip* c = cIdle;
    uint32_t local = nowMs - stateStartMs;
    switch (state) {
        case CHASE:    c = cWalk ? cWalk : cIdle; break;
        case ATTACK:   c = cAttack ? cAttack : cIdle; break;
        case HURT:     c = cHurt ? cHurt : cIdle; break;
        case DEAD:     c = cDie ? cDie : cIdle;
                       clip = c; tl = c->startMs + std::min(local, clipLen(c) - 1); return;
        default: break;
    }
    if (!c) { clip = nullptr; tl = 0; return; }
    clip = c; tl = c->startMs + (clipLen(c) ? local % clipLen(c) : 0);
}

// ----------------------------------------------------------------- HeroCombat
void HeroCombat::bind(const Model& hero) {
    cWindup  = pickClip(hero, {"idle_to_punch_right"});
    cRecover = pickClip(hero, {"punch_right_to_idle"});
}
bool HeroCombat::punching(uint32_t nowMs) const {
    if (!punchStartMs) return false;
    return nowMs - punchStartMs < clipLen(cWindup) + clipLen(cRecover);
}
void HeroCombat::tryPunch(uint32_t nowMs) {
    if (!punching(nowMs)) { punchStartMs = nowMs; hitApplied = false; }
}
int HeroCombat::update(uint32_t nowMs, const Vec3& pos, float yaw,
                       std::vector<EnemyActor>& enemies) {
    if (!punchStartMs || hitApplied) return 0;
    if (nowMs - punchStartMs < clipLen(cWindup)) return 0;   // strike at windup end
    hitApplied = true;
    int hits = 0;
    float fx = std::cos(yaw), fy = std::sin(yaw);
    for (EnemyActor& e : enemies) {
        if (!e.alive()) continue;
        float dx = e.x - pos.x, dy = e.y - pos.y;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d > reach) continue;
        if (d > 1.0f && (dx * fx + dy * fy) / d < 0.25f) continue;   // ~75-degree arc
        e.takeHit(damage, nowMs);
        ++hits;
    }
    return hits;
}
bool HeroCombat::poseInfo(uint32_t nowMs, const Clip*& clip, uint32_t& tl) const {
    if (!punching(nowMs)) return false;
    uint32_t local = nowMs - punchStartMs;
    if (local < clipLen(cWindup)) { clip = cWindup; tl = cWindup->startMs + local; }
    else { clip = cRecover; tl = cRecover->startMs + (local - clipLen(cWindup)); }
    return true;
}

} // namespace bdae
