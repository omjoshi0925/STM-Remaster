// Pure math invariants, no assets: matrices and point transforms.
#include "BDAEModel.hpp"
#include "unit_common.hpp"
#include <cmath>
using namespace bdae;
int main() {
    Mat4 I{}; for (int i = 0; i < 16; ++i) I.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    Vec3 p{3, -4, 5};
    Vec3 q = transformPoint(I, p);
    ck(q.x == 3 && q.y == -4 && q.z == 5, "identity leaves a point alone");
    Mat4 T = I; T.m[12] = 10; T.m[13] = 20; T.m[14] = 30;
    q = transformPoint(T, p);
    ck(q.x == 13 && q.y == 16 && q.z == 35, "translation lives in m[12..14] (column major)");
    Mat4 TT = mul(T, T);
    ck(TT.m[12] == 20 && TT.m[13] == 40 && TT.m[14] == 60, "mul composes translations");
    Mat4 R = I; float c = std::cos(1.0f), s = std::sin(1.0f);
    R.m[0] = c; R.m[1] = s; R.m[4] = -s; R.m[5] = c;
    Vec3 r = transformPoint(R, Vec3{1, 0, 0});
    ck(std::fabs(r.x - c) < 1e-6f && std::fabs(r.y - s) < 1e-6f, "rotation about Z acts on x");
    Mat4 RI = mul(R, I);
    bool same = true; for (int i = 0; i < 16; ++i) same &= std::fabs(RI.m[i] - R.m[i]) < 1e-6f;
    ck(same, "mul by identity is a no-op");
    UNIT_END("MATH UNIT");
}
