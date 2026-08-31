#include "BDAEModel.hpp"
#include <cstring>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <limits>
#include <map>

namespace bdae {

// =============================================================== tiny math ===
Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int i = 0; i < 4; ++i) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + i] * b.m[c * 4 + k];
            r.m[c * 4 + i] = s;
        }
    return r;
}

Vec3 transformPoint(const Mat4& m, const Vec3& p) {
    return { m.m[0]*p.x + m.m[4]*p.y + m.m[8]*p.z  + m.m[12],
             m.m[1]*p.x + m.m[5]*p.y + m.m[9]*p.z  + m.m[13],
             m.m[2]*p.x + m.m[6]*p.y + m.m[10]*p.z + m.m[14] };
}

// IMPORTANT: BDAE stores node rotations in the 3ds Max / row-vector sense.
// Building the matrix from the *conjugate* of the stored quaternion is what
// makes node world transforms agree with the file's own inverse bind matrices
// (verified: max abs error 6.0e-4 over all 38 Spider-Man joints).
Mat4 trs(const Vec3& t, const Quat& qin, const Vec3& s) {
    const float x = -qin.x, y = -qin.y, z = -qin.z, w = qin.w;
    Mat4 m;
    m(0,0) = 1 - 2*(y*y + z*z); m(0,1) = 2*(x*y - z*w);     m(0,2) = 2*(x*z + y*w);
    m(1,0) = 2*(x*y + z*w);     m(1,1) = 1 - 2*(x*x + z*z); m(1,2) = 2*(y*z - x*w);
    m(2,0) = 2*(x*z - y*w);     m(2,1) = 2*(y*z + x*w);     m(2,2) = 1 - 2*(x*x + y*y);
    for (int r = 0; r < 3; ++r) { m(r,0) *= s.x; m(r,1) *= s.y; m(r,2) *= s.z; }
    m(0,3) = t.x; m(1,3) = t.y; m(2,3) = t.z;
    m(3,0) = m(3,1) = m(3,2) = 0; m(3,3) = 1;
    return m;
}

Quat slerp(const Quat& a, const Quat& bIn, float u) {
    Quat b = bIn;
    float d = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if (d < 0) { b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w; d = -d; }
    float k0, k1;
    if (d > 0.9995f) { k0 = 1 - u; k1 = u; }
    else {
        float th = std::acos(d), st = std::sin(th);
        k0 = std::sin((1 - u) * th) / st;
        k1 = std::sin(u * th) / st;
    }
    Quat r{ a.x*k0 + b.x*k1, a.y*k0 + b.y*k1, a.z*k0 + b.z*k1, a.w*k0 + b.w*k1 };
    float n = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z + r.w*r.w);
    if (n > 1e-8f) { r.x/=n; r.y/=n; r.z/=n; r.w/=n; }
    return r;
}

static Mat4 invertAffine(const Mat4& s) {
    // inverse of [R|t] with possibly non-uniform scale baked into R.
    float a[3][3];
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) a[r][c] = s(r,c);
    float det = a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])
              - a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0])
              + a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]);
    if (std::fabs(det) < 1e-20f) return Mat4::identity();
    float id = 1.0f / det;
    Mat4 o;
    o(0,0) =  (a[1][1]*a[2][2]-a[1][2]*a[2][1])*id;
    o(0,1) = -(a[0][1]*a[2][2]-a[0][2]*a[2][1])*id;
    o(0,2) =  (a[0][1]*a[1][2]-a[0][2]*a[1][1])*id;
    o(1,0) = -(a[1][0]*a[2][2]-a[1][2]*a[2][0])*id;
    o(1,1) =  (a[0][0]*a[2][2]-a[0][2]*a[2][0])*id;
    o(1,2) = -(a[0][0]*a[1][2]-a[0][2]*a[1][0])*id;
    o(2,0) =  (a[1][0]*a[2][1]-a[1][1]*a[2][0])*id;
    o(2,1) = -(a[0][0]*a[2][1]-a[0][1]*a[2][0])*id;
    o(2,2) =  (a[0][0]*a[1][1]-a[0][1]*a[1][0])*id;
    Vec3 t{ s(0,3), s(1,3), s(2,3) };
    o(0,3) = -(o(0,0)*t.x + o(0,1)*t.y + o(0,2)*t.z);
    o(1,3) = -(o(1,0)*t.x + o(1,1)*t.y + o(1,2)*t.z);
    o(2,3) = -(o(2,0)*t.x + o(2,1)*t.y + o(2,2)*t.z);
    return o;
}

// ============================================================ file plumbing ===
namespace {

struct Blob {
    std::vector<uint8_t> b;
    uint32_t headerSize = 0, fileSize = 0, numReloc = 0;
    uint32_t relocOff = 0, strOff = 0, dataOff = 0;

    bool ok(size_t o, size_t n) const { return o <= b.size() && n <= b.size() - o; }
    uint8_t  u8 (size_t o) const { return ok(o,1) ? b[o] : 0; }
    uint16_t u16(size_t o) const { uint16_t v = 0; if (ok(o,2)) std::memcpy(&v, b.data()+o, 2); return v; }
    uint32_t u32(size_t o) const { uint32_t v = 0; if (ok(o,4)) std::memcpy(&v, b.data()+o, 4); return v; }
    float    f32(size_t o) const { float v = 0; if (ok(o,4)) std::memcpy(&v, b.data()+o, 4); return v; }
    Vec3     v3 (size_t o) const { return { f32(o), f32(o+4), f32(o+8) }; }
    Quat     q4 (size_t o) const { return { f32(o), f32(o+4), f32(o+8), f32(o+12) }; }
    Mat4     m16(size_t o) const { Mat4 m; if (ok(o,64)) std::memcpy(m.m, b.data()+o, 64); return m; }

    std::string str(uint32_t o) const {
        if (o < strOff || o >= dataOff || o >= b.size()) return {};
        size_t e = o;
        while (e < b.size() && e < dataOff && b[e]) ++e;
        return std::string(reinterpret_cast<const char*>(b.data()) + o, e - o);
    }
    std::string strAtPtr(size_t fieldOff) const { return str(u32(fieldOff)); }

    bool load(const std::string& path, std::string& err) {
        std::ifstream f(path, std::ios::binary);
        if (!f) { err = "cannot open " + path; return false; }
        b.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        if (b.size() < 32 || std::memcmp(b.data(), "BRES", 4) != 0) { err = "not a BRES/BDAE file"; return false; }
        headerSize = u32(8);  fileSize = u32(12); numReloc = u32(16);
        relocOff   = u32(20); strOff   = u32(24); dataOff  = u32(28);
        if (fileSize != b.size()) { err = "fileSize mismatch"; return false; }
        if (relocOff + 4ull * numReloc != strOff) { err = "relocation table does not abut string table"; return false; }
        if (strOff > dataOff || dataOff > b.size()) { err = "bad section offsets"; return false; }
        return true;
    }
};

constexpr size_t kNodeStride = 80;

// Root library table (offsets relative to dataOff).  Verified layout:
//   +0   char*  version string ("0,0,0,324")
//   +16  count / +20  ptr   animation channels     (36 bytes each)
//   +28  count / +32  ptr   animation clips        (12 bytes each)
//   +52  count / +56  ptr   images   (textures)
//   +60  count / +64  ptr   effects
//   +68  count / +72  ptr   materials
//   +76  count / +80  ptr   geometries             (16 bytes each)
//   +84  count / +88  ptr   controllers (skin)
//   +108 count / +112 ptr   visual scenes
//   +116 count / +120 ptr   scene
enum : uint32_t {
    kAnimCount = 16, kAnimPtr = 20, kClipCount = 28, kClipPtr = 32,
    kImgCount = 52, kImgPtr = 56, kGeoCount = 76, kGeoPtr = 80,
    kCtrlCount = 84, kCtrlPtr = 88, kSceneCount = 108, kScenePtr = 112
};

} // namespace

// ================================================================ mesh load ===

static void readNodes(const Blob& f, std::vector<Node>& out, std::vector<uint32_t>& addrs,
                      uint32_t addr, int parent) {
    int self = static_cast<int>(out.size());
    out.emplace_back();
    addrs.push_back(addr);
    {
        Node n;
        n.id     = f.strAtPtr(addr + 0);
        n.name   = f.strAtPtr(addr + 4);
        n.sid    = f.strAtPtr(addr + 8);
        n.t      = f.v3(addr + 12);
        n.q      = f.q4(addr + 24);
        n.s      = f.v3(addr + 40);
        n.parent = parent;
        out[self] = std::move(n);
    }
    if (parent >= 0) out[parent].children.push_back(self);
    uint32_t nc = f.u32(addr + 56), cp = f.u32(addr + 60);
    if (nc > 4096) nc = 0;                      // sanity clamp
    for (uint32_t i = 0; i < nc; ++i)
        readNodes(f, out, addrs, cp + static_cast<uint32_t>(i * kNodeStride), self);
}

bool Model::loadMesh(const std::string& path, std::string& err) {
    Blob f;
    if (!f.load(path, err)) return false;
    const uint32_t D = f.dataOff;

    version = f.strAtPtr(D + 0);
    if (version != "0,0,0,324") { err = "unsupported BDAE version '" + version + "'"; return false; }

    // ---- textures
    uint32_t ic = f.u32(D + kImgCount), ip = f.u32(D + kImgPtr);
    for (uint32_t i = 0; i < ic && i < 256; ++i) {
        std::string s = f.strAtPtr(ip + i * 20);
        if (!s.empty()) textureNames.push_back(s);
        std::string p = f.strAtPtr(ip + i * 20 + 8);
        for (char& ch : p) if (ch == '\\') ch = '/';
        size_t sl = p.find_last_of('/');
        std::string base = (sl == std::string::npos) ? p : p.substr(sl + 1);
        imageFiles.push_back(base.empty() ? s : base);
    }

    // ---- material -> texture binding (effect +76/+80 uv sets, +84/+88 image indices)
    struct Binding { std::string diffuse, lightmap; int duv = 0, luv = 1; };
    std::map<std::string, Binding> bindings;   // keyed by material name
    {
        uint32_t ec = f.u32(D + 60), ep = f.u32(D + 64);
        uint32_t mc = f.u32(D + 68), mp = f.u32(D + 72);
        uint32_t gp = f.u32(D + 80);
        std::map<std::string, Binding> byEffect;
        for (uint32_t i = 0; i < ec && i < 4096; ++i) {
            uint32_t e = ep + i * 92;
            uint32_t nImg = f.u32(e + 84), pImg = f.u32(e + 88);
            uint32_t nUv = f.u32(e + 76), pUv = f.u32(e + 80);
            if (!nImg || nImg > 8) continue;
            Binding b;
            for (uint32_t k = 0; k < nImg; ++k) {
                uint32_t idx = f.u32(pImg + k * 4);
                if (idx >= imageFiles.size()) continue;
                int uv = (nUv > k && nUv < 8) ? (int)f.u32(pUv + k * 4) : (int)k;
                std::string nm = imageFiles[idx];
                std::string low = nm; for (char& c : low) c = (char)std::tolower((unsigned char)c);
                if (low.find("lightmap") != std::string::npos) { b.lightmap = nm; b.luv = uv; }
                else if (b.diffuse.empty()) { b.diffuse = nm; b.duv = uv; }
            }
            byEffect[f.strAtPtr(e)] = b;
        }
        uint32_t stride = (mc && gp > mp && (gp - mp) % mc == 0) ? (gp - mp) / mc : 40;
        for (uint32_t i = 0; i < mc && i < 4096; ++i) {
            uint32_t me = mp + i * stride;
            std::string url = f.strAtPtr(me + 12);
            if (!url.empty() && url[0] == '#') url = url.substr(1);
            auto it = byEffect.find(url);
            if (it != byEffect.end()) bindings[f.strAtPtr(me)] = it->second;
        }
    }

    // ---- scene nodes
    std::vector<uint32_t> nodeAddr;
    uint32_t sc = f.u32(D + kSceneCount), sp = f.u32(D + kScenePtr);
    if (sc && sp) {
        uint32_t nroot = f.u32(sp + 8), roots = f.u32(sp + 12);
        if (nroot > 4096) nroot = 0;
        for (uint32_t i = 0; i < nroot; ++i)
            readNodes(f, nodes, nodeAddr, roots + static_cast<uint32_t>(i * kNodeStride), -1);
    }

    // ---- geometry
    // geometry entry (16B): +0 id, +4 name, +8 ?, +12 ptr mesh
    // mesh (>=44B):        +4 vertexCount, +8 ptr vertexDesc, +12 subMeshCount,
    //                      +16 ptr subMeshTable, +20 bboxMin[3], +32 bboxMax[3]
    // vertexDesc:          +0 stride, +4 attrCount, +8 ptr attrOffsets, +28 ptr vertexData
    // subMesh (32B):       +24 indexCount, +28 ptr indices (uint16)
    // Vertex attributes are described by three parallel arrays:
    //   vd+8  -> uint32 byteOffset[attrCount]
    //   vd+16 -> uint32 dataType[attrCount]     (6 = float, 1 = packed ubyte4 colour)
    //   vd+24 -> uint32 [attrCount]             (always 0 in this title)
    // The component count is not stored: it is the gap to the next attribute in
    // offset order (or to the stride for the last one).  Verified against
    // spiderman_mesh (0/12/24, all float), Nav01 and geometry01 (0/20/12 with a
    // ubyte4 colour and no normals) and collision01 (6 attributes, stride 52).
    uint32_t gc = f.u32(D + kGeoCount), gp = f.u32(D + kGeoPtr);
    std::vector<uint32_t> geoEntryAddr;
    for (uint32_t gi = 0; gi < gc; ++gi) {
        uint32_t ge = gp + gi * 16;
        uint32_t md = f.u32(ge + 12);
        if (!f.ok(md, 44)) continue;
        Mesh m;
        m.id   = f.strAtPtr(ge + 0);
        m.name = f.strAtPtr(ge + 4);
        if (m.name.empty()) m.name = m.id;
        m.bboxMin = f.v3(md + 20);
        m.bboxMax = f.v3(md + 32);

        uint32_t vc = f.u32(md + 4), vd = f.u32(md + 8);
        uint32_t smc = f.u32(md + 12), smt = f.u32(md + 16);
        if (!vc || !f.ok(vd, 32)) continue;

        uint32_t stride = f.u32(vd + 0), ac = f.u32(vd + 4), ao = f.u32(vd + 8);
        uint32_t tc = f.u32(vd + 12), to = f.u32(vd + 16), vdata = f.u32(vd + 28);
        if (stride < 12 || stride > 256 || ac < 1 || ac > 32) continue;
        if (!f.ok(ao, ac * 4ull) || !f.ok(vdata, static_cast<size_t>(vc) * stride)) continue;

        struct Attr { uint32_t off, type, bytes; };
        std::vector<Attr> attrs(ac);
        std::vector<uint32_t> sorted(ac);
        for (uint32_t a = 0; a < ac; ++a) {
            attrs[a].off  = f.u32(ao + a * 4);
            attrs[a].type = (tc == ac && f.ok(to, ac * 4ull)) ? f.u32(to + a * 4) : 6u;
            sorted[a] = attrs[a].off;
        }
        std::sort(sorted.begin(), sorted.end());
        bool sane = true;
        for (uint32_t a = 0; a < ac; ++a) {
            uint32_t next = stride;
            for (uint32_t s : sorted) if (s > attrs[a].off) { next = s; break; }
            if (next <= attrs[a].off || next > stride) { sane = false; break; }
            attrs[a].bytes = next - attrs[a].off;
        }
        if (!sane || attrs[0].bytes < 12) continue;

        uint32_t po = attrs[0].off;
        uint32_t no = 0xffffffffu, uo = 0xffffffffu, co = 0xffffffffu, uo2 = 0xffffffffu;
        for (uint32_t a = 1; a < ac; ++a) {
            if (attrs[a].type == 1 && attrs[a].bytes == 4) { if (co == 0xffffffffu) co = attrs[a].off; continue; }
            if (attrs[a].type != 6) continue;
            if (no == 0xffffffffu && attrs[a].bytes >= 12) { no = attrs[a].off; continue; }
            if (uo == 0xffffffffu && attrs[a].bytes == 8)  { uo = attrs[a].off; continue; }
            if (uo2 == 0xffffffffu && attrs[a].bytes == 8) { uo2 = attrs[a].off; }
        }
        m.sourceStride = stride;
        m.hadNormals = (no != 0xffffffffu);
        m.hadUVs     = (uo != 0xffffffffu);

        m.vertices.resize(vc);
        for (uint32_t i = 0; i < vc; ++i) {
            size_t v = vdata + static_cast<size_t>(i) * stride;
            Vertex& d = m.vertices[i];
            d.px = f.f32(v + po); d.py = f.f32(v + po + 4); d.pz = f.f32(v + po + 8);
            if (m.hadNormals) { d.nx = f.f32(v + no); d.ny = f.f32(v + no + 4); d.nz = f.f32(v + no + 8); }
            else              { d.nx = d.ny = 0; d.nz = 0; }
            if (m.hadUVs)     { d.u = f.f32(v + uo); d.v = f.f32(v + uo + 4); }
            if (co != 0xffffffffu)
                for (int k = 0; k < 4; ++k) d.color[k] = f.u8(v + co + k);
            if (uo2 != 0xffffffffu) { d.u2 = f.f32(v + uo2); d.v2 = f.f32(v + uo2 + 4); }
            else { d.u2 = d.u; d.v2 = d.v; }
        }
        // Submesh records are 64 bytes:
        //   +4 material name, +8 triangleCount, +24 indexCount, +28 index pointer,
        //   +36..+59 per-submesh bbox.  (A 32-byte stride happens to work for
        //   single-submesh models like Spider-Man, which is why it went unnoticed.)
        for (uint32_t k = 0; k < smc && k < 8192; ++k) {
            size_t se = smt + static_cast<size_t>(k) * 64;
            uint32_t tris = f.u32(se + 8);
            uint32_t icnt = f.u32(se + 24), ipos = f.u32(se + 28);
            if (icnt != tris * 3u) continue;          // cross-check both counters
            if (!icnt || icnt > 3u * 65536u || !f.ok(ipos, static_cast<size_t>(icnt) * 2)) continue;
            SubMesh s; s.firstIndex = static_cast<uint32_t>(m.indices.size()); s.indexCount = icnt;
            s.material = f.strAtPtr(se + 4);
            auto bit = bindings.find(s.material);
            if (bit != bindings.end()) {
                s.diffuse = bit->second.diffuse; s.lightmap = bit->second.lightmap;
                s.diffuseUv = bit->second.duv; s.lightmapUv = bit->second.luv;
            }
            for (uint32_t j = 0; j < icnt; ++j) {
                uint16_t ix = f.u16(ipos + j * 2);
                m.indices.push_back(ix < vc ? ix : 0);
            }
            m.subMeshes.push_back(s);
        }
        if (m.vertices.empty() || m.indices.empty()) continue;

        if (!m.hadNormals) {           // level geometry is vertex-lit and ships no normals
            for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
                Vertex& a = m.vertices[m.indices[i]];
                Vertex& b = m.vertices[m.indices[i + 1]];
                Vertex& cv = m.vertices[m.indices[i + 2]];
                float ux = b.px - a.px, uy = b.py - a.py, uz = b.pz - a.pz;
                float vx = cv.px - a.px, vy = cv.py - a.py, vz = cv.pz - a.pz;
                float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
                a.nx += nx; a.ny += ny; a.nz += nz;
                b.nx += nx; b.ny += ny; b.nz += nz;
                cv.nx += nx; cv.ny += ny; cv.nz += nz;
            }
            for (Vertex& v : m.vertices) {
                float l = std::sqrt(v.nx*v.nx + v.ny*v.ny + v.nz*v.nz);
                if (l > 1e-8f) { v.nx /= l; v.ny /= l; v.nz /= l; } else { v.nx = 0; v.ny = 0; v.nz = 1; }
            }
        }
        geoEntryAddr.push_back(ge);
        meshes.push_back(std::move(m));
    }

    // Resolve <instance_geometry url="#id"> on scene nodes.  The instance record
    // is a small tagged blob; the URL is the first '#'-prefixed string in it.
    for (size_t ni = 0; ni < nodes.size(); ++ni) {
        uint32_t addr = nodeAddr[ni];
        uint32_t icnt = f.u32(addr + 64), iptr = f.u32(addr + 68);
        if (!icnt || icnt > 64 || !f.ok(iptr, 64)) continue;
        for (uint32_t k = 0; k < icnt; ++k) {
            uint32_t rec = iptr + k * 40;
            for (uint32_t w = 0; w < 16; ++w) {
                std::string s = f.str(f.u32(rec + w * 4));
                if (s.size() < 2 || s[0] != '#') continue;
                std::string want = s.substr(1);
                for (size_t mi = 0; mi < meshes.size(); ++mi)
                    if (meshes[mi].id == want) { instances.push_back({ static_cast<int>(ni), static_cast<int>(mi) }); break; }
                break;
            }
        }
    }

    if (meshes.empty()) { err = "no renderable geometry in " + path; return false; }

    // ---- skin controller
    // controller: +4 id, +12 source url, +16 bindShape[16],
    //             +80 jointCount / +84 ptr joint sid array,
    //             +88 ibmFloatCount / +92 ptr inverse bind matrices,
    //             +96 weightCount / +100 ptr weight values,
    //             +104 vertexCount / +108 ptr uint8 influence counts,
    //             +112 shortCount / +116 ptr (jointIdx,weightIdx) uint16 pairs
    uint32_t cc = f.u32(D + kCtrlCount), cp = f.u32(D + kCtrlPtr);
    if (cc && cp && f.ok(cp, 128)) {
        Skin& sk = skin;
        sk.bindShape = f.m16(cp + 16);
        uint32_t nj = f.u32(cp + 80), jn = f.u32(cp + 84);
        uint32_t nibmF = f.u32(cp + 88), ibmp = f.u32(cp + 92);
        uint32_t nw = f.u32(cp + 96), wp = f.u32(cp + 100);
        uint32_t nv = f.u32(cp + 104), vcp = f.u32(cp + 108);
        uint32_t nsh = f.u32(cp + 112), vp = f.u32(cp + 116);
        bool good = nj && nj <= 512 && nibmF == nj * 16 &&
                    f.ok(jn, nj * 4ull) && f.ok(ibmp, nj * 64ull) &&
                    f.ok(wp, nw * 4ull) && f.ok(vcp, nv) && f.ok(vp, nsh * 2ull);
        if (good) {
            sk.jointSid.resize(nj); sk.inverseBind.resize(nj); sk.jointNode.assign(nj, -1);
            for (uint32_t i = 0; i < nj; ++i) {
                sk.jointSid[i] = f.str(f.u32(jn + i * 4));
                sk.inverseBind[i] = f.m16(ibmp + i * 64);
            }
            std::map<std::string, int> bySid;
            for (size_t i = 0; i < nodes.size(); ++i)
                if (!nodes[i].sid.empty()) bySid[nodes[i].sid] = static_cast<int>(i);
            for (uint32_t i = 0; i < nj; ++i) {
                auto it = bySid.find(sk.jointSid[i]);
                if (it != bySid.end()) sk.jointNode[i] = it->second;
            }
            std::vector<float> weights(nw);
            for (uint32_t i = 0; i < nw; ++i) weights[i] = f.f32(wp + i * 4);

            // distribute influences onto the first mesh's vertices
            Mesh& m = meshes.front();
            size_t cursor = 0;
            uint32_t limit = std::min<uint32_t>(nv, static_cast<uint32_t>(m.vertices.size()));
            for (uint32_t vi = 0; vi < limit; ++vi) {
                uint32_t n = f.u8(vcp + vi);
                sk.maxInfluences = std::max(sk.maxInfluences, n);
                float total = 0;
                uint32_t used = 0;
                for (uint32_t k = 0; k < n; ++k) {
                    if (cursor + 1 >= nsh) break;
                    uint16_t j  = f.u16(vp + static_cast<uint32_t>(cursor) * 2); ++cursor;
                    uint16_t wi = f.u16(vp + static_cast<uint32_t>(cursor) * 2); ++cursor;
                    if (used < 4 && j < nj && wi < nw) {
                        m.vertices[vi].bone[used]   = static_cast<uint8_t>(j);
                        m.vertices[vi].weight[used] = weights[wi];
                        total += weights[wi];
                        ++used;
                    }
                }
                if (total > 1e-6f)
                    for (uint32_t k = 0; k < used; ++k) m.vertices[vi].weight[k] /= total;
                else
                    m.vertices[vi].weight[0] = 1.0f;
            }
            sk.valid = true;
        }
    }

    poseBind();
    return true;
}

// =========================================================== animation load ===
bool Model::loadAnimation(const std::string& path, std::string& err) {
    Blob f;
    if (!f.load(path, err)) return false;
    const uint32_t D = f.dataOff;
    if (f.strAtPtr(D + 0) != "0,0,0,324") { err = "unsupported animation BDAE version"; return false; }

    std::map<std::string, int> byId;
    for (size_t i = 0; i < nodes.size(); ++i) byId[nodes[i].id] = static_cast<int>(i);

    // channel entry (36B): +0 name "<node-id>-rotation|-translation", +8 ptr source record
    // source record:       +4 keyCount, +8 ptr uint32 times(ms), +16 totalFloats, +20 ptr float values
    uint32_t ac = f.u32(D + kAnimCount), ap = f.u32(D + kAnimPtr);
    for (uint32_t i = 0; i < ac && i < 4096; ++i) {
        uint32_t e = ap + i * 36;
        std::string name = f.strAtPtr(e + 0);
        uint32_t rec = f.u32(e + 8);
        if (name.empty() || !f.ok(rec, 28)) continue;

        uint32_t keys = f.u32(rec + 4), tp = f.u32(rec + 8);
        uint32_t floats = f.u32(rec + 16), vp = f.u32(rec + 20);
        if (!keys || !floats || floats % keys) continue;
        uint32_t comps = floats / keys;
        if (comps != 1 && comps != 3 && comps != 4) continue;
        if (!f.ok(tp, keys * 4ull) || !f.ok(vp, floats * 4ull)) continue;

        Channel c;
        size_t dash = name.rfind('-');
        c.targetNodeId = (dash == std::string::npos) ? name : name.substr(0, dash);
        std::string kind = (dash == std::string::npos) ? "" : name.substr(dash + 1);
        c.isRotation = (kind == "rotation");
        c.components = comps;
        auto it = byId.find(c.targetNodeId);
        c.node = (it == byId.end()) ? -1 : it->second;
        c.timeMs.resize(keys);
        for (uint32_t k = 0; k < keys; ++k) c.timeMs[k] = f.u32(tp + k * 4);
        c.values.resize(floats);
        for (uint32_t k = 0; k < floats; ++k) c.values[k] = f.f32(vp + k * 4);
        channels.push_back(std::move(c));
    }

    // clip entry (12B): +0 name, +4 startMs, +8 endMs
    uint32_t cc = f.u32(D + kClipCount), cp = f.u32(D + kClipPtr);
    for (uint32_t i = 0; i < cc && i < 65536; ++i) {
        uint32_t e = cp + i * 12;
        Clip c; c.name = f.strAtPtr(e + 0); c.startMs = f.u32(e + 4); c.endMs = f.u32(e + 8);
        if (!c.name.empty() && c.endMs >= c.startMs) clips.push_back(std::move(c));
    }
    if (channels.empty()) { err = "no animation channels in " + path; return false; }
    return true;
}

const Clip* Model::findClip(const std::string& name) const {
    for (const auto& c : clips) if (c.name == name) return &c;
    return nullptr;
}

// ================================================================== posing ===
void Model::poseBind() {
    restT_.resize(nodes.size()); restQ_.resize(nodes.size()); restS_.resize(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) { restT_[i] = nodes[i].t; restQ_[i] = nodes[i].q; restS_[i] = nodes[i].s; }
    rebuildWorld();
    rebuildSkinning();
}

static void sample(const Channel& c, uint32_t t, float* out) {
    const size_t n = c.timeMs.size();
    size_t hi = static_cast<size_t>(std::lower_bound(c.timeMs.begin(), c.timeMs.end(), t) - c.timeMs.begin());
    if (hi == 0) { std::memcpy(out, c.values.data(), c.components * sizeof(float)); return; }
    if (hi >= n) { std::memcpy(out, c.values.data() + (n - 1) * c.components, c.components * sizeof(float)); return; }
    size_t lo = hi - 1;
    uint32_t t0 = c.timeMs[lo], t1 = c.timeMs[hi];
    float u = (t1 > t0) ? float(t - t0) / float(t1 - t0) : 0.0f;
    const float* a = c.values.data() + lo * c.components;
    const float* b = c.values.data() + hi * c.components;
    if (c.isRotation && c.components == 4) {
        Quat q = slerp({a[0],a[1],a[2],a[3]}, {b[0],b[1],b[2],b[3]}, u);
        out[0] = q.x; out[1] = q.y; out[2] = q.z; out[3] = q.w;
    } else {
        for (uint32_t k = 0; k < c.components; ++k) out[k] = a[k] + (b[k] - a[k]) * u;
    }
}

static void gatherPose(const Model& m, uint32_t t, const std::vector<Vec3>& restT,
                       std::vector<Vec3>& T, std::vector<Quat>& Q) {
    float buf[4];
    for (const Channel& c : m.channels) {
        if (c.node < 0 || c.node >= static_cast<int>(m.nodes.size())) continue;
        sample(c, t, buf);
        if (c.isRotation) {
            if (c.components == 4) Q[c.node] = { buf[0], buf[1], buf[2], buf[3] };
        } else if (c.components == 3) {
            T[c.node] = { buf[0], buf[1], buf[2] };
        } else if (c.components == 1) {
            // Single-component translation track: the exporter emitted only the
            // axis that actually animates.  Pick the axis whose rest value is
            // closest, so the other two keep their bind values.
            Vec3 r = restT[c.node];
            float d0 = std::fabs(r.x - buf[0]), d1 = std::fabs(r.y - buf[0]), d2 = std::fabs(r.z - buf[0]);
            if (d2 <= d0 && d2 <= d1) T[c.node].z = buf[0];
            else if (d1 <= d0)        T[c.node].y = buf[0];
            else                      T[c.node].x = buf[0];
        }
    }
}

void Model::applyPose(const std::vector<Vec3>& T, const std::vector<Quat>& Q) {
    for (size_t i = 0; i < nodes.size(); ++i) { nodes[i].t = T[i]; nodes[i].q = Q[i]; }
    rebuildWorld();
    rebuildSkinning();
    for (size_t i = 0; i < nodes.size(); ++i) { nodes[i].t = restT_[i]; nodes[i].q = restQ_[i]; }
}

void Model::poseAtTime(uint32_t timeMs) {
    if (restT_.size() != nodes.size()) poseBind();
    std::vector<Vec3> T = restT_;
    std::vector<Quat> Q = restQ_;
    gatherPose(*this, timeMs, restT_, T, Q);
    applyPose(T, Q);
}

void Model::poseBlend(uint32_t aMs, uint32_t bMs, float w) {
    if (restT_.size() != nodes.size()) poseBind();
    std::vector<Vec3> Ta = restT_, Tb = restT_;
    std::vector<Quat> Qa = restQ_, Qb = restQ_;
    gatherPose(*this, aMs, restT_, Ta, Qa);
    gatherPose(*this, bMs, restT_, Tb, Qb);
    w = w < 0 ? 0 : (w > 1 ? 1 : w);
    for (size_t i = 0; i < nodes.size(); ++i) {
        Ta[i] = { Ta[i].x + (Tb[i].x - Ta[i].x) * w,
                  Ta[i].y + (Tb[i].y - Ta[i].y) * w,
                  Ta[i].z + (Tb[i].z - Ta[i].z) * w };
        Qa[i] = slerp(Qa[i], Qb[i], w);
    }
    applyPose(Ta, Qa);
}

void Model::rebuildWorld() {
    world_.assign(nodes.size(), Mat4::identity());
    for (size_t i = 0; i < nodes.size(); ++i) {
        Mat4 local = trs(nodes[i].t, nodes[i].q, nodes[i].s);
        world_[i] = (nodes[i].parent < 0) ? local : mul(world_[nodes[i].parent], local);
    }
}

void Model::rebuildSkinning() {
    if (!skin.valid) { skinMat_.clear(); return; }
    skinMat_.resize(skin.jointNode.size());
    for (size_t j = 0; j < skin.jointNode.size(); ++j) {
        int n = skin.jointNode[j];
        Mat4 w = (n >= 0 && n < static_cast<int>(world_.size())) ? world_[n] : Mat4::identity();
        skinMat_[j] = mul(mul(w, skin.inverseBind[j]), skin.bindShape);
    }
}

Vec3 skinnedAnchor(Model& m, uint32_t timeMs) {
    if (m.meshes.empty() || !m.skin.valid) return {0, 0, 0};
    m.poseAtTime(timeMs);
    const Mesh& mesh = m.meshes.front();
    const std::vector<Mat4>& S = m.skinningMatrices();
    double sx = 0, sy = 0; float minZ = 1e30f; size_t n = 0;
    for (const Vertex& v : mesh.vertices) {
        Vec3 acc{0, 0, 0};
        for (int k = 0; k < 4; ++k) {
            if (v.weight[k] <= 0) continue;
            Vec3 p = transformPoint(S[v.bone[k]], Vec3{v.px, v.py, v.pz});
            acc.x += p.x * v.weight[k]; acc.y += p.y * v.weight[k]; acc.z += p.z * v.weight[k];
        }
        sx += acc.x; sy += acc.y; minZ = std::min(minZ, acc.z); ++n;
    }
    if (!n) return {0, 0, 0};
    return { static_cast<float>(sx / n), static_cast<float>(sy / n), minZ };
}

// ============================================================== clip player ===
void ClipPlayer::advance(float dtSeconds) {
    if (!clip) return;
    float len = static_cast<float>(clip->endMs - clip->startMs);
    localMs += dtSeconds * 1000.0f;
    if (len <= 0) { localMs = 0; return; }
    if (loop) { while (localMs >= len) localMs -= len; }
    else if (localMs > len) localMs = len;
}
uint32_t ClipPlayer::timelineMs() const {
    return clip ? clip->startMs + static_cast<uint32_t>(localMs) : 0;
}
bool ClipPlayer::finished() const {
    return clip && !loop && localMs >= static_cast<float>(clip->endMs - clip->startMs);
}

} // namespace bdae
