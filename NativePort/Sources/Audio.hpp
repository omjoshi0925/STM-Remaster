// Audio.hpp - Milestone 14: the original sound set.
//
// VoxSounds.bin is an event table: 493 pairs of (EVENT_NAME, relative wav
// path) plus per-event flags. sounds.pack ships the clips as RIFF WAV in
// IMA ADPCM (format 17, 4-bit). This header decodes both so the platform
// layer only ever deals with PCM16 buffers and event names.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace bdae {

struct VoxEvent {
    std::string file;        // "music/m_downtown_calm.wav"
    uint32_t flags = 0;      // first numeric field (bit 16 set on music rows)
    bool music() const { return file.rfind("music/", 0) == 0; }
};

// Per-archetype sound set. The original slot vocabulary lives in
// BehaviorSoundMapList.bin (Voice_1..12, hurt1..3, dies, attack_*); the
// clips themselves are VoxSounds events named SFX_<STAT>_HURT_n / _DIES /
// _VOICE_n. The slot-to-event numeric linkage is not decoded, so these are
// resolved by name and fall back to the generic hero-side effects.
struct EnemySounds {
    std::string hurt[3];
    std::string dies;
    std::string voice;
};

struct VoxTable {
    std::map<std::string, VoxEvent> byName;
    bool load(const std::string& configsDir, std::string& err);
    const VoxEvent* find(const std::string& event) const {
        auto it = byName.find(event);
        return it == byName.end() ? nullptr : &it->second;
    }
    // statName is the EnemysAttributeConfigs row ("THUG_KNIFE", "RHINO").
    EnemySounds soundsFor(const std::string& statName) const;
    // "M_BOSS_SANDMAN" / "M_BOSS_RHINO" ... or "" when the row is not a boss.
    std::string bossMusicFor(const std::string& statName) const;
};

// Decoded audio: 16-bit signed PCM, interleaved.
struct WavClip {
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    std::vector<int16_t> samples;   // interleaved frames * channels
    double seconds() const {
        return (sampleRate && channels) ? (double)samples.size() / channels / sampleRate : 0.0;
    }
    // Decodes RIFF WAV: PCM (1) passthrough and IMA ADPCM (17) expansion.
    bool load(const std::string& path, std::string& err);
};

} // namespace bdae
