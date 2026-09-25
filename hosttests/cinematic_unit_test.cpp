// Cinematic parser on a synthetic UTF-16 script, no assets.
#include "Cinematic.hpp"
#include "unit_common.hpp"
#include <cmath>
#include <cstdio>
#include <string>
using namespace bdae;
static std::string xml() {
    return "<?xml version=\"1.0\"?>\r\n<cinematicThread type=\"3\" name=\"Player Thread\" object=\"1\">\r\n"
           "<time stamp=\"0\"><command name=\"MoveObject\" id=\"82\"><attributes>"
           "<vector3d name=\"pos\" value=\"0.0, 0.0, 0.0\" /><quaternion name=\"rot\" value=\"0, 0, 0, 1\" />"
           "</attributes></command><command name=\"SetAnim\" id=\"22\"><attributes><string name=\"$Anim\" value=\"idle\" />"
           "<bool name=\"loop\" value=\"true\" /></attributes></command></time>\r\n"
           "<time stamp=\"1000\"><command name=\"MoveObject\" id=\"82\"><attributes>"
           "<vector3d name=\"pos\" value=\"100.0, 0.0, 0.0\" /></attributes></command>"
           "<command name=\"SoundControl\" id=\"9\"><attributes><bool name=\"Play2D\" value=\"true\" />"
           "<bool name=\"Stop\" value=\"false\" /><string name=\"$VoxSounds\" value=\"SFX_X\" /></attributes></command></time>\r\n"
           "</cinematicThread>\r\n<cinematicThread type=\"2\" name=\"Camera Thread\" object=\"2\">\r\n"
           "<time stamp=\"500\"><command name=\"ChangeCamera\" id=\"7\"><attributes>"
           "<vector3d name=\"target\" value=\"1, 2, 3\" /><vector3d name=\"dir\" value=\"0, 1, 0\" />"
           "<float name=\"Distance\" value=\"640\" /></attributes></command></time>\r\n</cinematicThread>\r\n"
           "<cinematicThread type=\"0\" name=\"Thug\" object=\"3\">\r\n"
           "<time stamp=\"0\"><command name=\"DisableAI\" id=\"1\"><attributes></attributes></command></time>"
           "<time stamp=\"800\"><command name=\"EnableAI\" id=\"2\"><attributes></attributes></command></time>"
           "</cinematicThread>\r\n";
}
int main() {
    std::string s = xml();
    char path[] = "/tmp/stmcffXXXXXX"; int fd = mkstemp(path); FILE* f = fdopen(fd, "wb");
    unsigned char bom[2] = {0xff, 0xfe}; fwrite(bom, 1, 2, f);
    for (char ch : s) { unsigned char u[2] = {(unsigned char)ch, 0}; fwrite(u, 1, 2, f); }
    fclose(f);
    Cinematic c; std::string e;
    ck(c.load(path, e), "synthetic UTF-16 script loads", e);
    ck(c.threads.size() == 3 && c.durationMs == 1000, "three threads, duration from the last stamp");
    Vec3 p; float yaw;
    ck(c.playerPoseAt(500, p, yaw) && std::fabs(p.x - 50.0f) < 1e-3f, "pose interpolates between keyframes", std::to_string(p.x));
    ck(c.playerPoseAt(5000, p, yaw) && p.x == 100.0f, "past the last key holds the last pose");
    ck(c.playerAnimAt(10) == "idle", "SetAnim read with its $Anim string");
    auto snd = c.soundsBetween(500, 1000);
    ck(snd.size() == 1 && snd[0] == "SFX_X" && c.soundsBetween(0, 500).empty(), "sounds reported once, in their window");
    CineCamera cc;
    ck(!c.cameraAt(100, cc) && c.cameraAt(600, cc) && cc.distance == 640 && cc.target.z == 3, "camera in effect after its stamp");
    ck(c.aiDisabledAt(400).size() == 1 && c.aiDisabledAt(900).empty(), "AI disabled between DisableAI and EnableAI");
    Vec3 op; float oyaw;
    ck(c.objectPoseAt(1, 500, op, oyaw) && std::fabs(op.x - 50.0f) < 1e-3f && !c.objectPoseAt(99, 500, op, oyaw),
       "objectPoseAt finds a thread by scene object id");
    ck(c.daeAnims().empty() && c.qtes().empty() && c.hiddenObjectsAt(10).empty(),
       "queries are empty when the script has no such commands");
    UNIT_END("CINEMATIC UNIT");
}
