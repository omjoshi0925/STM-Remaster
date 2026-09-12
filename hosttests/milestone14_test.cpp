// Milestone 14 headless verification: the original audio set.
#include "Audio.hpp"
#include <cstdio>
#include <cmath>
#include <set>
using namespace bdae;
static int fails = 0;
static void ck(bool c, const char* w, const std::string& d = "") {
    std::printf("  [%s] %s%s%s\n", c ? "PASS" : "FAIL", w, d.empty() ? "" : "  -> ", d.c_str());
    if (!c) ++fails;
}
int main(int argc, char** argv) {
    const std::string root = (argc > 1 ? argv[1] : "Assets");
    std::string e;

    VoxTable vox;
    ck(vox.load(root + "/configs", e), "VoxSounds.bin parses", e);
    std::printf("events: %zu\n", vox.byName.size());
    ck(vox.byName.size() >= 480, "the full event table is recovered",
       std::to_string(vox.byName.size()));

    // the events Level 1 gameplay actually needs
    const char* needed[] = {"M_DOWNTOWN_CALM", "SFX_PUNCH_IMPACT_1", "SFX_PUNCH_SWOOSH_1",
                            "SFX_HURT_1", "SFX_DIE", "SFX_THUG_KNIFE_DIES",
                            "SFX_WEB_THROW_1", "SFX_LAND", "M_WIN", "M_LOSE"};
    std::string missing;
    for (const char* n : needed) if (!vox.find(n)) missing += std::string(n) + " ";
    ck(missing.empty(), "every event the runtime references exists", missing);
    const VoxEvent* mus = vox.find("M_DOWNTOWN_CALM");
    ck(mus && mus->music() && mus->file == "music/m_downtown_calm.wav",
       "music events map to music/ paths", mus ? mus->file : "");

    // every referenced file is actually on disk
    int have = 0, absent = 0;
    std::string firstAbsent;
    for (auto& kv : vox.byName) {
        std::string p = root + "/sounds/" + kv.second.file;
        FILE* f = fopen(p.c_str(), "rb");
        if (f) { ++have; fclose(f); }
        else { ++absent; if (firstAbsent.empty()) firstAbsent = kv.second.file; }
    }
    std::printf("files: %d present, %d absent %s\n", have, absent, firstAbsent.c_str());
    ck(absent == 0, "every event's clip ships in sounds.pack", firstAbsent);

    // decode: a music bed and a short effect
    struct { const char* ev; double minSec; double maxSec; } probes[] = {
        {"M_DOWNTOWN_CALM", 5.0, 400.0},
        {"SFX_PUNCH_IMPACT_1", 0.02, 5.0},
        {"SFX_DIE", 0.05, 10.0},
    };
    for (auto& p : probes) {
        const VoxEvent* ev = vox.find(p.ev);
        if (!ev) { ck(false, p.ev, "event missing"); continue; }
        WavClip clip;
        bool ok = clip.load(root + "/sounds/" + ev->file, e);
        char d[160];
        snprintf(d, sizeof d, "%u Hz, %u ch, %zu samples, %.2f s",
                 clip.sampleRate, clip.channels, clip.samples.size(), clip.seconds());
        ck(ok && clip.sampleRate >= 11025 && clip.channels >= 1 &&
           clip.seconds() > p.minSec && clip.seconds() < p.maxSec, p.ev, ok ? d : e);
    }

    // decoded audio must be real signal, not silence or clipping
    const VoxEvent* ev = vox.find("SFX_PUNCH_IMPACT_1");
    WavClip clip;
    if (ev && clip.load(root + "/sounds/" + ev->file, e)) {
        double sum = 0; int peak = 0;
        for (int16_t s : clip.samples) { sum += (double)s * s; peak = std::max(peak, std::abs((int)s)); }
        double rms = std::sqrt(sum / std::max<size_t>(1, clip.samples.size()));
        char d[96]; snprintf(d, sizeof d, "rms %.0f, peak %d", rms, peak);
        ck(rms > 200.0 && peak > 3000 && peak <= 32767, "ADPCM decode yields real signal", d);
    } else ck(false, "punch clip decodes", e);

    // per-archetype sound sets resolve for every enemy the levels place
    const char* stats[] = {"THUG_KNIFE", "THUG_BAT", "THUG_MOLOTOV", "THUG_GUN", "SANDMAN", "RHINO"};
    std::string weak;
    for (const char* st : stats) {
        EnemySounds es = vox.soundsFor(st);
        bool ok = !es.dies.empty() && !es.hurt[0].empty() && !es.voice.empty();
        if (!ok) weak += std::string(st) + " ";
        bool own = es.dies.rfind(std::string("SFX_") + st, 0) == 0;
        std::printf("  %-12s hurt=%s dies=%s%s\n", st, es.hurt[0].c_str(), es.dies.c_str(),
                    own ? "" : "  (generic fallback)");
    }
    ck(weak.empty(), "every archetype resolves hurt, death and voice events", weak);
    ck(vox.soundsFor("THUG_KNIFE").dies == "SFX_THUG_KNIFE_DIES",
       "archetype-specific death events are preferred over the generic one");
    ck(vox.soundsFor("NOT_A_REAL_ENEMY").dies == "SFX_DIE",
       "unknown archetypes fall back to the generic death event");

    // boss music exists for the bosses the first two levels place
    ck(vox.bossMusicFor("SANDMAN") == "M_BOSS_SANDMAN" &&
       vox.bossMusicFor("RHINO") == "M_BOSS_RHINO" &&
       vox.bossMusicFor("THUG_KNIFE").empty(),
       "boss music resolves for bosses only");

    // every clip an archetype set points at must decode
    for (const char* st : {"THUG_KNIFE", "RHINO"}) {
        EnemySounds es = vox.soundsFor(st);
        const VoxEvent* d = vox.find(es.dies);
        WavClip c;
        std::string ce;
        bool ok = d && c.load(root + "/sounds/" + d->file, ce) && c.seconds() > 0.05;
        ck(ok, (std::string(st) + " death clip decodes").c_str(),
           ok ? std::to_string(c.seconds()) + " s" : ce);
    }

    std::printf("\n%s (%d failures)\n", fails ? "MILESTONE 14 AUDIO FAILED" : "MILESTONE 14 AUDIO PASSED", fails);
    return fails ? 1 : 0;
}
