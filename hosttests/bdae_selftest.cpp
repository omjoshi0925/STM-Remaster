// bdae_selftest.cpp - runs the loader against the real Total Mayhem assets and
// asserts structural invariants that would break if any offset were wrong.
#include "BDAEModel.hpp"
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

using namespace bdae;

static int failures = 0;
static void check(bool cond, const char* what, const std::string& detail = {}) {
    std::printf("  [%s] %s%s%s\n", cond ? "PASS" : "FAIL", what,
                detail.empty() ? "" : "  -> ", detail.c_str());
    if (!cond) ++failures;
}

static float boneLength(const Model& m, int a, int b) {
    const auto& W = m.worldTransforms();
    Vec3 pa{ W[a].m[12], W[a].m[13], W[a].m[14] };
    Vec3 pb{ W[b].m[12], W[b].m[13], W[b].m[14] };
    float dx = pa.x - pb.x, dy = pa.y - pb.y, dz = pa.z - pb.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

int findNode(const Model& m, const std::string& name) {
    for (size_t i = 0; i < m.nodes.size(); ++i) if (m.nodes[i].name == name) return int(i);
    return -1;
}

int main(int argc, char** argv) {
    std::string meshPath = argc > 1 ? argv[1] : "ex/entities/meshes_bin/spiderman_mesh.bdae";
    std::string animPath = argc > 2 ? argv[2] : "ex/entities/meshes_bin/spiderman_anim.bdae";

    Model model;
    std::string err;

    std::printf("== load mesh: %s\n", meshPath.c_str());
    if (!model.loadMesh(meshPath, err)) { std::printf("  FATAL: %s\n", err.c_str()); return 1; }

    const Mesh& mesh = model.meshes.front();
    std::printf("  version=%s nodes=%zu meshes=%zu verts=%zu tris=%zu submeshes=%zu stride=%u\n",
                model.version.c_str(), model.nodes.size(), model.meshes.size(),
                mesh.vertices.size(), mesh.indices.size() / 3, mesh.subMeshes.size(), mesh.sourceStride);
    std::printf("  textures:");
    for (auto& t : model.textureNames) std::printf(" %s", t.c_str());
    std::printf("\n");

    check(model.version == "0,0,0,324", "version is 0,0,0,324");
    check(mesh.vertices.size() == 641, "spiderman mesh has 641 vertices");
    check(mesh.indices.size() / 3 == 920, "spiderman mesh has 920 triangles");
    check(mesh.sourceStride == 32, "vertex stride is 32 bytes");
    check(model.nodes.size() == 46, "scene has 46 nodes");

    // --- skeleton integrity
    int nJoint = 0;
    for (auto& n : model.nodes) if (!n.sid.empty()) ++nJoint;
    check(nJoint == 38, "38 nodes carry a Bone sid");
    check(model.skin.valid, "skin controller parsed");
    check(model.skin.jointNode.size() == 38, "skin has 38 joints");
    int unresolved = 0;
    for (int n : model.skin.jointNode) if (n < 0) ++unresolved;
    check(unresolved == 0, "every skin joint resolves to a scene node");
    check(model.skin.maxInfluences <= 4, "at most 4 influences per vertex",
          "observed max = " + std::to_string(model.skin.maxInfluences));

    // --- weights normalised
    float worstW = 0;
    int weighted = 0;
    for (const Vertex& v : mesh.vertices) {
        float s = v.weight[0] + v.weight[1] + v.weight[2] + v.weight[3];
        worstW = std::max(worstW, std::fabs(s - 1.0f));
        if (s > 0.5f) ++weighted;
    }
    check(worstW < 1e-4f, "all vertex weights sum to 1.0",
          "worst deviation " + std::to_string(worstW));
    check(weighted == (int)mesh.vertices.size(), "every vertex has skin weights");

    // --- inverse bind matrices agree with the rest pose we reconstructed
    model.poseBind();
    float worstIbm = 0;
    for (size_t j = 0; j < model.skin.jointNode.size(); ++j) {
        const Mat4& s = model.skinningMatrices()[j];
        // world*IBM*bindShape at the bind pose must be (almost) identity
        // At the bind pose world*IBM must be identity, so the product with the
        // bind-shape matrix must equal the bind-shape matrix itself.
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                worstIbm = std::max(worstIbm, std::fabs(s(r, c) - model.skin.bindShape(r, c)));
    }
    check(worstIbm < 5e-3f, "world*inverseBind*bindShape == bindShape at the bind pose",
          "worst element error " + std::to_string(worstIbm));

    // --- animation
    std::printf("== load animation: %s\n", animPath.c_str());
    if (!model.loadAnimation(animPath, err)) { std::printf("  FATAL: %s\n", err.c_str()); return 1; }
    std::printf("  channels=%zu clips=%zu\n", model.channels.size(), model.clips.size());
    int rot = 0, tr = 0, bound = 0;
    for (auto& c : model.channels) { (c.isRotation ? rot : tr)++; if (c.node >= 0) ++bound; }
    std::printf("  rotation=%d translation=%d boundToNodes=%d\n", rot, tr, bound);
    check(model.channels.size() == 46, "46 animation channels");
    check(bound == (int)model.channels.size(), "every channel binds to a scene node");
    check(model.clips.size() == 242, "242 animation clips");

    for (const char* nm : {"idle_stand", "run", "walk", "punch_right_to_idle"}) {
        const Clip* c = model.findClip(nm);
        check(c != nullptr, (std::string("clip present: ") + nm).c_str(),
              c ? (std::to_string(c->startMs) + ".." + std::to_string(c->endMs) + " ms") : "");
    }

    // --- bone lengths must be rigid across the whole timeline
    struct Pair { const char* a; const char* b; };
    const Pair pairs[] = {
        {"Bip01_L_Thigh", "Bip01_L_Calf"}, {"Bip01_L_Calf", "Bip01_L_Foot"},
        {"Bip01_Spine", "Bip01_Spine1"},   {"Bip01_R_UpperArm", "Bip01_R_Forearm"},
        {"Bip01_Neck", "Bip01_Head"},
    };
    model.poseBind();
    std::vector<float> ref;
    std::vector<std::pair<int,int>> idx;
    for (auto& p : pairs) {
        int a = findNode(model, p.a), b = findNode(model, p.b);
        idx.push_back({a, b});
        ref.push_back(boneLength(model, a, b));
    }
    const Clip* idle = model.findClip("idle_stand");
    const Clip* run  = model.findClip("run");
    float worstRigid = 0;
    for (const Clip* c : {idle, run}) {
        if (!c) continue;
        for (uint32_t t = c->startMs; t <= c->endMs; t += 33) {
            model.poseAtTime(t);
            for (size_t i = 0; i < idx.size(); ++i) {
                float L = boneLength(model, idx[i].first, idx[i].second);
                if (ref[i] > 1e-3f) worstRigid = std::max(worstRigid, std::fabs(L - ref[i]) / ref[i]);
            }
        }
    }
    check(worstRigid < 1e-3f, "bone lengths stay rigid through idle_stand and run",
          "worst relative drift " + std::to_string(worstRigid));

    // --- skinned bounding box must stay near the authored one
    auto skinnedBounds = [&](Vec3& lo, Vec3& hi) {
        lo = { 1e30f, 1e30f, 1e30f }; hi = { -1e30f, -1e30f, -1e30f };
        const auto& S = model.skinningMatrices();
        for (const Vertex& v : mesh.vertices) {
            Vec3 acc{0,0,0};
            for (int k = 0; k < 4; ++k) {
                if (v.weight[k] <= 0) continue;
                Vec3 p = transformPoint(S[v.bone[k]], Vec3{v.px, v.py, v.pz});
                acc.x += p.x * v.weight[k]; acc.y += p.y * v.weight[k]; acc.z += p.z * v.weight[k];
            }
            lo.x = std::min(lo.x, acc.x); lo.y = std::min(lo.y, acc.y); lo.z = std::min(lo.z, acc.z);
            hi.x = std::max(hi.x, acc.x); hi.y = std::max(hi.y, acc.y); hi.z = std::max(hi.z, acc.z);
        }
    };
    Vec3 lo, hi;
    model.poseBind(); skinnedBounds(lo, hi);
    std::printf("  bind-pose skinned bbox   (%.2f %.2f %.2f) .. (%.2f %.2f %.2f)\n",
                lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
    std::printf("  authored mesh bbox       (%.2f %.2f %.2f) .. (%.2f %.2f %.2f)\n",
                mesh.bboxMin.x, mesh.bboxMin.y, mesh.bboxMin.z,
                mesh.bboxMax.x, mesh.bboxMax.y, mesh.bboxMax.z);
    float boxErr = std::max({ std::fabs(lo.x - mesh.bboxMin.x), std::fabs(lo.y - mesh.bboxMin.y),
                              std::fabs(lo.z - mesh.bboxMin.z), std::fabs(hi.x - mesh.bboxMax.x),
                              std::fabs(hi.y - mesh.bboxMax.y), std::fabs(hi.z - mesh.bboxMax.z) });
    check(boxErr < 1.0f, "bind-pose skinned bbox matches the authored bbox",
          "worst axis error " + std::to_string(boxErr));

    if (idle) {
        model.poseAtTime(idle->startMs + (idle->endMs - idle->startMs) / 2);
        skinnedBounds(lo, hi);
        std::printf("  idle_stand mid skinned bbox (%.2f %.2f %.2f) .. (%.2f %.2f %.2f)  height=%.1f\n",
                    lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, hi.z - lo.z);
        check(hi.z - lo.z > 120.0f && hi.z - lo.z < 230.0f,
              "idle_stand pose height is humanoid (120..230 units)");
        check(std::fabs(lo.z) < 25.0f, "idle_stand feet stay near the ground plane",
              "min Z = " + std::to_string(lo.z));
    }
    if (run) {
        model.poseAtTime(run->startMs + (run->endMs - run->startMs) / 2);
        skinnedBounds(lo, hi);
        std::printf("  run mid skinned bbox        (%.2f %.2f %.2f) .. (%.2f %.2f %.2f)  height=%.1f\n",
                    lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, hi.z - lo.z);
    }

    std::printf("\n%s  (%d failure%s)\n", failures ? "SELFTEST FAILED" : "SELFTEST PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
