#include "Audio.hpp"
#include <cstdio>
#include <cstring>

namespace bdae {
namespace {

bool readAll(const std::string& p, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0) { std::fclose(f); return false; }
    out.resize((size_t)n);
    size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}

bool printable(const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; ++i) if (p[i] < 32 || p[i] > 126) return false;
    return true;
}

uint16_t rd16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }
uint32_t rd32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }

// IMA ADPCM (DVI/Intel) tables - the standard ones.
const int kIndexTable[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
const int kStepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767 };

inline int16_t imaStep(uint8_t nib, int& pred, int& index) {
    int step = kStepTable[index];
    int diff = step >> 3;
    if (nib & 1) diff += step >> 2;
    if (nib & 2) diff += step >> 1;
    if (nib & 4) diff += step;
    if (nib & 8) pred -= diff; else pred += diff;
    if (pred > 32767) pred = 32767;
    if (pred < -32768) pred = -32768;
    index += kIndexTable[nib];
    if (index < 0) index = 0;
    if (index > 88) index = 88;
    return (int16_t)pred;
}

} // namespace

bool VoxTable::load(const std::string& configsDir, std::string& err) {
    std::vector<uint8_t> b;
    if (!readAll(configsDir + "/VoxSounds.bin", b)) { err = "cannot read VoxSounds.bin"; return false; }
    auto stringAt = [&](size_t o, uint16_t& L) {
        if (o + 2 > b.size()) return false;
        L = rd16(&b[o]);
        return L >= 2 && L <= 80 && o + 2 + L <= b.size() && printable(&b[o + 2], L);
    };
    size_t off = 4;
    std::string pendingEvent;
    while (off + 1 < b.size()) {
        uint16_t L = 0;
        if (!stringAt(off, L) && stringAt(off + 2, L)) off += 2;   // 2-byte alignment pad
        if (stringAt(off, L)) {
            std::string s((const char*)&b[off + 2], L);
            off += 2 + L;
            if (s.size() > 4 && s.compare(s.size() - 4, 4, ".wav") == 0) {
                if (!pendingEvent.empty()) {
                    VoxEvent ev;
                    ev.file = s;
                    if (off + 4 <= b.size()) ev.flags = rd32(&b[off]);
                    byName[pendingEvent] = ev;
                    pendingEvent.clear();
                }
            } else {
                pendingEvent = s;
            }
            continue;
        }
        off += 4;
    }
    if (byName.empty()) { err = "no events parsed"; return false; }
    return true;
}

EnemySounds VoxTable::soundsFor(const std::string& statName) const {
    EnemySounds s;
    auto pick = [&](const std::string& name) { return find(name) ? name : std::string(); };
    for (int i = 0; i < 3; ++i) {
        s.hurt[i] = pick("SFX_" + statName + "_HURT_" + std::to_string(i + 1));
        if (s.hurt[i].empty()) s.hurt[i] = pick("SFX_HURT_" + std::to_string(i + 1));
    }
    s.dies = pick("SFX_" + statName + "_DIES");
    if (s.dies.empty()) s.dies = pick("SFX_DIE");
    s.voice = pick("SFX_" + statName + "_VOICE_1");
    if (s.voice.empty()) s.voice = pick("SFX_THUG_VOICE_1");
    return s;
}

std::string VoxTable::bossMusicFor(const std::string& statName) const {
    std::string m = "M_BOSS_" + statName;
    return find(m) ? m : std::string();
}

bool WavClip::load(const std::string& path, std::string& err) {
    std::vector<uint8_t> b;
    if (!readAll(path, b)) { err = "cannot read " + path; return false; }
    if (b.size() < 44 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(&b[8], "WAVE", 4) != 0) {
        err = "not a RIFF/WAVE file"; return false;
    }
    uint16_t fmt = 0, blockAlign = 0, bits = 0;
    size_t dataOff = 0, dataLen = 0;
    for (size_t p = 12; p + 8 <= b.size();) {
        uint32_t sz = rd32(&b[p + 4]);
        if (std::memcmp(&b[p], "fmt ", 4) == 0 && p + 8 + 16 <= b.size()) {
            fmt = rd16(&b[p + 8]);
            channels = rd16(&b[p + 10]);
            sampleRate = rd32(&b[p + 12]);
            blockAlign = rd16(&b[p + 20]);
            bits = rd16(&b[p + 22]);
        } else if (std::memcmp(&b[p], "data", 4) == 0) {
            dataOff = p + 8;
            dataLen = (p + 8 + sz <= b.size()) ? sz : b.size() - (p + 8);
        }
        p += 8 + sz + (sz & 1);
    }
    if (!dataOff || !channels || !sampleRate) { err = "malformed WAV chunks"; return false; }

    if (fmt == 1 && bits == 16) {
        samples.resize(dataLen / 2);
        std::memcpy(samples.data(), &b[dataOff], samples.size() * 2);
        return true;
    }
    if (fmt != 17 || !blockAlign) { err = "unsupported WAV format " + std::to_string(fmt); return false; }

    // IMA ADPCM: each block starts with a per-channel preamble
    // (int16 predictor, uint8 index, uint8 pad), then interleaved 4-byte
    // groups of 8 nibbles per channel.
    const size_t nBlocks = dataLen / blockAlign;
    samples.clear();
    samples.reserve(nBlocks * (size_t)((blockAlign - 4 * channels) * 2 + channels));
    std::vector<int> pred(channels), index(channels);
    for (size_t blk = 0; blk < nBlocks; ++blk) {
        const uint8_t* p = &b[dataOff + blk * blockAlign];
        for (int c = 0; c < channels; ++c) {
            pred[c] = (int16_t)rd16(p + c * 4);
            index[c] = p[c * 4 + 2];
            if (index[c] > 88) index[c] = 88;
            samples.push_back((int16_t)pred[c]);
        }
        size_t groups = (blockAlign - 4 * (size_t)channels) / (4 * (size_t)channels);
        const uint8_t* q = p + 4 * channels;
        std::vector<int16_t> tmp((size_t)channels * 8);
        for (size_t g = 0; g < groups; ++g) {
            for (int c = 0; c < channels; ++c) {
                const uint8_t* src = q + (size_t)c * 4;
                for (int k = 0; k < 4; ++k) {
                    tmp[(size_t)(k * 2) * channels + c]     = imaStep(src[k] & 0x0f, pred[c], index[c]);
                    tmp[(size_t)(k * 2 + 1) * channels + c] = imaStep(src[k] >> 4, pred[c], index[c]);
                }
            }
            samples.insert(samples.end(), tmp.begin(), tmp.end());
            q += 4 * (size_t)channels;
        }
    }
    return !samples.empty();
}

} // namespace bdae
