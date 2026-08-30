// UIKitData.hpp - Milestone 7: the original UI data.
//  * BSprite: Gameloft .bsprite atlas module table (rects into the .tga atlas).
//    Header not fully decoded; the module table is located by scanning for the
//    longest run of in-bounds (x,y,w,h) u16 quads - validated in host tests.
//  * GameScreen: GS_*.json screen definitions (JSON with //-comments).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace bdae {

struct SpriteModule { uint16_t x, y, w, h; };

struct BSprite {
    std::vector<SpriteModule> modules;
    uint16_t atlasW = 512, atlasH = 512;
    bool load(const std::string& path, std::string& err);
};

// Minimal GS_*.json reader: strips //-comments, then extracts the widget
// counts we need for verification (full JSON parsing lives in future menu work).
struct GameScreen {
    std::string name;
    int buttonCount = 0, spriteCount = 0, textCount = 0;
    bool load(const std::string& path, std::string& err);
};

} // namespace bdae
