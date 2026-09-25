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
    std::vector<std::string> order;      // event names in file order: the row index the slot tables use
    bool load(const std::string& configsDir, std::string& err);
    const VoxEvent* find(const std::string& event) const {
        auto it = byName.find(event);
        return it == byName.end() ? nullptr : &it->second;
    }
    // statName is the EnemysAttributeConfigs row ("THUG_KNIFE", "RHINO").
    EnemySounds soundsFor(const std::string& statName) const;
    // Same, but the original BehaviorSoundMapList decides first; the name
    // convention only fills cells the table leaves empty.
    EnemySounds soundsFor(const std::string& statName, const struct BehaviorSoundMap& map) const;
    // "M_BOSS_SANDMAN" / "M_BOSS_RHINO" ... or "" when the row is not a boss.
    std::string bossMusicFor(const std::string& statName) const;
    // Event name for a VoxSounds row index, "" if out of range.
    std::string rowName(uint32_t row) const { return row < order.size() ? order[row] : std::string(); }
};

// BehaviorSoundMapList.bin: the original per-state enemy sound table. Each
// slot (Voice_1, hurt1, dies, attack_swoosh, gun_shoot ...) holds one
// VoxSounds row index per archetype column, 0xffffffff for none. Column
// order is inferred from the "dies" row (SFX_<ARCHETYPE>_DIES).
struct BehaviorSoundMap {
    std::vector<std::string> slots;                      // row names
    std::vector<std::vector<uint32_t>> rows;             // per slot: column indices
    std::vector<std::string> columnArchetype;            // e.g. THUG_KNIFE, THUG_BAT ...
    bool load(const std::string& configsDir, const VoxTable& vox, std::string& err);
    int columnFor(const std::string& statName) const;    // -1 if unknown
    // Event name for (slot, archetype), "" when the cell is empty.
    std::string event(const std::string& slot, const std::string& statName, const VoxTable& vox) const;
};

// MC_SOUND.bin: the hero's sound slots (k_mc_sfx_swoosh_punch, k_mc_sfx_land,
// k_mc_sfx_hurt ...), each a list of VoxSounds row indices (variants).
struct HeroSoundMap {
    std::map<std::string, std::vector<uint32_t>> variants;
    bool load(const std::string& configsDir, std::string& err);
    // One event name for a slot (variant i modulo count), "" if unknown.
    std::string event(const std::string& slot, const VoxTable& vox, int variant = 0) const;
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
