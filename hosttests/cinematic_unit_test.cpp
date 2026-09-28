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
           "</cinematicThread>\r\n"
           // Basic thread (object -1): camera file with a chained script, subtitles, SetVisible by ObjectID
           "<cinematicThread type=\"1\" name=\"Basic Thread\" object=\"-1\">\r\n"
           "<time stamp=\"0\"><command name=\"PlayDAECamera\" id=\"111\"><attributes>"
           "<string name=\"CameraAnimFile\" value=\".\\meshes_bin\\camera_lv1_start.bdae\" /><int name=\"clipID\" value=\"0\" />"
           "<int name=\"^ID^Cinematic^Next\" value=\"1266\" /><float name=\"farPlane\" value=\"10000.000000\" />"
           "</attributes></command>"
           "<command name=\"SetVisible\" id=\"5\"><attributes><int name=\"ObjectID\" value=\"789\" /><bool name=\"Visible\" value=\"false\" /></attributes></command></time>\r\n"
           "<time stamp=\"200\"><command name=\"ShowMessage\" id=\"6\"><attributes><int name=\"$MessageFace\" value=\"1\" />"
           "<string name=\"$LEVEL_STRINGID\" value=\"STR_PROLOGUE_SPIDERMAN_01\" /><int name=\"Timer\" value=\"300\" />"
           "<int name=\"$MessageFacePosition\" value=\"1\" /><bool name=\"modal\" value=\"false\" /></attributes></command></time>\r\n"
           "<time stamp=\"600\"><command name=\"SetVisible\" id=\"7\"><attributes><int name=\"ObjectID\" value=\"789\" /><bool name=\"Visible\" value=\"true\" /></attributes></command></time>\r\n"
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
    ck(c.threads.size() == 4 && c.durationMs == 1000, "four threads (Basic thread has object -1), duration from the last stamp");
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
    ck(c.daeAnims().empty() && c.qtes().empty(), "queries are empty when the script has no such commands");
    // Basic thread: PlayDAECamera, ObjectID SetVisible, ShowMessage
    Cinematic::CameraRequest cr;
    ck(c.cameraRequest(cr) && cr.file == "./meshes_bin/camera_lv1_start.bdae" && cr.nextCinematic == 1266 && cr.farPlane == 10000.0f && !cr.levelEnd,
       "PlayDAECamera: file with forward slashes, chained script id, far plane");
    auto hid = c.hiddenObjectsAt(10);
    ck(hid.size() == 1 && hid[0] == 789, "SetVisible with an ObjectID hides that object");
    ck(c.hiddenObjectsAt(700).empty(), "a later SetVisible true shows it again");
    Cinematic::Message m;
    ck(!c.messageAt(100, m) && c.messageAt(250, m) && m.stringId == "STR_PROLOGUE_SPIDERMAN_01" && m.face == 1 && m.timerMs == 300,
       "ShowMessage is on screen from its stamp");
    ck(!c.messageAt(500, m), "and gone after its timer");
    ck(c.messages().size() == 1, "messages() lists every ShowMessage");
    Cinematic::Qte q;
    ck(!c.nextQteAfter(0, q), "no QTE reported when the script has none");
    Cinematic::DaeAnim da;
    ck(!c.daeAnimAt(1, 5000, da), "no DAE animation reported when the thread has none");
    UNIT_END("CINEMATIC UNIT");
}
