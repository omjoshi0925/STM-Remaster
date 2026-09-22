// WavClip on a synthetic 16-bit PCM file, no assets.
#include "Audio.hpp"
#include "unit_common.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
using namespace bdae;
static void u32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 255); }
static void u16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v & 255); b.push_back(v >> 8); }
int main() {
    std::vector<uint8_t> b;
    const int N = 100;
    b.insert(b.end(), {'R','I','F','F'}); u32(b, 36 + N * 2); b.insert(b.end(), {'W','A','V','E'});
    b.insert(b.end(), {'f','m','t',' '}); u32(b, 16); u16(b, 1); u16(b, 1); u32(b, 22050); u32(b, 44100); u16(b, 2); u16(b, 16);
    b.insert(b.end(), {'d','a','t','a'}); u32(b, N * 2);
    for (int i = 0; i < N; ++i) u16(b, (uint16_t)(int16_t)(i * 100 - 5000));
    char path[] = "/tmp/stmwavXXXXXX"; int fd = mkstemp(path); FILE* f = fdopen(fd, "wb"); fwrite(b.data(), 1, b.size(), f); fclose(f);
    WavClip c; std::string e;
    ck(c.load(path, e), "PCM16 WAV loads", e);
    ck(c.sampleRate == 22050 && c.channels == 1 && c.samples.size() == N, "format and length preserved");
    ck(c.samples[0] == -5000 && c.samples[N - 1] == (int16_t)(99 * 100 - 5000), "samples pass through untouched");
    ck(c.seconds() > 0.0044 && c.seconds() < 0.0046, "duration computed from rate");
    UNIT_END("WAV UNIT");
}
