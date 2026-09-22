// Stats are u32 integers after an UPPERCASE name. The config field-stream parser must survive odd-length strings that shift
// the 4-byte phase (the bug that once dropped 15 of 25 stat records).
#include "Combat.hpp"
#include "unit_common.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
using namespace bdae;
static void str(std::vector<uint8_t>& b, const char* s) { uint16_t L = (uint16_t)strlen(s); b.push_back(L & 255); b.push_back(L >> 8); b.insert(b.end(), s, s + L); }
static void num(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 255); }
int main() {
    std::vector<uint8_t> b = {2, 0, 0, 0};
    str(b, "THUG_ODDS");            // 9 chars: the following numerics start at an odd phase
    for (int i = 0; i < 10; ++i) num(b, i == 0 ? 40u : 1u);
    str(b, "even_clip_name_x");     // 16 chars, lowercase (not a record start)
    str(b, "RHINO_T");              // odd again
    for (int i = 0; i < 10; ++i) num(b, i == 0 ? 100u : 2u);
    char dir[] = "/tmp/stmcfgXXXXXX"; mkdtemp(dir);
    std::string d = dir;
    FILE* f = fopen((d + "/EnemysAttributeConfigs.bin").c_str(), "wb"); fwrite(b.data(), 1, b.size(), f); fclose(f);
    std::string e;
    auto st = loadEnemyStats(d, e);
    ck(st.size() == 2, "both records survive odd string lengths", std::to_string(st.size()) + " " + e);
    ck(st.count("THUG_ODDS") && st["THUG_ODDS"].hp == 40.0f, "first record keeps its HP");
    ck(st.count("RHINO_T") && st["RHINO_T"].hp == 100.0f, "second record keeps its HP after a lowercase clip name");
    UNIT_END("CONFIG PARSER UNIT");
}
