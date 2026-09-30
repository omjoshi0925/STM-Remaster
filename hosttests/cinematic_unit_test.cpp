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
           "</cinematicThread>\r\n"
           // control flow (Milestone 22): gates, hand-over, trigger toggles, save, interface, shake, tutorial, slow motion
           "<cinematicThread type=\"1\" name=\"Basic Thread\" object=\"-1\">\r\n"
           "<time stamp=\"0\">"
           "<command name=\"IfObjectDestroyed\" id=\"1\"><attributes><int name=\"ObjectID\" value=\"1009\" /></attributes></command>"
           "<command name=\"IfEnemyDead\" id=\"2\"><attributes><int name=\"IDEnemy\" value=\"1139\" /></attributes></command>"
           "<command name=\"IfHealthTo\" id=\"3\"><attributes><int name=\"IDEnemy\" value=\"20055\" /><int name=\"Health\" value=\"66\" /></attributes></command>"
           "<command name=\"InterfaceControl\" id=\"4\"><attributes><bool name=\"ControlEnable\" value=\"false\" /><bool name=\"BlackEnable\" value=\"true\" /><bool name=\"SkipEnable\" value=\"true\" /></attributes></command>"
           "<command name=\"SetSlowMotion\" id=\"5\"><attributes><bool name=\"Enable\" value=\"true\" /><float name=\"Denominator\" value=\"5.0\" /><float name=\"TimeOn\" value=\"400.0\" /></attributes></command>"
           "</time>\r\n"
           "<time stamp=\"300\">"
           "<command name=\"DisableTrigger\" id=\"6\"><attributes><int name=\"^ID^Trigger\" value=\"1046\" /></attributes></command>"
           "<command name=\"EnableTrigger\" id=\"7\"><attributes><int name=\"^ID^Trigger\" value=\"1002\" /></attributes></command>"
           "<command name=\"Save\" id=\"8\"><attributes><int name=\"^ID^CheckPoint\" value=\"30027\" /></attributes></command>"
           "<command name=\"ShakeCamera\" id=\"9\"><attributes><float name=\"MaxOff\" value=\"20.0\" /><int name=\"ShakeFrame\" value=\"20\" /></attributes></command>"
           "<command name=\"Tutorial\" id=\"10\"><attributes><string name=\"Title$Tutorial_STRINGID\" value=\"\" /><string name=\"Content$Tutorial_STRINGID\" value=\"STR_JUMP\" /><int name=\"$TutorialButton\" value=\"-1\" /><bool name=\"blackScreen\" value=\"false\" /><int name=\"Timer\" value=\"3000\" /></attributes></command>"
           "<command name=\"InterfaceControl\" id=\"11\"><attributes><bool name=\"ControlEnable\" value=\"true\" /><bool name=\"BlackEnable\" value=\"false\" /><bool name=\"SkipEnable\" value=\"false\" /></attributes></command>"
           "<command name=\"EnableCameraArea\" id=\"12\"><attributes><int name=\"^ID^CameraArea\" value=\"166\" /><bool name=\"enable\" value=\"true\" /></attributes></command>"
           "</time>\r\n"
           "<time stamp=\"1000\">"
           "<command name=\"StartCinematic\" id=\"13\"><attributes><int name=\"CinematicID\" value=\"1212\" /></attributes></command>"
           "<command name=\"LevelEnd\" id=\"14\"><attributes><bool name=\"GoToNext\" value=\"true\" /></attributes></command>"
           "<command name=\"Unlock\" id=\"15\"><attributes><string name=\"$SkillID\" value=\"1 sense\" /></attributes></command>"
           "</time>\r\n"
           "</cinematicThread>\r\n"
           "<cinematicThread type=\"0\" name=\"thug\" object=\"400\">\r\n"
           "<time stamp=\"500\"><command name=\"KillObject\" id=\"16\"><attributes></attributes></command>"
           "<command name=\"ShowHealth\" id=\"17\"><attributes><int name=\"ObjectID\" value=\"400\" /></attributes></command></time>\r\n"
           "</cinematicThread>\r\n"
           "<cinematicThread type=\"3\" name=\"Player Thread\" object=\"288\">\r\n"
           "<time stamp=\"600\"><command name=\"GetDamage\" id=\"18\"><attributes><float name=\"DamageValue\" value=\"200.0\" /></attributes></command></time>\r\n"
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
    ck(c.threads.size() == 7 && c.durationMs == 1000, "seven threads (Basic threads have object -1), duration from the last stamp");
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
    // control flow
    auto conds = c.conditions();
    ck(conds.size() == 3 && conds[0].kind == Cinematic::Condition::OBJECT_DESTROYED && conds[0].id == 1009 &&
       conds[1].kind == Cinematic::Condition::ENEMY_DEAD && conds[1].id == 1139 &&
       conds[2].kind == Cinematic::Condition::HEALTH_AT_MOST && conds[2].id == 20055 && conds[2].value == 66.0f,
       "If* gates read as typed conditions");
    auto starts = c.startsBetween(999, 1000);
    ck(starts.size() == 1 && starts[0] == 1212 && c.startsBetween(0, 999).empty(), "StartCinematic hands over at its stamp");
    auto tog = c.triggerTogglesBetween(0, 300);
    ck(tog.size() == 2 && tog[0].first == 1046 && !tog[0].second && tog[1].first == 1002 && tog[1].second, "trigger toggles carry id and state");
    auto cam = c.cameraAreaTogglesBetween(0, 300);
    ck(cam.size() == 1 && cam[0].first == 166 && cam[0].second, "EnableCameraArea toggle");
    auto saves = c.savesBetween(0, 300);
    ck(saves.size() == 1 && saves[0] == 30027, "Save names its CheckPoint id");
    ck(c.damageBetween(500, 600) == 200.0f && c.damageBetween(0, 500) == 0.0f, "GetDamage sums on the player thread");
    auto kills = c.killsBetween(0, 500);
    ck(kills.size() == 1 && kills[0] == 400, "KillObject reports the thread's object");
    auto hp = c.showHealthBetween(0, 500);
    ck(hp.size() == 1 && hp[0] == 400, "ShowHealth reports the enemy id");
    ck(c.levelEndBetween(999, 1000) && !c.levelEndBetween(0, 999), "LevelEnd in its window");
    auto un = c.unlocksBetween(999, 1000);
    ck(un.size() == 1 && un[0] == "1 sense", "Unlock carries its skill id");
    Cinematic::Interface ui0 = c.interfaceAt(0), ui1 = c.interfaceAt(300);
    ck(ui0.set && !ui0.control && ui0.black && ui0.skip, "InterfaceControl at 0: no control, black, skip");
    ck(ui1.control && !ui1.black && !ui1.skip, "a later InterfaceControl replaces it");
    ck(!Cinematic().interfaceAt(0).set && Cinematic().interfaceAt(0).skip, "defaults when a script never sets it");
    auto sh = c.shakesBetween(0, 300);
    ck(sh.size() == 1 && sh[0].maxOff == 20.0f && sh[0].frames == 20, "ShakeCamera amplitude and frames");
    Cinematic::Tutorial tu;
    ck(!c.tutorialAt(100, tu) && c.tutorialAt(300, tu) && tu.contentId == "STR_JUMP" && tu.timerMs == 3000 && !tu.blackScreen,
       "Tutorial card from its stamp");
    ck(c.tutorialAt(3299, tu) && !c.tutorialAt(3300, tu), "a timed card expires");
    ck(c.slowMotionAt(0) == 5.0f && c.slowMotionAt(399) == 5.0f && c.slowMotionAt(400) == 1.0f, "SetSlowMotion divisor for TimeOn");
    ck(Cinematic::expandTutorialMarkup("Tap ^J once.\nUse ^2red^0 orbs") == "Tap JUMP once. Use red orbs", "tutorial markup expands buttons and drops colours");
    Cinematic::Qte q;
    ck(!c.nextQteAfter(0, q), "no QTE reported when the script has none");
    Cinematic::DaeAnim da;
    ck(!c.daeAnimAt(1, 5000, da), "no DAE animation reported when the thread has none");
    UNIT_END("CINEMATIC UNIT");
}
