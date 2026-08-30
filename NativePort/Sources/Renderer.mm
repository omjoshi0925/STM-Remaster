// Renderer.mm - Milestone 4: skinned Spider-Man + original Level 1 Room 1.
//
// All parsing/animation/collision lives in portable C++ (BDAEModel, Level,
// Character) which is unit-tested on the host.  This file only does Metal.
#import "Renderer.h"
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#include "GameRuntime.hpp"
#include "BDAEModel.hpp"
#include "Level.hpp"
#include "Character.hpp"
#include <memory>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <simd/simd.h>

using namespace bdae;

// The Spider-Man rig's toes point down -Y, so a yaw of 0 (facing +X) needs a
// +90 degree correction.  Derived from the bind pose, not guessed:
//   Bip01_L_Foot y = +4.95, Bip01_L_Toe0 y = -8.30
static const float kModelYawOffset = (float)M_PI_2;

static const NSUInteger kMaxBones = 38;

struct GPUSkinVertex { float p[3]; float n[3]; float uv[2]; uint16_t bone[4]; float w[4]; };
struct GPUStaticVertex { float p[3]; float n[3]; float uv[2]; uint8_t c[4]; };
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
static id<MTLTexture> LoadPVRTC(id<MTLDevice> d, NSString *path) {
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (!data || data.length < 60) return nil;
    const uint8_t *b = (const uint8_t *)data.bytes;
    if (memcmp(b, "BTEXpvr", 7) != 0) return nil;
    uint32_t hs = rdle32(b + 8), w = rdle32(b + 12), h = rdle32(b + 16);
    uint32_t flags = rdle32(b + 24), len = rdle32(b + 28), bpp = rdle32(b + 32);
    if (hs < 52 || !w || !h || bpp != 4) return nil;
    size_t p = hs;
    if (p + 8 <= data.length && memcmp(b + p, "PVR!", 4) == 0) p += 8;
    if (p + len > data.length || (flags & 0xff) != 0x19) return nil;
    MTLPixelFormat f = (flags & 0x8000) ? MTLPixelFormatPVRTC_RGBA_4BPP : MTLPixelFormatPVRTC_RGB_4BPP;
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f
                                                                                  width:w height:h mipmapped:NO];
    id<MTLTexture> t = [d newTextureWithDescriptor:td];
    if (t) [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:b + p bytesPerRow:0];
    return t;
}

static const char *const kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct SkinV   { packed_float3 p; packed_float3 n; float2 uv; ushort4 b; packed_float4 w; };
struct StaticV { packed_float3 p; packed_float3 n; float2 uv; uchar4 c; };
struct U       { float4x4 vp; float4x4 model; float4 tint; float4 misc; };
struct Out     { float4 p [[position]]; float2 uv; float l; float3 vc; };

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
    o.vc = float3(v.c.rgb) / 255.0;
    return o;
}

fragment half4 frag(Out i                   [[stage_in]],
                    texture2d<half> t       [[texture(0)]],
                    sampler s               [[sampler(0)]],
                    constant U &u           [[buffer(1)]]) {
    half3 base = (u.misc.x > 0.5) ? t.sample(s, i.uv).rgb : half3(u.tint.rgb);
    return half4(base * half3(i.vc) * half(i.l), half(u.tint.a));
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
    id<MTLBuffer> _npcBones;

    std::unique_ptr<GameRuntime> _game;
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
    _game = std::make_unique<GameRuntime>();

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
    _game->boot(root.UTF8String);

    std::string assetRoot = std::string(root.UTF8String) + "/Assets";
    std::string err, heroErr, animErr, levelErr;

    _hero = std::make_unique<Model>();
    _heroReady = _hero->loadMesh(assetRoot + "/entities/meshes_bin/spiderman_mesh.bdae", heroErr);
    BOOL animReady = NO;
    if (_heroReady) animReady = _hero->loadAnimation(assetRoot + "/entities/meshes_bin/spiderman_anim.bdae", animErr);

    _room = std::make_unique<LevelRoom>();
    _levelReady = _room->loadFullLevel(assetRoot, "levelnew_01", levelErr);

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
        _boneBuf = [_device newBufferWithLength:kMaxBones * sizeof(simd_float4x4)
                                        options:MTLResourceStorageModeShared];
        if (!_hero->textureNames.empty()) {
            NSString *tp = [root stringByAppendingPathComponent:
                            [NSString stringWithFormat:@"Assets/entities/textures_bin/%s",
                             _hero->textureNames.front().c_str()]];
            _heroTex = LoadPVRTC(_device, tp);
        }
    }

    _levelVBs = [NSMutableArray new];
    _levelIBs = [NSMutableArray new];
    if (_levelReady) {
        for (const TriMesh &lv : _room->visualBatches) {
            std::vector<GPUStaticVertex> verts(lv.vertices.size());
            for (size_t i = 0; i < lv.vertices.size(); ++i) {
                const Vertex &s = lv.vertices[i];
                GPUStaticVertex &d = verts[i];
                d.p[0] = s.px; d.p[1] = s.py; d.p[2] = s.pz;
                d.n[0] = s.nx; d.n[1] = s.ny; d.n[2] = s.nz;
                d.uv[0] = s.u; d.uv[1] = s.v;
                for (int k = 0; k < 4; ++k) d.c[k] = s.color[k];
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

    [self loadEnemiesFromLevel:assetRoot];

    _actor = std::make_unique<Character>();
    if (_heroReady && animReady) {
        _actor->bind(_hero.get(), _levelReady ? _room.get() : nullptr);
        if (_levelReady && _room->hasSpawn) _actor->spawnAt(_room->spawn, _room->spawnYaw);
    }

    _camYaw = 0.6f;
    _camPitch = 0.30f;
    _camDist = 430.0f;
    _moveTouchActive = -1;
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
        : [NSString stringWithFormat:@"Level FAILED: %s", levelErr.c_str()];
    _label.numberOfLines = 0;
    _label.text = [NSString stringWithFormat:@"%@\n%@\nLEFT drag = move  RIGHT drag = camera", heroLine, levelLine];

    return self;
}

- (void)drawInMTKView:(MTKView *)view {
    CFTimeInterval now = CACurrentMediaTime();
    float dt = (float)fmin(0.05, now - _last);
    _last = now;
    _game->update(dt);

    // Stick input is camera-relative: +Y on the stick walks away from the camera.
    float fwdX = -sinf(_camYaw), fwdY = -cosf(_camYaw);
    float rightX = cosf(_camYaw), rightY = -sinf(_camYaw);
    float moveX = rightX * _stickX + fwdX * _stickY;
    float moveY = rightY * _stickX + fwdY * _stickY;
    if (_actor) _actor->update(dt, moveX, moveY);

    id<CAMetalDrawable> drawable = view.currentDrawable;
    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    if (!drawable || !pass) return;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.07, 0.09, 0.13, 1.0);
    pass.depthAttachment.clearDepth = 1.0;

    id<MTLCommandBuffer> cb = [_queue commandBuffer];
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

    if (_levelReady && _levelCounts.size()) {
        Uniforms u;
        u.vp = vp;
        u.model = MIdent();
        u.tint = (simd_float4){0.92f, 0.94f, 1.0f, 1.0f};
        u.misc = (simd_float4){0, 0.75f, 0, 0};   // mostly baked vertex lighting
        [enc setRenderPipelineState:_staticPipe];
        [enc setVertexBytes:&u length:sizeof(u) atIndex:1];
        [enc setFragmentTexture:_white atIndex:0];
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
        for (NSUInteger b = 0; b < _levelCounts.size(); ++b) {
            [enc setVertexBuffer:_levelVBs[b] offset:0 atIndex:0];
            [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:_levelCounts[b]
                             indexType:MTLIndexTypeUInt16
                           indexBuffer:_levelIBs[b]
                     indexBufferOffset:0];
        }
    }

    [self drawEnemies:enc vp:vp time:now];

    if (_heroReady && _heroIndexCount) {
        const std::vector<Mat4> &sk = _hero->skinningMatrices();
        simd_float4x4 *dst = (simd_float4x4 *)_boneBuf.contents;
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
        [enc setVertexBuffer:_boneBuf offset:0 atIndex:2];
        [enc setFragmentTexture:(_heroTex ? _heroTex : _white) atIndex:0];
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
        [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:_heroIndexCount
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:_heroIB
                 indexBufferOffset:0];
    }

    [enc endEncoding];
    [cb presentDrawable:drawable];
    [cb commit];
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
    };
    std::map<std::string, int> typeIndex;
    const NSUInteger kMaxNPC = 40;
    const NSUInteger kSlot = 2560;   // 40 bones * 64B, 256-aligned

    for (const LevelRoom::EnemySpawn &en : _room->enemies) {
        if (_npcs.size() >= kMaxNPC) break;
        if (en.pos.z > 200.0f) continue;   // elevated placements (Sandman etc.) wait for real AI
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
    }
    if (!_npcs.empty())
        _npcBones = [_device newBufferWithLength:_npcs.size() * kSlot
                                         options:MTLResourceStorageModeShared];
    NSLog(@"[TotalMayhem] enemies placed: %zu of %zu parsed spawns (%zu archetypes)",
          _npcs.size(), _room->enemies.size(), _npcModel.size());
}

- (void)drawEnemies:(id<MTLRenderCommandEncoder>)enc vp:(simd_float4x4)vp time:(CFTimeInterval)now {
    if (_npcs.empty() || !_npcBones) return;
    const NSUInteger kSlot = 2560;
    uint8_t *base = (uint8_t *)_npcBones.contents;
    [enc setRenderPipelineState:_skinPipe];
    for (size_t i = 0; i < _npcs.size(); ++i) {
        const NPCInst &n = _npcs[i];
        Model &m = *_npcModel[n.type];
        const Clip *idle = _npcIdle[n.type];
        uint32_t len = idle->endMs - idle->startMs;
        uint32_t t = idle->startMs + (len ? (uint32_t)((uint64_t)(now * 1000.0 + n.phase) % len) : 0);
        m.poseAtTime(t);
        const std::vector<Mat4> &sk = m.skinningMatrices();
        simd_float4x4 *dst = (simd_float4x4 *)(base + i * kSlot);
        NSUInteger nb = MIN((NSUInteger)sk.size(), (NSUInteger)40);
        for (NSUInteger k = 0; k < nb; ++k) dst[k] = ToSimd(sk[k]);

        const Vec3 &a = _npcAnchor[n.type];
        Uniforms u;
        u.vp = vp;
        u.model = simd_mul(simd_mul(MTranslate(n.x, n.y, n.z), MRotZ(n.yaw + kModelYawOffset)),
                           MTranslate(-a.x, -a.y, -a.z));
        u.tint = (simd_float4){1, 1, 1, 1};
        u.misc = (simd_float4){[_npcTexs objectAtIndex:n.type] != _white ? 1.0f : 0.0f, 0, 0, 0};
        [enc setVertexBuffer:_npcVBs[n.type] offset:0 atIndex:0];
        [enc setVertexBytes:&u length:sizeof(u) atIndex:1];
        [enc setVertexBuffer:_npcBones offset:i * kSlot atIndex:2];
        [enc setFragmentTexture:_npcTexs[n.type] atIndex:0];
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
        [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:_npcIndexCount[n.type]
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:_npcIBs[n.type]
                 indexBufferOffset:0];
    }
}

// ------------------------------------------------------------------ input ---
- (void)touchBegin:(CGPoint)p {
    _game->touchBegin(p.x, p.y);
    CGFloat half = UIScreen.mainScreen.bounds.size.width * 0.5;
    if (p.x < half && _moveTouchActive < 0) {
        _moveTouchActive = 1;
        _moveOriginX = p.x;
        _moveOriginY = p.y;
        _stickX = _stickY = 0;
    } else {
        _lookTouchActive = 1;
        _lookLastX = p.x;
        _lookLastY = p.y;
    }
}

- (void)touchMove:(CGPoint)p {
    _game->touchMove(p.x, p.y);
    CGFloat half = UIScreen.mainScreen.bounds.size.width * 0.5;
    if (p.x < half && _moveTouchActive > 0) {
        const float radius = 70.0f;
        float dx = ((float)p.x - _moveOriginX) / radius;
        float dy = -((float)p.y - _moveOriginY) / radius;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 1.0f) { dx /= len; dy /= len; }
        _stickX = dx;
        _stickY = dy;
    } else if (_lookTouchActive > 0) {
        _camYaw -= ((float)p.x - _lookLastX) * 0.009f;
        _camPitch = fmaxf(-0.10f, fminf(0.85f, _camPitch - ((float)p.y - _lookLastY) * 0.006f));
        _lookLastX = p.x;
        _lookLastY = p.y;
    }
}

- (void)touchEnd:(CGPoint)p {
    _game->touchEnd(p.x, p.y);
    CGFloat half = UIScreen.mainScreen.bounds.size.width * 0.5;
    if (p.x < half) { _moveTouchActive = -1; _stickX = _stickY = 0; }
    else            { _lookTouchActive = -1; }
}

- (void)accelerometerX:(float)x y:(float)y z:(float)z { _game->accelerometer(x, y, z); }
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {}

@end
