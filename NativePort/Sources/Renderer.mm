// Renderer.mm - Milestone 4: skinned Spider-Man + original Level 1 Room 1.
//
// All parsing/animation/collision lives in portable C++ (BDAEModel, Level,
// Character) which is unit-tested on the host.  This file only does Metal.
#import "Renderer.h"
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <AVFoundation/AVFoundation.h>
#include "BDAEModel.hpp"
#include "Level.hpp"
#include "Character.hpp"
#include "Combat.hpp"
#include "UIKitData.hpp"
#include "GameFlow.hpp"
#include <memory>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <simd/simd.h>

using namespace bdae;

// The Spider-Man rig's toes point down -Y, so a yaw of 0 (facing +X) needs a
// +90 degree correction.  Derived from the bind pose, not guessed:
//   Bip01_L_Foot y = +4.95, Bip01_L_Toe0 y = -8.30
static const float kModelYawOffset = (float)M_PI_2;

#define TM_DEBUG_HUD 0   // 1 = show the developer status line
// font_outline_big.tga glyph table, segmented from the atlas and verified by
// rendering (docs/font_proof.png). Uppercase-only, as the original UI is.
struct FontGlyph { char c; uint16_t x, y, w, h; };
static const FontGlyph kFont[] = {
    {'!', 10, 3, 13, 29},
    {'"', 333, 3, 15, 29},
    {'#', 47, 3, 19, 29},
    {'$', 70, 3, 15, 29},
    {'%', 91, 3, 20, 29},
    {'&', 119, 3, 19, 29},
    {'\'', 144, 3, 10, 29},
    {'(', 157, 3, 12, 29},
    {')', 172, 3, 13, 29},
    {'*', 188, 3, 18, 29},
    {'+', 210, 3, 17, 29},
    {',', 233, 3, 10, 29},
    {'-', 246, 3, 15, 29},
    {'.', 265, 3, 9, 29},
    {'/', 278, 3, 16, 29},
    {'0', 20, 33, 18, 23},
    {'1', 44, 33, 11, 23},
    {'2', 60, 33, 18, 23},
    {'3', 81, 33, 18, 23},
    {'4', 104, 33, 18, 23},
    {'5', 127, 33, 18, 23},
    {'6', 148, 33, 16, 23},
    {'7', 170, 33, 18, 23},
    {'8', 192, 33, 18, 23},
    {'9', 214, 33, 17, 23},
    {':', 317, 3, 11, 29},
    {';', 301, 3, 10, 29},
    {'?', 253, 33, 17, 23},
    {'@', 273, 33, 22, 23},
    {'A', 12, 73, 18, 23},
    {'B', 36, 73, 19, 23},
    {'C', 59, 73, 18, 23},
    {'D', 81, 73, 19, 23},
    {'E', 105, 73, 17, 23},
    {'F', 126, 73, 17, 23},
    {'G', 146, 73, 18, 23},
    {'H', 169, 73, 19, 23},
    {'I', 191, 73, 12, 23},
    {'J', 207, 73, 18, 23},
    {'K', 228, 73, 20, 23},
    {'L', 251, 73, 14, 23},
    {'M', 271, 73, 24, 23},
    {'N', 298, 73, 20, 23},
    {'O', 322, 73, 18, 23},
    {'P', 21, 113, 19, 23},
    {'Q', 44, 113, 18, 23},
    {'R', 68, 113, 19, 23},
    {'S', 90, 113, 19, 23},
    {'T', 112, 113, 19, 23},
    {'U', 135, 113, 19, 23},
    {'V', 158, 113, 19, 23},
    {'W', 181, 113, 25, 23},
    {'X', 208, 113, 20, 23},
    {'Y', 233, 113, 18, 23},
    {'Z', 254, 113, 18, 23},
};
static const int kFontCount = (int)(sizeof(kFont) / sizeof(kFont[0]));

static const NSUInteger kMaxBones = 38;
static const NSUInteger kBoneSlot = 2560;      // 40 bones * 64 B, 256-aligned
static const NSUInteger kFramesInFlight = 3;   // bone buffers are rewritten per frame
static const char *kLevelDirs[] = {"levelnew_01", "levelnew_02"};
static const int kLevelCount = 2;

struct GPUSkinVertex { float p[3]; float n[3]; float uv[2]; uint16_t bone[4]; float w[4]; };
struct GPUStaticVertex { float p[3]; float n[3]; float uv[2]; float uv2[2]; uint8_t c[4]; };   // 44 B
struct Uniforms { simd_float4x4 vp; simd_float4x4 model; simd_float4 tint; simd_float4 misc; };

static uint32_t rdle32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static simd_float4x4 MIdent(void) { return matrix_identity_float4x4; }
static simd_float4x4 MTranslate(float x, float y, float z) {
    simd_float4x4 m = MIdent();
    m.columns[3] = (simd_float4){x, y, z, 1};
    return m;
}
static simd_float4x4 MRotZ(float a) {
    float c = cosf(a), s = sinf(a);
    simd_float4x4 m = MIdent();
    m.columns[0] = (simd_float4){ c, s, 0, 0};
    m.columns[1] = (simd_float4){-s, c, 0, 0};
    return m;
}
static simd_float4x4 MPerspective(float fovy, float aspect, float zn, float zf) {
    float y = 1.0f / tanf(fovy * 0.5f), x = y / aspect, z = zf / (zn - zf);
    simd_float4x4 m = (simd_float4x4){};
    m.columns[0] = (simd_float4){x, 0, 0, 0};
    m.columns[1] = (simd_float4){0, y, 0, 0};
    m.columns[2] = (simd_float4){0, 0, z, -1};
    m.columns[3] = (simd_float4){0, 0, z * zn, 0};
    return m;
}
static simd_float4x4 MLookAt(simd_float3 eye, simd_float3 target, simd_float3 up) {
    simd_float3 z = simd_normalize(eye - target);
    simd_float3 x = simd_normalize(simd_cross(up, z));
    simd_float3 y = simd_cross(z, x);
    simd_float4x4 m = MIdent();
    m.columns[0] = (simd_float4){x.x, y.x, z.x, 0};
    m.columns[1] = (simd_float4){x.y, y.y, z.y, 0};
    m.columns[2] = (simd_float4){x.z, y.z, z.z, 0};
    m.columns[3] = (simd_float4){-simd_dot(x, eye), -simd_dot(y, eye), -simd_dot(z, eye), 1};
    return m;
}
static bool WorldToScreen(simd_float4x4 vp, float W, float H, float x, float y, float z, float &sx, float &sy);
static simd_float4x4 ToSimd(const Mat4 &m) {
    simd_float4x4 o;
    for (int c = 0; c < 4; ++c)
        o.columns[c] = (simd_float4){m.m[c * 4 + 0], m.m[c * 4 + 1], m.m[c * 4 + 2], m.m[c * 4 + 3]};
    return o;
}

static id<MTLTexture> MakeWhite(id<MTLDevice> d) {
    MTLTextureDescriptor *q = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                width:1 height:1 mipmapped:NO];
    id<MTLTexture> t = [d newTextureWithDescriptor:q];
    uint8_t p[4] = {255, 255, 255, 255};
    [t replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:p bytesPerRow:4];
    return t;
}

// Gameloft wraps PVRTC in a "BTEXpvr" container; unchanged from Milestone 2/3.
static id<MTLTexture> MakeRGBA8(id<MTLDevice> d, uint32_t w, uint32_t h, const std::vector<uint8_t> &rgba) {
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                  width:w height:h mipmapped:NO];
    id<MTLTexture> t = [d newTextureWithDescriptor:td];
    if (t) [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:rgba.data() bytesPerRow:w * 4];
    return t;
}

// Plain Truevision TGA (title/comic art ships this way): types 2/10, 24/32-bit.
static id<MTLTexture> LoadTGA(id<MTLDevice> d, const uint8_t *b, size_t n) {
    if (n < 18) return nil;
    uint8_t idLen = b[0], type = b[2], bpp = b[16], desc = b[17];
    uint32_t w = b[12] | (b[13] << 8), h = b[14] | (b[15] << 8);
    if (!w || !h || w > 4096 || h > 4096 || (bpp != 24 && bpp != 32) || (type != 2 && type != 10)) return nil;
    size_t p = 18 + idLen, bytes = bpp / 8;
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    size_t px = 0, total = (size_t)w * h;
    auto put = [&](const uint8_t *s) {
        rgba[px * 4 + 0] = s[2]; rgba[px * 4 + 1] = s[1]; rgba[px * 4 + 2] = s[0];
        rgba[px * 4 + 3] = bytes == 4 ? s[3] : 255; ++px;
    };
    if (type == 2) {
        if (p + total * bytes > n) return nil;
        for (; px < total;) put(b + p + px * bytes);
    } else {
        while (px < total && p < n) {
            uint8_t hdr = b[p++]; size_t cnt = (hdr & 0x7f) + 1;
            if (hdr & 0x80) { if (p + bytes > n) break; for (size_t k = 0; k < cnt && px < total; ++k) put(b + p); p += bytes; }
            else { for (size_t k = 0; k < cnt && px < total && p + bytes <= n; ++k) { put(b + p); p += bytes; } }
        }
    }
    if (!(desc & 0x20)) {   // bottom-left origin -> flip rows
        std::vector<uint8_t> f(rgba.size());
        for (uint32_t y = 0; y < h; ++y) memcpy(&f[(size_t)y * w * 4], &rgba[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
        rgba.swap(f);
    }
    return MakeRGBA8(d, w, h, rgba);
}

// BTEX container: PVRTC4 (square, uploaded compressed) or uncompressed
// 16/32-bit (RGBA4444 / RGBA5551 / RGB565 / RGBA8888 -> expanded to RGBA8).
static id<MTLTexture> LoadPVRTC(id<MTLDevice> d, NSString *path) {
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (!data || data.length < 60) return nil;
    const uint8_t *b = (const uint8_t *)data.bytes;
    if (memcmp(b, "BTEXpvr", 7) != 0) return LoadTGA(d, b, data.length);
    uint32_t hs = rdle32(b + 8), w = rdle32(b + 12), h = rdle32(b + 16);
    uint32_t flags = rdle32(b + 24), len = rdle32(b + 28), bpp = rdle32(b + 32);
    if (hs < 52 || !w || !h) return nil;
    size_t p = hs;
    if (p + 8 <= data.length && memcmp(b + p, "PVR!", 4) == 0) p += 8;
    if (p + len > data.length) return nil;
    uint32_t fmt = flags & 0xff;
    if (fmt == 0x19 && bpp == 4) {
        MTLPixelFormat f = (flags & 0x8000) ? MTLPixelFormatPVRTC_RGBA_4BPP : MTLPixelFormatPVRTC_RGB_4BPP;
        if (w != h) return nil;   // Metal PVRTC must be square
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f
                                                                                      width:w height:h mipmapped:NO];
        id<MTLTexture> t = [d newTextureWithDescriptor:td];
        if (t) [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:b + p bytesPerRow:0];
        return t;
    }
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    const uint8_t *src = b + p;
    size_t total = (size_t)w * h;
    if (fmt == 0x10 && bpp == 16) {            // OGL_RGBA_4444: R hi nibble ... A lo nibble
        if (total * 2 > len) return nil;
        for (size_t i = 0; i < total; ++i) {
            uint16_t v = (uint16_t)(src[i * 2] | (src[i * 2 + 1] << 8));
            rgba[i*4+0] = (v >> 12) * 17; rgba[i*4+1] = ((v >> 8) & 15) * 17;
            rgba[i*4+2] = ((v >> 4) & 15) * 17; rgba[i*4+3] = (v & 15) * 17;
        }
    } else if (fmt == 0x11 && bpp == 16) {     // OGL_RGBA_5551
        if (total * 2 > len) return nil;
        for (size_t i = 0; i < total; ++i) {
            uint16_t v = (uint16_t)(src[i * 2] | (src[i * 2 + 1] << 8));
            rgba[i*4+0] = (uint8_t)(((v >> 11) & 31) * 255 / 31); rgba[i*4+1] = (uint8_t)(((v >> 6) & 31) * 255 / 31);
            rgba[i*4+2] = (uint8_t)(((v >> 1) & 31) * 255 / 31); rgba[i*4+3] = (v & 1) ? 255 : 0;
        }
    } else if (fmt == 0x13 && bpp == 16) {     // OGL_RGB_565
        if (total * 2 > len) return nil;
        for (size_t i = 0; i < total; ++i) {
            uint16_t v = (uint16_t)(src[i * 2] | (src[i * 2 + 1] << 8));
            rgba[i*4+0] = (uint8_t)(((v >> 11) & 31) * 255 / 31); rgba[i*4+1] = (uint8_t)(((v >> 5) & 63) * 255 / 63);
            rgba[i*4+2] = (uint8_t)((v & 31) * 255 / 31); rgba[i*4+3] = 255;
        }
    } else if (fmt == 0x12 && bpp == 32) {     // OGL_RGBA_8888
        if (total * 4 > len) return nil;
        memcpy(rgba.data(), src, total * 4);
    } else {
        return nil;
    }
    return MakeRGBA8(d, w, h, rgba);
}

static const char *const kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct SkinV   { packed_float3 p; packed_float3 n; float2 uv; ushort4 b; packed_float4 w; };
struct StaticV { packed_float3 p; packed_float3 n; packed_float2 uv; packed_float2 uv2; uchar4 c; };  // fully packed: sizeof==44, matches CPU
struct U       { float4x4 vp; float4x4 model; float4 tint; float4 misc; };
struct Out     { float4 p [[position]]; float2 uv; float2 uv2; float l; float3 vc; };

static float shade(float3 n) {
    float3 L = normalize(float3(-0.35, -0.55, 0.75));
    return 0.34 + 0.66 * max(dot(normalize(n), L), 0.0);
}

vertex Out skinnedV(const device SkinV *a      [[buffer(0)]],
                    constant U &u              [[buffer(1)]],
                    const device float4x4 *bone[[buffer(2)]],
                    uint i                     [[vertex_id]]) {
    SkinV v = a[i];
    float4 w = float4(v.w);
    float4x4 s = bone[v.b.x] * w.x + bone[v.b.y] * w.y
               + bone[v.b.z] * w.z + bone[v.b.w] * w.w;
    float4 lp = s * float4(float3(v.p), 1.0);
    float3 ln = (s * float4(float3(v.n), 0.0)).xyz;
    Out o;
    o.p  = u.vp * (u.model * float4(lp.xyz, 1.0));
    o.uv = float2(v.uv.x, 1.0 - v.uv.y);
    o.l  = shade((u.model * float4(ln, 0.0)).xyz);
    o.uv2 = o.uv;
    o.vc = float3(1.0);
    return o;
}

vertex Out staticV(const device StaticV *a [[buffer(0)]],
                   constant U &u           [[buffer(1)]],
                   uint i                  [[vertex_id]]) {
    StaticV v = a[i];
    Out o;
    o.p  = u.vp * (u.model * float4(float3(v.p), 1.0));
    o.uv = float2(v.uv.x, 1.0 - v.uv.y);
    // misc.y blends between directional shading (0) and the baked vertex
    // lighting the level ships (1).
    o.l  = mix(shade((u.model * float4(float3(v.n), 0.0)).xyz), 1.0, u.misc.y);
    o.uv2 = float2(v.uv2.x, 1.0 - v.uv2.y);
    o.vc = float3(v.c.rgb) / 255.0;
    return o;
}

struct SpriteV { packed_float2 p; packed_float2 uv; uchar4 tint; };   // 20 B, fully packed
struct SpriteOut { float4 p [[position]]; float2 uv; float4 tint; };
vertex SpriteOut spriteV(const device SpriteV *a [[buffer(0)]],
                         constant float2 &vp     [[buffer(1)]],
                         uint i                  [[vertex_id]]) {
    SpriteV v = a[i];
    SpriteOut o;
    float2 ndc = float2(v.p) / vp * 2.0 - 1.0;
    o.p = float4(ndc.x, -ndc.y, 0.0, 1.0);           // top-left pixel origin
    o.uv = float2(v.uv);
    o.tint = float4(v.tint) / 255.0;
    return o;
}
fragment half4 spriteF(SpriteOut i             [[stage_in]],
                       texture2d<half> t       [[texture(0)]],
                       sampler s               [[sampler(0)]]) {
    half4 c = t.sample(s, i.uv);
    return c * half4(i.tint);
}

fragment half4 frag(Out i                   [[stage_in]],
                    texture2d<half> t       [[texture(0)]],
                    texture2d<half> lm      [[texture(1)]],
                    sampler s               [[sampler(0)]],
                    constant U &u           [[buffer(1)]]) {
    half4 tex = t.sample(s, (u.misc.x > 1.5) ? i.uv2 : i.uv);   // diffuse may declare UV set 1
    if (u.misc.w > 0.5 && tex.a < 0.5h) discard_fragment();     // alpha-tested foliage/fences
    half3 base = (u.misc.x > 0.5) ? tex.rgb : half3(u.tint.rgb);
    if (u.misc.z > 0.5) base *= lm.sample(s, i.uv2).rgb * 2.0h;  // original lightmap layer (M2)
    // baked vertex colour is a 2x modulate on textured level batches (mean 0.66 -> daylight);
    // characters (misc.y == 0) and lightmapped batches leave it alone
    half3 vcol = (u.misc.y > 0.5 && u.misc.z < 0.5) ? half3(i.vc) * 2.0h : half3(1.0h);
    return half4(base * vcol * half(i.l), half(u.tint.a));
}
)";

@interface TMRenderer () {
    id<MTLDevice> _device;
    id<MTLCommandQueue> _queue;
    id<MTLRenderPipelineState> _skinPipe;
    id<MTLRenderPipelineState> _staticPipe;
    id<MTLDepthStencilState> _depth;
    id<MTLSamplerState> _sampler;

    id<MTLBuffer> _heroVB;
    id<MTLBuffer> _heroIB;
    id<MTLBuffer> _boneBuf;
    id<MTLTexture> _heroTex;
    id<MTLTexture> _white;
    NSUInteger _heroIndexCount;

    NSMutableArray<id<MTLBuffer>> *_levelVBs;
    NSMutableArray<id<MTLBuffer>> *_levelIBs;
    std::vector<NSUInteger> _levelCounts;
    NSMutableArray<id<MTLTexture>> *_levelTex;     // diffuse per batch (_white if unresolved)
    NSMutableArray<id<MTLTexture>> *_levelLM;      // lightmap per batch (_white if none)
    std::vector<int> _levelFlags;                  // bit0 hasTex, bit1 hasLM, bit2 alphaTest
    NSMutableDictionary<NSString *, id<MTLTexture>> *_texCache;
    NSMutableDictionary<NSString *, NSString *> *_texIndex;   // lowercase file -> path

    // Enemy archetypes (thug variants) + placed instances from the original level.
    std::vector<std::unique_ptr<bdae::Model>> _npcModel;
    NSMutableArray<id<MTLBuffer>> *_npcVBs;
    NSMutableArray<id<MTLBuffer>> *_npcIBs;
    NSMutableArray<id<MTLTexture>> *_npcTexs;
    std::vector<NSUInteger> _npcIndexCount;
    std::vector<const bdae::Clip *> _npcIdle;
    std::vector<bdae::Vec3> _npcAnchor;
    struct NPCInst { int type; float x, y, z, yaw, phase; };
    std::vector<NPCInst> _npcs;
    std::vector<bdae::EnemyActor> _foes;      // combat sim, aligned with _npcs
    bdae::HeroCombat _fists;
    float _heroHP;

    // Milestone 7: original HUD sprites
    bdae::BSprite _ui;
    id<MTLTexture> _uiAtlas;
    id<MTLRenderPipelineState> _spritePipe;
    id<MTLDepthStencilState> _noDepth;
    id<MTLBuffer> _spriteVB;
    int _modStickBase, _modStickPuck, _modButton, _modBarFrame;
    BOOL _showAtlasSheet, _paused;

    // Milestone 8: level flow
    bdae::GameFlow _flow;
    bdae::StringTable _strings;
    std::string _assetRootStr;
    id<MTLTexture> _paperTex;
    id<MTLTexture> _fontAtlas;                 // font_outline_big.tga: the yellow outlined UI font
    struct Popup { float x, y, z; int value; uint32_t bornMs; };
    std::vector<Popup> _popups;
    int _comboHits; uint32_t _comboLastMs;
    float _webEnergy;
    UILabel *_flowLabel;
    UILabel *_skipLabel;
    MTKView *_mtkView;
    AVPlayer *_video;
    AVPlayerLayer *_videoLayer;
    NSMutableArray<NSString *> *_bootVideos;
    NSUInteger _videoIndex;
    BOOL _bootPlayed;
    float _fpsEma;
    id<MTLTexture> _comicTex;
    int _comicLoadedPage;
    NSMutableArray<id<MTLBuffer>> *_skyVBs, *_skyIBs;
    NSMutableArray<id<MTLTexture>> *_skyTex;
    std::vector<NSUInteger> _skyCounts;

    // Milestone 12: world props, bonuses, sense, score
    NSMutableArray<id<MTLBuffer>> *_propVBs, *_propIBs;
    NSMutableArray<id<MTLTexture>> *_propTex;
    std::vector<NSUInteger> _propCounts;
    std::vector<int> _propBatchArch;           // archetype index per GPU batch
    std::vector<int> _propAlphaTest;
    struct PropInst { int arch; simd_float4x4 model; float x, y, z; bool alive; bool destructible; };
    std::vector<PropInst> _props;
    std::vector<bool> _bonusTaken;
    std::vector<std::pair<int, int>> _propRanges;
    int _score;
    simd_float4x4 _lastVP;
    float _lastW, _lastH;
    id<MTLBuffer> _npcBones;

    std::unique_ptr<Model> _hero;
    std::unique_ptr<LevelRoom> _room;
    std::unique_ptr<Character> _actor;

    UILabel *_label;
    CFTimeInterval _last;
    float _camYaw, _camPitch, _camDist;
    float _stickX, _stickY;
    float _moveOriginX, _moveOriginY;
    float _lookLastX, _lookLastY;
    NSInteger _moveTouchActive, _lookTouchActive;
    NSUInteger _frameIdx;
    dispatch_semaphore_t _inFlight;
    CFTimeInterval _moveDownT;
    float _moveMaxLen;
    BOOL _heroReady, _levelReady;
}
@end

@implementation TMRenderer

- (instancetype)initWithView:(MTKView *)view statusLabel:(UILabel *)label {
    self = [super init];
    if (!self) return nil;

    _device = MTLCreateSystemDefaultDevice();
    _queue = [_device newCommandQueue];
    _label = label;

    view.device = _device;
    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    view.preferredFramesPerSecond = 60;
    view.delegate = self;

    NSError *error = nil;
    id<MTLLibrary> lib = [_device newLibraryWithSource:[NSString stringWithUTF8String:kShaderSource] options:nil error:&error];
    if (!lib) NSLog(@"[TotalMayhem] shader compile failed: %@", error);

    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [lib newFunctionWithName:@"skinnedV"];
    pd.fragmentFunction = [lib newFunctionWithName:@"frag"];
    pd.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    pd.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
    _skinPipe = [_device newRenderPipelineStateWithDescriptor:pd error:&error];
    if (!_skinPipe) NSLog(@"[TotalMayhem] skinned pipeline failed: %@", error);

    MTLRenderPipelineDescriptor *sd = [MTLRenderPipelineDescriptor new];
    sd.vertexFunction = [lib newFunctionWithName:@"staticV"];
    sd.fragmentFunction = [lib newFunctionWithName:@"frag"];
    sd.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    sd.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
    _staticPipe = [_device newRenderPipelineStateWithDescriptor:sd error:&error];

    MTLRenderPipelineDescriptor *ud = [MTLRenderPipelineDescriptor new];
    ud.vertexFunction = [lib newFunctionWithName:@"spriteV"];
    ud.fragmentFunction = [lib newFunctionWithName:@"spriteF"];
    ud.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    ud.colorAttachments[0].blendingEnabled = YES;
    ud.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    ud.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    ud.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    ud.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    ud.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
    _spritePipe = [_device newRenderPipelineStateWithDescriptor:ud error:&error];
    MTLDepthStencilDescriptor *nd = [MTLDepthStencilDescriptor new];
    nd.depthCompareFunction = MTLCompareFunctionAlways;
    nd.depthWriteEnabled = NO;
    _noDepth = [_device newDepthStencilStateWithDescriptor:nd];
    _spriteVB = [_device newBufferWithLength:kFramesInFlight * 512 * 6 * 20
                                     options:MTLResourceStorageModeShared];
    if (!_staticPipe) NSLog(@"[TotalMayhem] static pipeline failed: %@", error);

    MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
    dd.depthCompareFunction = MTLCompareFunctionLess;
    dd.depthWriteEnabled = YES;
    _depth = [_device newDepthStencilStateWithDescriptor:dd];

    MTLSamplerDescriptor *smp = [MTLSamplerDescriptor new];
    smp.minFilter = MTLSamplerMinMagFilterLinear;
    smp.magFilter = MTLSamplerMinMagFilterLinear;
    _sampler = [_device newSamplerStateWithDescriptor:smp];
    _white = MakeWhite(_device);

    NSString *root = NSBundle.mainBundle.resourcePath;

    std::string assetRoot = std::string(root.UTF8String) + "/Assets";
    std::string err, heroErr, animErr, levelErr;

    _hero = std::make_unique<Model>();
    _heroReady = _hero->loadMesh(assetRoot + "/entities/meshes_bin/spiderman_mesh.bdae", heroErr);
    BOOL animReady = NO;
    if (_heroReady) animReady = _hero->loadAnimation(assetRoot + "/entities/meshes_bin/spiderman_anim.bdae", animErr);

    _assetRootStr = assetRoot;
    {
        std::string se;
        if (!_strings.load(assetRoot + "/xlsStrings/MAIN.map",
                           assetRoot + "/xlsStrings/MAIN_EN.data", se))
            NSLog(@"[TotalMayhem] strings: %s", se.c_str());
    }
    [self loadLevelIndex:0];

    if (_heroReady) {
        const Mesh &m = _hero->meshes.front();
        std::vector<GPUSkinVertex> verts(m.vertices.size());
        for (size_t i = 0; i < m.vertices.size(); ++i) {
            const Vertex &s = m.vertices[i];
            GPUSkinVertex &d = verts[i];
            d.p[0] = s.px; d.p[1] = s.py; d.p[2] = s.pz;
            d.n[0] = s.nx; d.n[1] = s.ny; d.n[2] = s.nz;
            d.uv[0] = s.u; d.uv[1] = s.v;
            for (int k = 0; k < 4; ++k) { d.bone[k] = s.bone[k]; d.w[k] = s.weight[k]; }
        }
        _heroVB = [_device newBufferWithBytes:verts.data()
                                       length:verts.size() * sizeof(GPUSkinVertex)
                                      options:MTLResourceStorageModeShared];
        _heroIB = [_device newBufferWithBytes:m.indices.data()
                                       length:m.indices.size() * sizeof(uint16_t)
                                      options:MTLResourceStorageModeShared];
        _heroIndexCount = m.indices.size();
        _boneBuf = [_device newBufferWithLength:kFramesInFlight * kBoneSlot
                                        options:MTLResourceStorageModeShared];
        if (!_hero->textureNames.empty()) {
            NSString *tp = [root stringByAppendingPathComponent:
                            [NSString stringWithFormat:@"Assets/entities/textures_bin/%s",
                             _hero->textureNames.front().c_str()]];
            _heroTex = LoadPVRTC(_device, tp);
        }
    }



    // Original HUD atlas (sprites.pack). Missing assets degrade gracefully.
    _modStickBase = _modStickPuck = _modButton = _modBarFrame = -1;
    {
        std::string uerr;
        if (_ui.load(assetRoot + "/sprites/interface.bsprite", uerr)) {
            NSString *ap = [NSString stringWithFormat:@"%s/sprites/interface.tga", assetRoot.c_str()];
            _uiAtlas = LoadPVRTC(_device, ap);
            _fontAtlas = LoadPVRTC(_device, [NSString stringWithFormat:@"%s/sprites/font_outline_big.tga", assetRoot.c_str()]);
            NSLog(@"[TotalMayhem] HUD atlas %s, font atlas %s", _uiAtlas ? "ok" : "FAILED", _fontAtlas ? "ok" : "FAILED");
            // Best-guess module roles by shape until visually mapped:
            int bestSq = -1, bestPuck = -1, bestBtn = -1, bestBar = -1;
            for (int i = 0; i < (int)_ui.modules.size(); ++i) {
                const bdae::SpriteModule &m = _ui.modules[i];
                int dw = abs((int)m.w - (int)m.h);
                if (dw < 10 && m.w >= 110 && (bestSq < 0 || m.w > _ui.modules[bestSq].w)) bestSq = i;
                if (dw < 8 && m.w >= 36 && m.w <= 60 && bestPuck < 0) bestPuck = i;
                if (dw < 8 && m.w >= 44 && m.w <= 72 && bestBtn < 0) bestBtn = i;
                if (m.w >= 3 * m.h && m.w >= 60 && bestBar < 0) bestBar = i;
            }
            _modStickBase = bestSq; _modStickPuck = bestPuck;
            _modButton = bestBtn; _modBarFrame = bestBar;
            NSLog(@"[TotalMayhem] HUD atlas: %zu modules; guesses base=%d puck=%d button=%d bar=%d",
                  _ui.modules.size(), bestSq, bestPuck, bestBtn, bestBar);
        } else {
            NSLog(@"[TotalMayhem] HUD sprites not found (%s) - run the sprites extraction step", uerr.c_str());
        }
        NSString *pp = [NSString stringWithFormat:@"%s/sprites/paper_title9.tga", assetRoot.c_str()];
        _paperTex = LoadPVRTC(_device, pp);
    }
    _mtkView = view;
    _label.hidden = !TM_DEBUG_HUD;
    _flowLabel = [[UILabel alloc] initWithFrame:view.bounds];
    _flowLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _flowLabel.textAlignment = NSTextAlignmentCenter;
    _flowLabel.numberOfLines = 0;
    _flowLabel.textColor = UIColor.whiteColor;
    _flowLabel.font = [UIFont boldSystemFontOfSize:30];
    _flowLabel.shadowColor = UIColor.blackColor;
    _flowLabel.shadowOffset = CGSizeMake(0, 2);
    [view addSubview:_flowLabel];
    _skipLabel = [[UILabel alloc] initWithFrame:CGRectMake(view.bounds.size.width - 150, 24, 130, 40)];
    _skipLabel.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleBottomMargin;
    _skipLabel.textAlignment = NSTextAlignmentRight;
    _skipLabel.textColor = [UIColor colorWithRed:1.0 green:0.78 blue:0.2 alpha:1.0];
    _skipLabel.font = [UIFont italicSystemFontOfSize:26];
    _skipLabel.text = @"SKIP";
    _skipLabel.hidden = YES;
    [view addSubview:_skipLabel];
    [self startBootVideos];

    _actor = std::make_unique<Character>();
    if (_heroReady && animReady) {
        _actor->bind(_hero.get(), _levelReady ? _room.get() : nullptr);
        if (_levelReady && _room->hasSpawn) _actor->spawnAt(_room->spawn, _room->spawnYaw);
    }

    _camYaw = 0.6f;
    _camPitch = 0.30f;
    _camDist = 430.0f;
    _moveTouchActive = -1;
    _heroHP = 100.0f;
    _frameIdx = 0;
    _inFlight = dispatch_semaphore_create(kFramesInFlight);
    _lookTouchActive = -1;
    _last = CACurrentMediaTime();

    NSString *heroLine = _heroReady
        ? [NSString stringWithFormat:@"Spider-Man %zu verts / %zu tris / %zu bones / %zu clips",
           _hero->meshes.front().vertices.size(), _hero->meshes.front().indices.size() / 3,
           _hero->skin.jointNode.size(), _hero->clips.size()]
        : [NSString stringWithFormat:@"Spider-Man FAILED: %s", heroErr.c_str()];
    NSString *levelLine = _levelReady
        ? [NSString stringWithFormat:@"Level 1 (all rooms)  %zu tris  nav %zu tris  %zu enemies",
           _room->visualTriangleCount(), _room->navmesh.indices.size() / 3, _npcs.size()]
        : @"Level FAILED (see log)";
    _label.numberOfLines = 0;
    _label.text = [NSString stringWithFormat:@"%@\n%@\nLEFT drag = move  RIGHT drag = camera", heroLine, levelLine];

    return self;
}

- (void)drawInMTKView:(MTKView *)view {
    CFTimeInterval now = CACurrentMediaTime();
    float dt = (float)fmin(0.05, now - _last);
    _last = now;

    // Stick input is camera-relative: +Y on the stick walks away from the camera.
    // Camera forward (eye->target) is (+sin(yaw), +cos(yaw)).
    float fwdX = sinf(_camYaw), fwdY = cosf(_camYaw);
    float rightX = cosf(_camYaw), rightY = -sinf(_camYaw);
    float moveX = rightX * _stickX + fwdX * _stickY;
    float moveY = rightY * _stickX + fwdY * _stickY;
    if (dt > 0.0001f) _fpsEma = _fpsEma > 0 ? (_fpsEma * 0.9f + (1.0f / dt) * 0.1f) : (1.0f / dt);
    if (_flow.phase == bdae::GameFlow::COMIC) {
        uint32_t cms = (uint32_t)(now * 1000.0);
        if (cms - _flow.comicPageStartMs > 5200) _flow.advanceComic(cms);
    }
    BOOL playing = (_flow.phase == bdae::GameFlow::PLAYING) && !_paused;
    if (!playing) dt = 0;
    if (_actor && playing) _actor->update(dt, moveX, moveY);

    // -------- combat simulation (Milestone 6) --------
    uint32_t nowMs = (uint32_t)(now * 1000.0);
    uint32_t dtMs = (uint32_t)(dt * 1000.0f);
    Vec3 heroPos = _actor ? _actor->position() : Vec3{0, 0, 0};
    for (bdae::EnemyActor &f : _foes)
        if (playing && f.update(nowMs, dtMs, heroPos) && _heroHP > 0)
            _heroHP = fmaxf(0.0f, _heroHP - f.stats.damage);
    if (playing) _webEnergy = fminf(100.0f, _webEnergy + dt * 8.0f);
    if (playing && _flow.takeCheckpointReached()) {
        _heroHP = 100.0f;   // the original restores health at checkpoints (assumption, documented)
        _popups.push_back({heroPos.x, heroPos.y, heroPos.z + 220.0f, 0, nowMs});   // 0 = CHECKPOINT
    }
    if (playing) {
        int hits = _fists.update(nowMs, heroPos, _actor ? _actor->yaw() : 0.0f, _foes);
        _score += hits * 10;
        if (hits > 0) {
            _popups.push_back({heroPos.x, heroPos.y, heroPos.z + 190.0f, hits * 10, nowMs});
            _comboHits = (nowMs - _comboLastMs < 1500) ? _comboHits + 1 : 1;
            _comboLastMs = nowMs;
        }
        if (_fists.justStruck && _actor) {
            float fx = cosf(_actor->yaw()), fy = sinf(_actor->yaw());
            for (PropInst &p : _props) {
                if (!p.alive || !p.destructible) continue;
                float dx = p.x - heroPos.x, dy = p.y - heroPos.y, d = sqrtf(dx * dx + dy * dy);
                if (d < 240.0f && (d < 1.0f || (dx * fx + dy * fy) / d > 0.2f)) {
                    p.alive = false; _score += 50; _popups.push_back({p.x, p.y, p.z + 120.0f, 50, nowMs});
                }
            }
        }
        for (size_t i = 0; i < _bonusTaken.size(); ++i) {
            if (_bonusTaken[i]) continue;
            const Vec3 &b = _room->bonuses[i];
            float dx = b.x - heroPos.x, dy = b.y - heroPos.y;
            if (dx * dx + dy * dy < 150.0f * 150.0f && fabsf(b.z - heroPos.z) < 300.0f) {
                _bonusTaken[i] = true; _score += 25; _popups.push_back({b.x, b.y, b.z + 100.0f, 25, nowMs});
            }
        }
        if (_heroHP <= 0) _flow.onDeath(nowMs);
        else _flow.updatePlaying(heroPos, nowMs);
    }
    if (_heroReady) {
        const Clip *oc; uint32_t otl;
        if (_fists.poseInfo(nowMs, oc, otl)) _hero->poseAtTime(otl);   // punch overrides locomotion
    }
    if (_frameIdx % 30 == 0 && _levelReady) {
        int alive = 0; for (auto &f : _foes) if (f.alive()) ++alive;
        _label.text = [NSString stringWithFormat:
            @"HP %.0f   score %d   enemies %d/%zu   checkpoints %d/%zu   %.0f fps%@\nLEFT drag = move  tap = punch  RIGHT drag = camera",
            _heroHP, _score, alive, _foes.size(), _flow.visitedCount(), _flow.checkpointsAll.size(),
            _fpsEma, _paused ? @"  PAUSED" : @""];
        NSString *gn = [NSString stringWithUTF8String:
            _strings.get("STR_GAME_NAME", "SPIDER-MAN: TOTAL MAYHEM").c_str()];
        NSString *lvName = [NSString stringWithUTF8String:
            _strings.get("STR_LEVELNEW_" + std::to_string(_flow.levelIndex + 1) + "_NAME",
                         "LEVEL " + std::to_string(_flow.levelIndex + 1)).c_str()];
        _skipLabel.hidden = _fontAtlas || !(_flow.phase == bdae::GameFlow::COMIC || _flow.phase == bdae::GameFlow::VIDEO);
        _flowLabel.hidden = (_fontAtlas != nil);
        switch (_flow.phase) {
            case bdae::GameFlow::VIDEO:
            case bdae::GameFlow::COMIC:
                _flowLabel.text = @""; break;
            case bdae::GameFlow::TITLE:
                _flowLabel.text = [NSString stringWithFormat:@"%@\n\n%@\n\nTap to start", gn, lvName]; break;
            case bdae::GameFlow::DEAD:
                _flowLabel.text = @"SPIDER-MAN IS DOWN\n\nTap to retry from the last checkpoint"; break;
            case bdae::GameFlow::COMPLETE:
                _flowLabel.text = [NSString stringWithFormat:@"LEVEL %d COMPLETE\n\nTap to continue",
                                   _flow.levelIndex + 1]; break;
            default: _flowLabel.text = @""; break;
        }
    }

    id<CAMetalDrawable> drawable = view.currentDrawable;
    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    if (!drawable || !pass) return;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.07, 0.09, 0.13, 1.0);
    pass.depthAttachment.clearDepth = 1.0;

    dispatch_semaphore_wait(_inFlight, DISPATCH_TIME_FOREVER);   // bone buffers ahead
    id<MTLCommandBuffer> cb = [_queue commandBuffer];
    __block dispatch_semaphore_t doneSem = _inFlight;
    [cb addCompletedHandler:^(id<MTLCommandBuffer> b) { dispatch_semaphore_signal(doneSem); }];
    id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:pass];
    [enc setDepthStencilState:_depth];
    [enc setCullMode:MTLCullModeNone];
    [enc setFragmentSamplerState:_sampler atIndex:0];

    Vec3 p = _actor ? _actor->position() : Vec3{0, 0, 0};
    float aspect = (float)view.drawableSize.width / fmaxf(1.0f, (float)view.drawableSize.height);
    simd_float3 target = (simd_float3){p.x, p.y, p.z + 95.0f};
    simd_float3 eye = (simd_float3){
        p.x - sinf(_camYaw) * _camDist * cosf(_camPitch),
        p.y - cosf(_camYaw) * _camDist * cosf(_camPitch),
        p.z + 95.0f + sinf(_camPitch) * _camDist};
    simd_float4x4 vp = simd_mul(MPerspective(58.0f * (float)M_PI / 180.0f, aspect, 10.0f, 120000.0f),
                                MLookAt(eye, target, (simd_float3){0, 0, 1}));

    if (_skyCounts.size()) {
        Uniforms su;
        su.vp = vp;
        su.model = MTranslate(eye.x, eye.y, eye.z - 300.0f);
        su.tint = (simd_float4){1, 1, 1, 1};
        [enc setRenderPipelineState:_staticPipe];
        [enc setDepthStencilState:_noDepth];
        for (NSUInteger b = 0; b < _skyCounts.size(); ++b) {
            su.misc = (simd_float4){_skyTex[b] != _white ? 1.0f : 0.0f, 0, 0, 0};
            [enc setVertexBytes:&su length:sizeof(su) atIndex:1];
            [enc setFragmentBytes:&su length:sizeof(su) atIndex:1];
            [enc setFragmentTexture:_skyTex[b] atIndex:0];
            [enc setFragmentTexture:_white atIndex:1];
            [enc setVertexBuffer:_skyVBs[b] offset:0 atIndex:0];
            [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:_skyCounts[b]
                             indexType:MTLIndexTypeUInt16 indexBuffer:_skyIBs[b] indexBufferOffset:0];
        }
        [enc setDepthStencilState:_depth];
    }

    if (_levelReady && _levelCounts.size()) {
        Uniforms u;
        u.vp = vp;
        u.model = MIdent();
        u.tint = (simd_float4){0.92f, 0.94f, 1.0f, 1.0f};
        [enc setRenderPipelineState:_staticPipe];
        for (NSUInteger b = 0; b < _levelCounts.size(); ++b) {
            int fl = (b < _levelFlags.size()) ? _levelFlags[b] : 0;
            // misc = {hasTexture, bake blend, hasLightmap, alphaTest}
            // misc.x: 0 none, 1 texture on uv0, 2 texture on uv1
            u.misc = (simd_float4){(fl & 1) ? ((fl & 8) ? 2.0f : 1.0f) : 0.0f, 0.75f, (fl & 2) ? 1.0f : 0.0f, (fl & 4) ? 1.0f : 0.0f};
            [enc setVertexBytes:&u length:sizeof(u) atIndex:1];
            [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
            [enc setFragmentTexture:(b < _levelTex.count ? _levelTex[b] : _white) atIndex:0];
            [enc setFragmentTexture:(b < _levelLM.count ? _levelLM[b] : _white) atIndex:1];
            [enc setVertexBuffer:_levelVBs[b] offset:0 atIndex:0];
            [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:_levelCounts[b]
                             indexType:MTLIndexTypeUInt16
                           indexBuffer:_levelIBs[b]
                     indexBufferOffset:0];
        }
    }

    [self drawProps:enc vp:vp eye:eye];
    _lastVP = vp; _lastW = (float)view.drawableSize.width; _lastH = (float)view.drawableSize.height;
    [self drawEnemies:enc vp:vp time:now];

    if (_heroReady && _heroIndexCount) {
        const std::vector<Mat4> &sk = _hero->skinningMatrices();
        NSUInteger boneOff = (_frameIdx % kFramesInFlight) * kBoneSlot;
        simd_float4x4 *dst = (simd_float4x4 *)((uint8_t *)_boneBuf.contents + boneOff);
        NSUInteger n = MIN((NSUInteger)sk.size(), kMaxBones);
        for (NSUInteger i = 0; i < kMaxBones; ++i) dst[i] = (i < n) ? ToSimd(sk[i]) : MIdent();

        Uniforms u;
        u.vp = vp;
        u.model = simd_mul(MTranslate(p.x, p.y, p.z),
                           MRotZ((_actor ? _actor->yaw() : 0.0f) + kModelYawOffset));
        u.tint = (simd_float4){1, 1, 1, 1};
        u.misc = (simd_float4){_heroTex ? 1.0f : 0.0f, 0, 0, 0};
        [enc setRenderPipelineState:_skinPipe];
        [enc setVertexBuffer:_heroVB offset:0 atIndex:0];
        [enc setVertexBytes:&u length:sizeof(u) atIndex:1];
        [enc setVertexBuffer:_boneBuf offset:boneOff atIndex:2];
        [enc setFragmentTexture:(_heroTex ? _heroTex : _white) atIndex:0];
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
        [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:_heroIndexCount
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:_heroIB
                 indexBufferOffset:0];
    }

    [self drawHUD:enc view:view];

    [enc endEncoding];
    [cb presentDrawable:drawable];
    [cb commit];
    ++_frameIdx;
}

// -------------------------------------------------------------------- HUD ---
struct SpriteVert { float p[2]; float uv[2]; uint8_t tint[4]; };

- (void)drawHUD:(id<MTLRenderCommandEncoder>)enc view:(MTKView *)view {
    if (!_spritePipe) return;
    float W = (float)view.drawableSize.width, H = (float)view.drawableSize.height;
    float sc = H / 768.0f;   // original layout space: 768 px tall

    // ---- authentic sprites, rectangles verified visually on the decoded interface.tga
    static const bdae::SpriteModule kPause{478, 194, 30, 36}, kPortrait{467, 65, 41, 62},
        kHpFrame{279, 446, 139, 18}, kHpFill{92, 476, 133, 16}, kWebMeter{0, 270, 212, 32},
        kKnob{177, 212, 50, 52}, kRing{345, 4, 58, 58}, kBtn{59, 212, 57, 52},
        kFist{355, 303, 38, 40}, kFistHot{395, 303, 35, 40}, kDodge{471, 303, 30, 36}, kWeb{230, 302, 38, 40},
        kToken{262, 212, 52, 52}, kFull{0, 0, 4, 4};
    // font_outline_big.tga digits (row y=34, h=24) and '+'
    static const uint16_t kDigX[10] = {20, 44, 60, 81, 104, 127, 148, 170, 192, 214};
    static const uint16_t kDigW[10] = {18, 11, 18, 18, 18, 18, 16, 18, 18, 17};

    std::vector<SpriteVert> verts; verts.reserve(6 * 256);
    struct Seg { NSUInteger start, count; id<MTLTexture> tex; };
    std::vector<Seg> segs;
    auto useTex = [&](id<MTLTexture> t) {
        if (!segs.empty() && segs.back().tex == t) return;
        if (!segs.empty()) segs.back().count = verts.size() - segs.back().start;
        segs.push_back({verts.size(), 0, t});
    };
    auto quad = [&](float x, float y, float w, float h, const bdae::SpriteModule &m,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        float u0 = m.x / 512.0f, v0 = m.y / 512.0f, u1 = (m.x + m.w) / 512.0f, v1 = (m.y + m.h) / 512.0f;
        SpriteVert q[6] = {{{x, y}, {u0, v0}, {r, g, b, a}}, {{x + w, y}, {u1, v0}, {r, g, b, a}},
                           {{x + w, y + h}, {u1, v1}, {r, g, b, a}}, {{x, y}, {u0, v0}, {r, g, b, a}},
                           {{x + w, y + h}, {u1, v1}, {r, g, b, a}}, {{x, y + h}, {u0, v1}, {r, g, b, a}}};
        verts.insert(verts.end(), q, q + 6);
    };
    auto circle = [&](float cx, float cy, float rad, const bdae::SpriteModule &m, uint8_t a) {
        quad(cx - rad, cy - rad, 2 * rad, 2 * rad, m, 255, 255, 255, a);
    };
    uint32_t nowMs = (uint32_t)(CACurrentMediaTime() * 1000.0);
    // text in the original outlined font; align 0 = left, 1 = centre, 2 = right
    auto glyph = [&](char c) -> const FontGlyph * {
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        for (int i = 0; i < kFontCount; ++i) if (kFont[i].c == c) return &kFont[i];
        return nullptr;
    };
    auto textWidth = [&](const char *t, float h) {
        float w = 0, gs = h / 29.0f;
        for (const char *c = t; *c; ++c) { auto g = glyph(*c); w += (g ? g->w * gs : 9 * gs) + 2 * sc; }
        return w;
    };
    auto drawText = [&](const char *t, float x, float y, float h, int align, uint8_t a) {
        if (!_fontAtlas) return;
        useTex(_fontAtlas);
        float gs = h / 29.0f, w = textWidth(t, h);
        if (align == 1) x -= w * 0.5f; else if (align == 2) x -= w;
        for (const char *c = t; *c; ++c) {
            auto g = glyph(*c);
            if (g) { quad(x, y + (29 - g->h) * gs * 0.5f, g->w * gs, g->h * gs, bdae::SpriteModule{g->x, g->y, g->w, g->h}, 255, 255, 255, a); x += g->w * gs + 2 * sc; }
            else x += 9 * gs + 2 * sc;
        }
    };

    // ---- flow overlays (video/title/death/complete) and the comic page
    if (_flow.phase != bdae::GameFlow::PLAYING) {
        useTex(_white);
        quad(0, 0, W, H, kFull, 6, 6, 10, _flow.phase == bdae::GameFlow::COMIC ? 255 : 225);
        if (_flow.phase == bdae::GameFlow::COMIC) {
            id<MTLTexture> page = [self comicPage:_flow.comicFirst + _flow.comicIndex];
            if (page) {
                float t = fminf(1.0f, (float)(nowMs - _flow.comicPageStartMs) / 4500.0f);
                float ph = H * 0.94f * (1.04f + 0.14f * t), pw = ph;
                useTex(page);
                quad((W - pw) * 0.5f, (H - ph) * 0.5f, pw, ph, bdae::SpriteModule{0, 0, 512, 512}, 255, 255, 255, 255);
            }
        }
        if (_fontAtlas) {
            std::string lvName = _strings.get("STR_LEVELNEW_" + std::to_string(_flow.levelIndex + 1) + "_NAME",
                                              "LEVEL " + std::to_string(_flow.levelIndex + 1));
            for (char &c : lvName) if (c == '|') c = '\n';
            float pulse = 0.75f + 0.25f * sinf((float)CACurrentMediaTime() * 4.0f);
            switch (_flow.phase) {
                case bdae::GameFlow::VIDEO:
                case bdae::GameFlow::COMIC:
                    drawText("SKIP", W - 30 * sc, 24 * sc, 44 * sc, 2, 255); break;
                case bdae::GameFlow::TITLE: {
                    float y = H * 0.36f;
                    size_t p = 0;
                    while (p <= lvName.size()) {
                        size_t nl = lvName.find('\n', p); if (nl == std::string::npos) nl = lvName.size();
                        std::string line = lvName.substr(p, nl - p);
                        if (!line.empty()) { drawText(line.c_str(), W * 0.5f, y, 72 * sc, 1, 255); y += 84 * sc; }
                        p = nl + 1;
                    }
                    drawText("TAP TO START", W * 0.5f, H * 0.78f, 40 * sc, 1, (uint8_t)(255 * pulse));
                    break;
                }
                case bdae::GameFlow::DEAD:
                    drawText("SPIDER-MAN IS DOWN", W * 0.5f, H * 0.40f, 64 * sc, 1, 255);
                    drawText("TAP TO RETRY", W * 0.5f, H * 0.62f, 40 * sc, 1, (uint8_t)(255 * pulse));
                    break;
                case bdae::GameFlow::COMPLETE:
                    drawText("LEVEL COMPLETE!", W * 0.5f, H * 0.40f, 72 * sc, 1, 255);
                    drawText("TAP TO CONTINUE", W * 0.5f, H * 0.62f, 40 * sc, 1, (uint8_t)(255 * pulse));
                    break;
                default: break;
            }
        }
    }

    if (_uiAtlas && (_flow.phase == bdae::GameFlow::PLAYING || _paused)) {
        useTex(_uiAtlas);
        // top-left cluster: pause bubble, portrait, health frame + fill, web meter
        quad(11 * sc, 16 * sc, 48 * sc, 56 * sc, kPause, 255, 255, 255, 255);
        quad(74 * sc, 20 * sc, 80 * sc, 120 * sc, kPortrait, 255, 255, 255, 255);
        quad(170 * sc, 34 * sc, 404 * sc, 40 * sc, kHpFrame, 255, 255, 255, 255);
        quad(178 * sc, 40 * sc, 386 * sc * fmaxf(0.0f, _heroHP / 100.0f), 28 * sc, kHpFill, 255, 255, 255, 255);
        quad(172 * sc, 80 * sc, 356 * sc * fmaxf(0.06f, _webEnergy / 100.0f), 34 * sc, kWebMeter, 255, 255, 255, 235);
        // joystick: fixed home like the original, knob follows the stick
        float jx = 175 * sc, jy = H - 135 * sc, R = 92 * sc;
        circle(jx, jy, R, kRing, 200);
        circle(jx + _stickX * R * 0.75f, jy - _stickY * R * 0.75f, 64 * sc, kKnob, 255);
        // action buttons: fist, dodge, web (glyphs are separate white sprites)
        float bx1 = W - 266 * sc, by1 = 511 * sc, bx2 = W - 96 * sc, bx3 = W - 319 * sc, by3 = 665 * sc, br = 70 * sc;
        circle(bx1, by1, br, kBtn, 255);
        circle(bx2, by1, br, kBtn, 255);
        circle(bx3, by3, br, kBtn, 255);
        bool punching = _fists.punching(nowMs);
        const bdae::SpriteModule &fist = punching ? kFistHot : kFist;
        quad(bx1 - 32 * sc, by1 - 34 * sc, 64 * sc, 68 * sc, fist, 255, 255, 255, 255);
        quad(bx2 - 26 * sc, by1 - 32 * sc, 52 * sc, 64 * sc, kDodge, 255, 255, 255, 255);
        quad(bx3 - 32 * sc, by3 - 34 * sc, 64 * sc, 68 * sc, kWeb, 255, 255, 255, 255);
        // collectible tokens at the original Bonus positions
        for (size_t i = 0; i < _bonusTaken.size(); ++i) {
            if (_bonusTaken[i]) continue;
            const Vec3 &b = _room->bonuses[i]; float sx, sy;
            if (!WorldToScreen(_lastVP, W, H, b.x, b.y, b.z + 70.0f + 12.0f * sinf((float)CACurrentMediaTime() * 3.0f + i), sx, sy)) continue;
            circle(sx, sy, 22 * sc, kToken, 240);
        }
        // spider-sense: the ticked ring, red, pulsing over enemies that have noticed you
        float pulse = 0.5f + 0.5f * sinf((float)CACurrentMediaTime() * 6.0f);
        Vec3 hpos = _actor ? _actor->position() : Vec3{};
        for (const bdae::EnemyActor &f : _foes) {
            if (!f.alive() || f.state == bdae::EnemyActor::IDLE) continue;
            float dd = sqrtf((f.x - hpos.x) * (f.x - hpos.x) + (f.y - hpos.y) * (f.y - hpos.y));
            if (dd > 2600.0f) continue;   // sense only nearby threats, no x-ray walls
            float fade = 1.0f - dd / 2600.0f;
            float sx, sy;
            if (!WorldToScreen(_lastVP, W, H, f.x, f.y, f.z + 210.0f, sx, sy)) continue;
            float rr = (30 + 8 * pulse) * sc;
            quad(sx - rr, sy - rr, 2 * rr, 2 * rr, kRing, 255, 70, 60, (uint8_t)((90 + 110 * pulse) * fade + 30));
        }
        // score popups: "+N" in the original outlined font, rising and fading
        if (_fontAtlas) {
            useTex(_fontAtlas);
            for (size_t i = 0; i < _popups.size();) {
                const Popup &pp = _popups[i];
                uint32_t age = nowMs - pp.bornMs;
                if (age > 1300) { _popups.erase(_popups.begin() + i); continue; }
                float sx, sy;
                if (WorldToScreen(_lastVP, W, H, pp.x, pp.y, pp.z, sx, sy)) {
                    float rise = age / 1300.0f;
                    uint8_t a = (uint8_t)(255 * (1.0f - rise * rise));
                    float gh = 52 * sc, gs = gh / 29.0f;   // font row height 29 in the atlas
                    char buf[16];
                    if (pp.value == 0) snprintf(buf, sizeof buf, "CHECKPOINT");
                    else snprintf(buf, sizeof buf, "+%d", pp.value);
                    float total = 0;
                    for (char *c = buf; *c; ++c) total += (*c == '+' ? 18 : kDigW[*c - '0']) * gs + 2 * sc;
                    float x = sx - total * 0.5f, y = sy - 90.0f * sc * rise - gh;
                    for (char *c = buf; *c; ++c) {
                        bdae::SpriteModule g = (*c == '+') ? bdae::SpriteModule{210, 3, 17, 29}
                                                           : bdae::SpriteModule{kDigX[*c - '0'], 33, kDigW[*c - '0'], 23};
                        float gh2 = g.h * gs;
                        quad(x, y + (29 * gs - gh2) * 0.5f, g.w * gs, gh2, g, 255, 255, 255, a);
                        x += g.w * gs + 2 * sc;
                    }
                }
                ++i;
            }
        }
    }
    if (_paused && _flow.phase == bdae::GameFlow::PLAYING) {
        useTex(_white);
        quad(0, 0, W, H, kFull, 6, 6, 10, 150);
        drawText("PAUSED", W * 0.5f, H * 0.40f, 72 * sc, 1, 255);
        drawText("TAP THE BUBBLE TO RESUME", W * 0.5f, H * 0.60f, 34 * sc, 1, 220);
    }
    // combo banner (right side), like the original "N COMBOS!"
    if (_fontAtlas && _flow.phase == bdae::GameFlow::PLAYING && _comboHits >= 2 && nowMs - _comboLastMs < 1200) {
        char cb[32]; snprintf(cb, sizeof cb, "%d COMBOS!", _comboHits);
        uint8_t a = (uint8_t)(255 * (1.0f - (nowMs - _comboLastMs) / 1200.0f));
        drawText(cb, W - 40 * sc, H * 0.30f, 56 * sc, 2, a);
    }
    if (verts.empty()) return;
    if (!segs.empty()) segs.back().count = verts.size() - segs.back().start;
    NSUInteger bytes = verts.size() * sizeof(SpriteVert), cap = 512 * 6 * 20;
    if (bytes > cap) { verts.resize(cap / sizeof(SpriteVert)); bytes = cap; }
    NSUInteger off = (_frameIdx % kFramesInFlight) * cap;
    memcpy((uint8_t *)_spriteVB.contents + off, verts.data(), bytes);
    simd_float2 vpsz = {W, H};
    [enc setRenderPipelineState:_spritePipe];
    [enc setDepthStencilState:_noDepth];
    [enc setVertexBuffer:_spriteVB offset:off atIndex:0];
    [enc setVertexBytes:&vpsz length:sizeof(vpsz) atIndex:1];
    for (const Seg &sg : segs) {
        if (!sg.count || sg.start + sg.count > verts.size()) continue;
        [enc setFragmentTexture:sg.tex atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:sg.start vertexCount:sg.count];
    }
    [enc setDepthStencilState:_depth];
}

// ------------------------------------------------------------ boot videos ---
// The original boots into Gameloft-Logo.m4v then Spiderman-Trailer.m4v (the
// comic-art motion piece), both skippable. Files live in Assets/videos/.
- (void)startBootVideos {
    if (_bootPlayed) return;
    _bootPlayed = YES;
    _bootVideos = [NSMutableArray new];
    NSString *root = [NSString stringWithUTF8String:_assetRootStr.c_str()];
    for (NSString *n in @[@"Gameloft-Logo.m4v", @"Spiderman-Trailer.m4v"]) {
        NSString *p = [NSString stringWithFormat:@"%@/videos/%@", root, n];
        if ([NSFileManager.defaultManager fileExistsAtPath:p]) [_bootVideos addObject:p];
    }
    NSLog(@"[TotalMayhem] boot videos found: %lu", (unsigned long)_bootVideos.count);
    if (!_bootVideos.count) return;
    _videoIndex = 0;
    _flow.phase = bdae::GameFlow::VIDEO;
    [self playVideoAtIndex:0];
}

- (void)playVideoAtIndex:(NSUInteger)i {
    [self stopVideo];
    if (i >= _bootVideos.count) { _flow.showTitle((uint32_t)(CACurrentMediaTime() * 1000.0)); return; }
    _videoIndex = i;
    AVPlayerItem *item = [AVPlayerItem playerItemWithURL:[NSURL fileURLWithPath:_bootVideos[i]]];
    _video = [AVPlayer playerWithPlayerItem:item];
    _videoLayer = [AVPlayerLayer playerLayerWithPlayer:_video];
    _videoLayer.frame = _mtkView.bounds;
    _videoLayer.videoGravity = AVLayerVideoGravityResizeAspect;
    _videoLayer.backgroundColor = UIColor.blackColor.CGColor;
    [_mtkView.layer addSublayer:_videoLayer];
    [_mtkView bringSubviewToFront:_skipLabel];
    __weak typeof(self) weakSelf = self;
    [NSNotificationCenter.defaultCenter addObserverForName:AVPlayerItemDidPlayToEndTimeNotification
                                                    object:item queue:NSOperationQueue.mainQueue
                                                usingBlock:^(NSNotification *n) {
        typeof(self) sself = weakSelf;
        if (sself) [sself playVideoAtIndex:sself->_videoIndex + 1];
    }];
    [_video play];
}

- (void)stopVideo {
    if (_video) { [_video pause]; _video = nil; }
    if (_videoLayer) { [_videoLayer removeFromSuperlayer]; _videoLayer = nil; }
}

// ------------------------------------------------------------------ props ---
// Every original prop placement (lampposts, cars, hostages, destructibles...)
// as static textured geometry. Archetypes are shared per mesh file; skinned
// props (hostages) draw in bind pose for now.
- (void)loadPropsFromLevel:(const std::string &)assetRoot {
    _propVBs = [NSMutableArray new]; _propIBs = [NSMutableArray new]; _propTex = [NSMutableArray new];
    _propCounts.clear(); _propBatchArch.clear(); _propAlphaTest.clear(); _props.clear();
    _bonusTaken.assign(_room ? _room->bonuses.size() : 0, false);
    _score = 0;
    if (!_levelReady) return;
    std::map<std::string, int> archOf;      // meshFile -> archetype index (-1 = failed)
    std::vector<std::pair<int, int>> archRange;   // [firstBatch, count) per archetype
    int placed = 0;
    for (const LevelRoom::PropSpawn &pr : _room->props) {
        if (_props.size() >= 320) break;
        auto it = archOf.find(pr.meshFile);
        int ai;
        if (it != archOf.end()) ai = it->second;
        else {
            Model m; std::string e;
            bool ok = m.loadMesh(resolveCaseInsensitive(assetRoot + "/" + pr.meshFile), e);
            if (!ok) {   // original mounts all packs: try the sibling level pack
                std::string alt = pr.meshFile;
                size_t lp = alt.find("levelnew_01");
                if (lp != std::string::npos) alt.replace(lp, 11, "levelnew_02");
                ok = m.loadMesh(resolveCaseInsensitive(assetRoot + "/" + alt), e);
            }
            if (!ok) { archOf[pr.meshFile] = -1; continue; }
            int first = (int)_propCounts.size();
            for (const Mesh &mesh : m.meshes) {
                auto pushRange = [&](uint32_t i0, uint32_t i1, const std::string &texName) {
                    std::vector<GPUStaticVertex> verts; std::vector<uint16_t> idx;
                    std::vector<int32_t> remap(mesh.vertices.size(), -1);
                    for (uint32_t i = i0; i < i1 && i < mesh.indices.size(); ++i) {
                        uint16_t vi = mesh.indices[i];
                        if (remap[vi] < 0) {
                            const Vertex &v = mesh.vertices[vi]; GPUStaticVertex d;
                            d.p[0] = v.px; d.p[1] = v.py; d.p[2] = v.pz;
                            d.n[0] = v.nx; d.n[1] = v.ny; d.n[2] = v.nz;
                            d.uv[0] = v.u; d.uv[1] = v.v; d.uv2[0] = v.u2; d.uv2[1] = v.v2;
                            for (int k = 0; k < 4; ++k) d.c[k] = 255;
                            remap[vi] = (int32_t)verts.size(); verts.push_back(d);
                        }
                        idx.push_back((uint16_t)remap[vi]);
                    }
                    if (verts.empty()) return;
                    id<MTLTexture> t = [self levelTextureNamed:texName];
                    if (!t && !m.textureNames.empty()) t = [self levelTextureNamed:m.textureNames.front()];
                    [_propVBs addObject:[_device newBufferWithBytes:verts.data() length:verts.size() * sizeof(GPUStaticVertex) options:MTLResourceStorageModeShared]];
                    [_propIBs addObject:[_device newBufferWithBytes:idx.data() length:idx.size() * sizeof(uint16_t) options:MTLResourceStorageModeShared]];
                    [_propTex addObject:(t ? t : _white)];
                    _propCounts.push_back(idx.size());
                    _propBatchArch.push_back((int)archRange.size());
                    NSString *low = [NSString stringWithUTF8String:texName.c_str()].lowercaseString;
                    _propAlphaTest.push_back([low containsString:@"alphatest"] ? 1 : 0);
                };
                if (mesh.subMeshes.empty()) pushRange(0, (uint32_t)mesh.indices.size(), "");
                else for (const SubMesh &sm : mesh.subMeshes) pushRange(sm.firstIndex, sm.firstIndex + sm.indexCount, sm.diffuse);
            }
            ai = (int)archRange.size();
            archRange.push_back({first, (int)_propCounts.size() - first});
            archOf[pr.meshFile] = ai;
        }
        if (ai < 0) continue;
        PropInst pi;
        pi.arch = ai; pi.model = ToSimd(pr.transform);
        pi.x = pr.transform.m[12]; pi.y = pr.transform.m[13]; pi.z = pr.transform.m[14];
        pi.alive = true; pi.destructible = (pr.type == "DestroyableObject");
        _props.push_back(pi); ++placed;
    }
    _propRanges = archRange;
    NSLog(@"[TotalMayhem] props: %d placed, %zu archetypes, %zu GPU batches; %zu bonuses",
          placed, archRange.size(), _propCounts.size(), _bonusTaken.size());
}

- (void)drawProps:(id<MTLRenderCommandEncoder>)enc vp:(simd_float4x4)vp eye:(simd_float3)eye {
    if (_props.empty()) return;
    [enc setRenderPipelineState:_staticPipe];
    Uniforms u; u.vp = vp; u.tint = (simd_float4){1, 1, 1, 1};
    for (const PropInst &p : _props) {
        if (!p.alive) continue;
        float dx = p.x - eye.x, dy = p.y - eye.y;
        if (dx * dx + dy * dy > 9000.0f * 9000.0f) continue;   // distance cull
        u.model = p.model;
        const auto &r = _propRanges[p.arch];
        for (int b = r.first; b < r.first + r.second; ++b) {
            u.misc = (simd_float4){_propTex[b] != _white ? 1.0f : 0.0f, 0, 0, _propAlphaTest[b] ? 1.0f : 0.0f};
            [enc setVertexBytes:&u length:sizeof(u) atIndex:1];
            [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
            [enc setFragmentTexture:_propTex[b] atIndex:0];
            [enc setFragmentTexture:_white atIndex:1];
            [enc setVertexBuffer:_propVBs[b] offset:0 atIndex:0];
            [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:_propCounts[b]
                             indexType:MTLIndexTypeUInt16 indexBuffer:_propIBs[b] indexBufferOffset:0];
        }
    }
}

// world -> screen pixels (top-left origin); returns false when behind the camera
static bool WorldToScreen(simd_float4x4 vp, float W, float H, float x, float y, float z, float &sx, float &sy) {
    simd_float4 c = simd_mul(vp, (simd_float4){x, y, z, 1});
    if (c.w <= 1.0f) return false;
    sx = (c.x / c.w * 0.5f + 0.5f) * W;
    sy = (1.0f - (c.y / c.w * 0.5f + 0.5f)) * H;
    return true;
}

// -------------------------------------------------------------------- sky ---
- (void)loadSky:(const std::string &)assetRoot levelDir:(const char *)levelDir {
    _skyVBs = [NSMutableArray new]; _skyIBs = [NSMutableArray new]; _skyTex = [NSMutableArray new];
    _skyCounts.clear();
    std::string dir = assetRoot + "/" + levelDir + "/meshes_bin/";
    const char *names[] = {"lvl01_sky.bdae", "lvl02_sky.bdae", "sky.bdae"};
    Model sky; std::string e; bool ok = false;
    for (const char *n : names) if (sky.loadMesh(resolveCaseInsensitive(dir + n), e)) { ok = true; break; }
    if (!ok) { NSLog(@"[TotalMayhem] sky: none in %s", dir.c_str()); return; }
    for (const Mesh &m : sky.meshes) {
        for (const SubMesh &sm : m.subMeshes) {
            std::vector<GPUStaticVertex> verts;
            std::vector<uint16_t> idx;
            std::vector<int32_t> remap(m.vertices.size(), -1);
            for (uint32_t i = sm.firstIndex; i < sm.firstIndex + sm.indexCount && i < m.indices.size(); ++i) {
                uint16_t vi = m.indices[i];
                if (remap[vi] < 0) {
                    const Vertex &v = m.vertices[vi];
                    GPUStaticVertex d;
                    d.p[0] = v.px; d.p[1] = v.py; d.p[2] = v.pz;
                    d.n[0] = v.nx; d.n[1] = v.ny; d.n[2] = v.nz;
                    d.uv[0] = v.u; d.uv[1] = v.v; d.uv2[0] = v.u2; d.uv2[1] = v.v2;
                    for (int k = 0; k < 4; ++k) d.c[k] = 255;
                    remap[vi] = (int32_t)verts.size(); verts.push_back(d);
                }
                idx.push_back((uint16_t)remap[vi]);
            }
            if (verts.empty()) continue;
            id<MTLTexture> t = [self levelTextureNamed:sm.diffuse];
            if (!t && !sky.textureNames.empty()) t = [self levelTextureNamed:sky.textureNames.front()];
            [_skyVBs addObject:[_device newBufferWithBytes:verts.data() length:verts.size() * sizeof(GPUStaticVertex) options:MTLResourceStorageModeShared]];
            [_skyIBs addObject:[_device newBufferWithBytes:idx.data() length:idx.size() * sizeof(uint16_t) options:MTLResourceStorageModeShared]];
            [_skyTex addObject:(t ? t : _white)];
            _skyCounts.push_back(idx.size());
        }
    }
    NSLog(@"[TotalMayhem] sky: %zu batches", _skyCounts.size());
}

- (id<MTLTexture>)comicPage:(int)page {
    if (_comicLoadedPage == page) return _comicTex;
    NSString *root = [NSString stringWithUTF8String:_assetRootStr.c_str()];
    for (NSString *dir in @[@"comic1", @"comic2", @"comics"]) {
        NSString *p = [NSString stringWithFormat:@"%@/%@/comic_%d.tga", root, dir, page];
        if ([NSFileManager.defaultManager fileExistsAtPath:p]) {
            _comicTex = LoadPVRTC(_device, p); _comicLoadedPage = page;
            if (!_comicTex) NSLog(@"[TotalMayhem] comic page %d failed to decode", page);
            return _comicTex;
        }
    }
    NSLog(@"[TotalMayhem] comic page %d not found (extract comic1.pack into Assets/comic1)", page);
    _comicLoadedPage = page; _comicTex = nil;
    return nil;
}

// --------------------------------------------------------------- textures ---
// The original app mounts every pack; level geometry references textures that
// ship in other level packs, so index every */textures_bin under the asset root.
- (void)buildTextureIndex:(const std::string &)assetRoot {
    if (_texIndex) return;
    _texIndex = [NSMutableDictionary new];
    _texCache = [NSMutableDictionary new];
    NSFileManager *fm = NSFileManager.defaultManager;
    NSString *root = [NSString stringWithUTF8String:assetRoot.c_str()];
    for (NSString *sub in [fm contentsOfDirectoryAtPath:root error:nil]) {
        NSString *dir = [[root stringByAppendingPathComponent:sub] stringByAppendingPathComponent:@"textures_bin"];
        for (NSString *f in [fm contentsOfDirectoryAtPath:dir error:nil])
            if (!_texIndex[f.lowercaseString]) _texIndex[f.lowercaseString] = [dir stringByAppendingPathComponent:f];
    }
    NSLog(@"[TotalMayhem] texture index: %lu files", (unsigned long)_texIndex.count);
}

- (id<MTLTexture>)levelTextureNamed:(const std::string &)name {
    if (name.empty()) return nil;
    NSString *key = [NSString stringWithUTF8String:name.c_str()].lowercaseString;
    id<MTLTexture> cached = _texCache[key];
    if (cached) return cached == (id)NSNull.null ? nil : cached;
    NSString *path = _texIndex[key];
    if (!path) {   // "42_mall_glass2.tga" -> "42_mall_glass.tga"
        NSString *stem = [key stringByDeletingPathExtension];
        while (stem.length && [stem hasSuffix:@"1"]) stem = [stem substringToIndex:stem.length - 1];
        NSString *stripped = [stem stringByReplacingOccurrencesOfString:@"[0-9]+$" withString:@""
                                                                options:NSRegularExpressionSearch range:NSMakeRange(0, stem.length)];
        path = _texIndex[[stripped stringByAppendingPathExtension:key.pathExtension]];
    }
    if (!path) {   // "13_building.tga" -> any "*building.tga" (leading pack/slot digits differ)
        NSString *core = [key stringByReplacingOccurrencesOfString:@"^[_0-9]+" withString:@""
                                                           options:NSRegularExpressionSearch range:NSMakeRange(0, key.length)];
        if (core.length > 5)
            for (NSString *k in _texIndex) if ([k hasSuffix:core]) { path = _texIndex[k]; break; }
    }
    id<MTLTexture> t = path ? LoadPVRTC(_device, path) : nil;
    _texCache[key] = t ? t : (id)NSNull.null;
    if (!t) NSLog(@"[TotalMayhem] texture missing: %s", name.c_str());
    return t;
}

// ---------------------------------------------------------------- enemies ---
// Thug archetypes map straight off the original !GameType names.  thug_knife
// ships no animation file but shares thug_bat's 26-node rig (verified: all 26
// channels bind), so it reuses thug_bat_anim.
- (void)loadEnemiesFromLevel:(const std::string &)assetRoot {
    _npcVBs = [NSMutableArray new];
    _npcIBs = [NSMutableArray new];
    _npcTexs = [NSMutableArray new];
    if (!_levelReady) return;

    struct Arch { const char *prefix; const char *mesh; const char *anim; };
    static const Arch kArch[] = {
        {"MeleeThugEnemy_bat",   "thug_bat_mesh.bdae",     "thug_bat_anim.bdae"},
        {"MeleeThugEnemy_knife", "thug_knife_mesh.bdae",   "thug_bat_anim.bdae"},
        {"RangeThug_molotov",    "thug_molotov_mesh.bdae", "thug_molotov_anim.bdae"},
        {"MeleeThug_gun",        "thug_gun_mesh.bdae",     "thug_gun_anim.bdae"},
        {"RangeThug_hammer",     "thug_hammer_mesh.bdae",  "thug_hammer_anim.bdae"},
        {"RangeThug_big",        "thug_big_mesh.bdae",     "thug_big_anim.bdae"},
        {"Boss_Sandman",         "sandman_mesh.bdae",      "sandman_anim.bdae"},
        {"Boss_Rhino",           "rhino_mesh.bdae",        "rhino_anim.bdae"},
    };
    std::string statErr;
    std::map<std::string, bdae::EnemyStats> statTable =
        bdae::loadEnemyStats(assetRoot + "/configs", statErr);
    if (!statErr.empty()) NSLog(@"[TotalMayhem] enemy stats: %s", statErr.c_str());
    static const std::map<std::string, std::string> kStatName = {
        {"MeleeThugEnemy_bat", "THUG_BAT"},   {"MeleeThugEnemy_knife", "THUG_KNIFE"},
        {"RangeThug_molotov", "THUG_MOLOTOV"},{"MeleeThug_gun", "THUG_GUN"},
        {"RangeThug_hammer", "THUG_HAMMER"},  {"RangeThug_big", "THUG_BIG"},
        {"Boss_Sandman", "SANDMAN"},          {"Boss_Rhino", "RHINO"}};
    std::map<std::string, int> typeIndex;
    const NSUInteger kMaxNPC = 40;
    const NSUInteger kSlot = kBoneSlot;

    for (const LevelRoom::EnemySpawn &en : _room->enemies) {
        if (_npcs.size() >= kMaxNPC) break;
        bool isBoss = en.type.rfind("Boss_", 0) == 0;
        if (en.pos.z > 200.0f && !isBoss) continue;   // elevated non-boss placements wait for triggers
        const Arch *arch = nullptr;
        for (const Arch &a : kArch)
            if (en.type.rfind(a.prefix, 0) == 0) { arch = &a; break; }
        if (!arch) continue;

        auto it = typeIndex.find(arch->prefix);
        int ti;
        if (it != typeIndex.end()) {
            ti = it->second;
        } else {
            auto model = std::make_unique<Model>();
            std::string e;
            std::string base = assetRoot + "/entities/meshes_bin/";
            if (!model->loadMesh(base + arch->mesh, e) ||
                !model->loadAnimation(base + arch->anim, e)) {
                NSLog(@"[TotalMayhem] enemy archetype %s failed: %s", arch->prefix, e.c_str());
                typeIndex[arch->prefix] = -1;
                continue;
            }
            const Clip *idle = model->findClip("idle");
            if (!idle || !model->skin.valid || model->skin.jointNode.size() > 40) {
                typeIndex[arch->prefix] = -1;
                continue;
            }
            const Mesh &m = model->meshes.front();
            std::vector<GPUSkinVertex> verts(m.vertices.size());
            for (size_t i = 0; i < m.vertices.size(); ++i) {
                const Vertex &s = m.vertices[i];
                GPUSkinVertex &d = verts[i];
                d.p[0] = s.px; d.p[1] = s.py; d.p[2] = s.pz;
                d.n[0] = s.nx; d.n[1] = s.ny; d.n[2] = s.nz;
                d.uv[0] = s.u; d.uv[1] = s.v;
                for (int k = 0; k < 4; ++k) { d.bone[k] = s.bone[k]; d.w[k] = s.weight[k]; }
            }
            [_npcVBs addObject:[_device newBufferWithBytes:verts.data()
                                                    length:verts.size() * sizeof(GPUSkinVertex)
                                                   options:MTLResourceStorageModeShared]];
            [_npcIBs addObject:[_device newBufferWithBytes:m.indices.data()
                                                    length:m.indices.size() * sizeof(uint16_t)
                                                   options:MTLResourceStorageModeShared]];
            _npcIndexCount.push_back(m.indices.size());
            id<MTLTexture> tex = nil;
            if (!model->textureNames.empty()) {
                NSString *tp = [NSString stringWithFormat:@"%s/entities/textures_bin/%s",
                                assetRoot.c_str(), model->textureNames.front().c_str()];
                tex = LoadPVRTC(_device, tp);
            }
            [_npcTexs addObject:(tex ? tex : _white)];
            _npcIdle.push_back(idle);
            _npcAnchor.push_back(skinnedAnchor(*model, idle->startMs));
            _npcModel.push_back(std::move(model));
            ti = (int)_npcModel.size() - 1;
            typeIndex[arch->prefix] = ti;
        }
        if (ti < 0) continue;

        float gz = en.pos.z;
        float z;
        if (_room->canStandAt(en.pos.x, en.pos.y, z)) gz = z;
        _npcs.push_back({ti, en.pos.x, en.pos.y, gz, en.yaw,
                         (float)((_npcs.size() * 977) % 4000)});
        bdae::EnemyStats st;
        auto sn = kStatName.find(arch->prefix);
        if (sn != kStatName.end() && statTable.count(sn->second)) st = statTable[sn->second];
        st.ranged = (std::strncmp(arch->prefix, "Range", 5) == 0) ||
                    (std::strcmp(arch->prefix, "MeleeThug_gun") == 0);
        st.damage = (std::strncmp(arch->prefix, "Boss_", 5) == 0) ? 12.0f : 5.0f;
        _foes.emplace_back();
        _foes.back().bind(_npcModel[ti].get(), _room.get(), st,
                          en.pos.x, en.pos.y, gz, en.yaw);
    }
    if (!_npcs.empty())
        _npcBones = [_device newBufferWithLength:kFramesInFlight * _npcs.size() * kSlot
                                         options:MTLResourceStorageModeShared];
    NSLog(@"[TotalMayhem] enemies placed: %zu of %zu parsed spawns (%zu archetypes)",
          _npcs.size(), _room->enemies.size(), _npcModel.size());
}

- (void)drawEnemies:(id<MTLRenderCommandEncoder>)enc vp:(simd_float4x4)vp time:(CFTimeInterval)now {
    if (_npcs.empty() || !_npcBones) return;
    const NSUInteger kSlot = kBoneSlot;
    const NSUInteger regionOff = (_frameIdx % kFramesInFlight) * _npcs.size() * kSlot;
    uint8_t *base = (uint8_t *)_npcBones.contents + regionOff;
    [enc setRenderPipelineState:_skinPipe];
    uint32_t nowMs = (uint32_t)(now * 1000.0);
    for (size_t i = 0; i < _npcs.size(); ++i) {
        const NPCInst &n = _npcs[i];
        Model &m = *_npcModel[n.type];
        const Clip *clip = nullptr; uint32_t t = 0;
        if (i < _foes.size()) _foes[i].poseInfo(nowMs + (uint32_t)n.phase, clip, t);
        if (!clip) { const Clip *idle = _npcIdle[n.type]; clip = idle; t = idle->startMs; }
        m.poseAtTime(t);
        const std::vector<Mat4> &sk = m.skinningMatrices();
        simd_float4x4 *dst = (simd_float4x4 *)(base + i * kSlot);
        NSUInteger nb = MIN((NSUInteger)sk.size(), (NSUInteger)40);
        for (NSUInteger k = 0; k < nb; ++k) dst[k] = ToSimd(sk[k]);

        const Vec3 &a = _npcAnchor[n.type];
        float ex = n.x, ey = n.y, ez = n.z, eyaw = n.yaw;
        if (i < _foes.size()) { ex = _foes[i].x; ey = _foes[i].y; ez = _foes[i].z; eyaw = _foes[i].yaw; }
        if (i < _foes.size()) {
            float sink = _foes[i].corpseSink(nowMs);
            if (sink >= 1.0f) continue;
            ez -= sink * 240.0f;
        }
        Uniforms u;
        u.vp = vp;
        u.model = simd_mul(simd_mul(MTranslate(ex, ey, ez), MRotZ(eyaw + kModelYawOffset)),
                           MTranslate(-a.x, -a.y, -a.z));
        u.tint = (simd_float4){1, 1, 1, 1};
        u.misc = (simd_float4){[_npcTexs objectAtIndex:n.type] != _white ? 1.0f : 0.0f, 0, 0, 0};
        [enc setVertexBuffer:_npcVBs[n.type] offset:0 atIndex:0];
        [enc setVertexBytes:&u length:sizeof(u) atIndex:1];
        [enc setVertexBuffer:_npcBones offset:regionOff + i * kSlot atIndex:2];
        [enc setFragmentTexture:_npcTexs[n.type] atIndex:0];
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
        [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:_npcIndexCount[n.type]
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:_npcIBs[n.type]
                 indexBufferOffset:0];
    }
}


- (void)loadLevelIndex:(int)idx {
    const std::string assetRoot = _assetRootStr;
    std::string levelErr;
    _levelCounts.clear();
    _npcModel.clear(); _npcIndexCount.clear(); _npcIdle.clear();
    _npcAnchor.clear(); _npcs.clear(); _foes.clear();
    _npcBones = nil;
    _room = std::make_unique<LevelRoom>();
    _levelReady = _room->loadFullLevel(assetRoot, kLevelDirs[idx % kLevelCount], levelErr);
    if (!_levelReady) NSLog(@"[TotalMayhem] level %d failed: %s", idx, levelErr.c_str());
    _levelVBs = [NSMutableArray new];
    _levelIBs = [NSMutableArray new];
    _levelTex = [NSMutableArray new];
    _levelLM = [NSMutableArray new];
    _levelFlags.clear();
    [self buildTextureIndex:assetRoot];
    if (_levelReady) {
        for (const TriMesh &lv : _room->visualBatches) {
            std::vector<GPUStaticVertex> verts(lv.vertices.size());
            for (size_t i = 0; i < lv.vertices.size(); ++i) {
                const Vertex &s = lv.vertices[i];
                GPUStaticVertex &d = verts[i];
                d.p[0] = s.px; d.p[1] = s.py; d.p[2] = s.pz;
                d.n[0] = s.nx; d.n[1] = s.ny; d.n[2] = s.nz;
                d.uv[0] = s.u; d.uv[1] = s.v;
                d.uv2[0] = s.u2; d.uv2[1] = s.v2;
                for (int k = 0; k < 4; ++k) d.c[k] = s.color[k];
            }
            {
                id<MTLTexture> dt = [self levelTextureNamed:lv.diffuse];
                id<MTLTexture> lt = [self levelTextureNamed:lv.lightmap];
                int flags = (dt ? 1 : 0) | (lt ? 2 : 0) | (lv.diffuseUv == 1 ? 8 : 0);
                NSString *low = [NSString stringWithUTF8String:lv.diffuse.c_str()].lowercaseString;
                if ([low containsString:@"alphatest"]) flags |= 4;
                [_levelTex addObject:(dt ? dt : _white)];
                [_levelLM addObject:(lt ? lt : _white)];
                _levelFlags.push_back(flags);
            }
            [_levelVBs addObject:[_device newBufferWithBytes:verts.data()
                                                      length:verts.size() * sizeof(GPUStaticVertex)
                                                     options:MTLResourceStorageModeShared]];
            [_levelIBs addObject:[_device newBufferWithBytes:lv.indices.data()
                                                      length:lv.indices.size() * sizeof(uint16_t)
                                                     options:MTLResourceStorageModeShared]];
            _levelCounts.push_back(lv.indices.size());
        }
    }

    if (_heroReady) _fists.bind(*_hero);
    [self loadEnemiesFromLevel:assetRoot];
    [self loadSky:assetRoot levelDir:kLevelDirs[idx % kLevelCount]];
    [self loadPropsFromLevel:assetRoot];
    _comicLoadedPage = -1; _comicTex = nil;
    if (_actor) {
        _actor->bind(_hero.get(), _levelReady ? _room.get() : nullptr);
        if (_levelReady && _room->hasSpawn) _actor->spawnAt(_room->spawn, _room->spawnYaw);
    }
    _heroHP = 100.0f;
    _webEnergy = 100.0f;
    _paused = NO;
    if (_levelReady)
        _flow.beginLevel(*_room, idx % kLevelCount, (uint32_t)(CACurrentMediaTime() * 1000.0));
    NSLog(@"[TotalMayhem] level %d (%s): %zu enemies, %zu checkpoints", idx,
          kLevelDirs[idx % kLevelCount], _foes.size(), _flow.checkpointsAll.size());
}

// ------------------------------------------------------------------ input ---
- (void)touchBegin:(CGPoint)p {
    if (_flow.phase != bdae::GameFlow::PLAYING) {
        uint32_t nowMs = (uint32_t)(CACurrentMediaTime() * 1000.0);
        if (_flow.phase == bdae::GameFlow::VIDEO) {
            [self playVideoAtIndex:_videoIndex + 1];   // tap = skip this clip
            return;
        }
        if (_flow.phase == bdae::GameFlow::COMIC) {
            CGFloat sw = UIScreen.mainScreen.bounds.size.width;
            if (p.x > sw * 0.78 && p.y < 90) { _flow.comicIndex = _flow.comicCount - 1; }   // SKIP zone
            _flow.advanceComic(nowMs);
        } else if (_flow.phase == bdae::GameFlow::TITLE) {
            _flow.startPlay(nowMs);
        } else if (_flow.phase == bdae::GameFlow::DEAD) {
            _heroHP = 100.0f;
            if (_actor) _actor->spawnAt(_flow.checkpoint, _flow.checkpointYaw);
            _flow.respawn(nowMs);
        } else if (_flow.phase == bdae::GameFlow::COMPLETE) {
            [self loadLevelIndex:(_flow.levelIndex + 1) % kLevelCount];
        }
        return;
    }

    CGFloat sw = UIScreen.mainScreen.bounds.size.width;
    CGFloat sh = UIScreen.mainScreen.bounds.size.height;
    // top-left corner: single tap = pause, double-height zone toggles atlas sheet
    if (p.x < 44 && p.y < 44) { _paused = !_paused; return; }
    if (p.x < 30 && p.y > 60 && p.y < 100) { _showAtlasSheet = !_showAtlasSheet; return; }
    // the three action buttons (layout in 768-pt space, mirrored in drawHUD)
    {
        CGFloat k = sh / 768.0;
        CGPoint fist = CGPointMake(sw - 266 * k, 511 * k), dodge = CGPointMake(sw - 96 * k, 511 * k), web = CGPointMake(sw - 319 * k, 665 * k);
        CGFloat r = 82 * k;
        auto inside = [&](CGPoint c) { return hypot(p.x - c.x, p.y - c.y) < r; };
        if (inside(fist)) { _fists.tryPunch((uint32_t)(CACurrentMediaTime() * 1000.0)); return; }
        if (inside(web)) {   // web attack: costs web power, snaps the nearest foe in front
            uint32_t nowMs = (uint32_t)(CACurrentMediaTime() * 1000.0);
            if (_webEnergy >= 25.0f && _actor) {
                Vec3 hp = _actor->position(); float yaw = _actor->yaw();
                float fx = cosf(yaw), fy = sinf(yaw);
                bdae::EnemyActor *best = nullptr; float bestD = 800.0f;
                for (bdae::EnemyActor &f : _foes) {
                    if (!f.alive()) continue;
                    float dx = f.x - hp.x, dy = f.y - hp.y, d = sqrtf(dx * dx + dy * dy);
                    if (d < bestD && (d < 1.0f || (dx * fx + dy * fy) / d > 0.3f)) { best = &f; bestD = d; }
                }
                if (best) {
                    _webEnergy -= 25.0f;
                    best->takeHit(15.0f, nowMs, hp.x, hp.y);
                    _score += 15;
                    _popups.push_back({best->x, best->y, best->z + 190.0f, 15, nowMs});
                    _fists.tryPunch(nowMs);   // reuse the strike animation for now
                }
            }
            return;
        }
        if (inside(dodge)) return;   // dodge: not implemented yet
    }
    CGFloat half = sw * 0.5;
    if (p.x < half && _moveTouchActive < 0) {
        _moveTouchActive = 1;
        _moveOriginX = p.x;
        _moveOriginY = p.y;
        _stickX = _stickY = 0;
        _moveDownT = CACurrentMediaTime();
        _moveMaxLen = 0;
    } else {
        _lookTouchActive = 1;
        _lookLastX = p.x;
        _lookLastY = p.y;
    }
}

- (void)touchMove:(CGPoint)p {
    
    CGFloat half = UIScreen.mainScreen.bounds.size.width * 0.5;
    if (p.x < half && _moveTouchActive > 0) {
        const float radius = 70.0f;
        float dx = ((float)p.x - _moveOriginX) / radius;
        float dy = -((float)p.y - _moveOriginY) / radius;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 1.0f) { dx /= len; dy /= len; }
        _stickX = dx;
        _stickY = dy;
        if (len > _moveMaxLen) _moveMaxLen = len;
    } else if (_lookTouchActive > 0) {
        _camYaw -= ((float)p.x - _lookLastX) * 0.009f;
        _camPitch = fmaxf(-0.10f, fminf(0.85f, _camPitch - ((float)p.y - _lookLastY) * 0.006f));
        _lookLastX = p.x;
        _lookLastY = p.y;
    }
}

- (void)touchEnd:(CGPoint)p {
    
    CGFloat half = UIScreen.mainScreen.bounds.size.width * 0.5;
    if (p.x < half) {
        // A quick, near-stationary touch on the move side is a punch.
        if (CACurrentMediaTime() - _moveDownT < 0.30 && _moveMaxLen < 0.25)
            _fists.tryPunch((uint32_t)(CACurrentMediaTime() * 1000.0));
        _moveTouchActive = -1; _stickX = _stickY = 0;
    }
    else            { _lookTouchActive = -1; }
}

- (void)accelerometerX:(float)x y:(float)y z:(float)z { }
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {}

@end
