# .cff cinematic scripts

Each level pack ships its cinematics as UTF-16LE XML under `cinematics/`
(Level 1: 78, Level 2: 67). A script is a set of threads, each a timeline of
commands:

    <cinematicThread type="3" name="Player Thread" object="288">
      <time stamp="1100">
        <command name="MoveObject" id="82">
          <attributes>
            <vector3d name="pos" value="-7347.06, 57044.10, 2.74" />
            <quaternion name="rot" value="0, 0, -0.668, 0.744" />
          </attributes>
        </command>
        <command name="SetAnim" id="22"> ... <string name="$Anim" value="idle_stand" /> ...

Thread types: 0 object (a named scene object: Thug_bat, Bus, Rhino, Knife),
1 basic (level-wide commands), 2 camera, 3 player. `object` is the scene node
id. Attribute types: int, float, bool, string, vector3d, quaternion.

Commands seen across both levels (43 kinds): MoveObject (pos, rot, abspos),
SetAnim ($Anim, loop, reverse, speed, ObjectID), SoundControl (Play2D,
Play3D, Loop, Stop, $VoxSounds), DisableAI / EnableAI, SetVisible, PlayDAEAnim
(AnimFile pointing at camera_lv1_* and spiderman_lv1_* BDAEs, clipID),
SetCameraArea (^ID^CameraArea), ChangeCamera (target, dir, Distance),
Transport, StartQTE (QTEID, success and fail cinematic ids), GetDamage,
Physics, KillObject, ShowHealth, Tutorial, Unlock, PlayEffect.

Durations run from 0 to 55 s; the median script is a 1.1 s beat.

Runtime (`Cinematic.hpp`): parse to threads and commands; `playerPoseAt`
interpolates MoveObject keyframes; `playerAnimAt` gives the SetAnim clip in
effect; `cameraAt` gives the ChangeCamera in effect; `soundsBetween` yields
Vox events in a window; `aiDisabledAt` lists object threads with AI off.
objectPoseAt poses any thread by scene object id, hiddenObjectsAt
reads SetVisible windows, daeAnims and qtes expose PlayDAEAnim and StartQTE.
Honoured in play: player keyframes and SetAnim, object-thread motion for
placed enemies, SetVisible, sounds, ChangeCamera. Not yet: PlayDAEAnim
camera animations, QTE branching, PlayEffect.

## Camera tracks and QTEs (Milestone 19)
PlayDAEAnim names per-cinematic character animations (spiderman_lv1_start,
rhino_lv1_end). The camera files are requested by PlayDAECamera instead (see
Milestone 21 below; the Milestone 19 text assumed PlayDAEAnim and never
fired). A CameraTrack is nodes Camera01 (eye) and Camera01_Target (look-at)
sampled on the script clock; Camera_Lv1_End and Camera_Lv1_Gameover are two
of the five tracks the two levels ship. StartQTE opens a tap window in
play; a tap branches to the success cinematic id, a lapse to the fail id,
resolved through the level's cinematic id map.

## Character animation files (Milestone 20)
A PlayDAEAnim on the player thread binds the hero mesh to the named file
(spiderman_lv1_start is a 53 s clip on the 38-joint skin) in a second model
on the script clock, played once and held on the last frame. Object threads'
files are the cinematic actors of Milestone 21.

## Cinematic actors, the Basic thread and subtitles (Milestone 21)

**Object threads name AnimatedObject actors, not enemies.** In Level 1 the
18 object-thread PlayDAEAnims target `CI_*` AnimatedObject scene nodes
(CI_thug1, CI_thug2, CI_thug_big, CI_girl, CI_Cop, CI_Car, CI_web,
fake_sandman, CI_Rhino, camera_CI, Women_CI) whose MeshFile is the character
or vehicle mesh in entities (thug_bat_mesh, police02_mesh, car_plice,
sandman_mesh, rhino_mesh, girl_mesh, web_rope_ci_mesh) or a level-local
mesh (car_mesh_lv1_end, woman_camera_mesh_lv1_gameover). Level 2's end
script animates the Boss_Rhino spawn itself (node 20055). `resolveActorMesh`
picks the prop's MeshFile or the enemy archetype mesh by scene id.

**The files are authored in world space.** thug_bat01_lv1_start.bdae puts
Bip01 at (14568, -10002, 77), 331 units from the Level 1 spawn; the scene
node's own transform (16248, -12992, -650) is not the placement. So an
actor, and Spider-Man in his own per-cinematic file, draws with an identity
model matrix; the anchor (feet) is read back from the skinned vertices for
the camera and for where play resumes. The M20 hero path applied the
gameplay placement on top of the world-space pose and was corrected here.

**File names.** The script asks for `web_rope_ci_lv1_start.bdae`; the pack
also holds `web_rope_ci_0_lv1_start.bdae`, `car_plice_1107_lv1_Start.bdae`,
`THUGK_480_lv1_Start.bdae`. `resolveAnimVariant` tries the exact name
(case-insensitively) and then the name with every `_<digits>_` token
collapsed, refusing ambiguous matches. All 18 requests in Levels 1 and 2
resolve; 14 of them are skinned characters, 6 rigid (cars, the web rope, the
camera prop; rigid files animate nodes and the meshes attach by instance
table or by name, with `bbox*`, `_` and morph-target meshes skipped).

**Rigid actor node animation.** car_plice.bdae has 6 meshes, 2 nodes and no
instance table; car_plice_lv1_Start.bdae animates node `car_plice` from
(16387, -14943) to (14902, -10412) over 2.6 s. Car_Mesh_Lv1_End's animation
also carries morpher and `offsetU` channels that bind to nothing (ignored).

**Basic thread (`object="-1"`).** The regex in the first dump_cff.py
required a non-negative id and hid it. It carries:
- `PlayDAECamera` (CameraAnimFile, clipID, `^ID^Cinematic^Next`, farPlane,
  `level end`, `game end`): the authored camera for the script. Five scripts
  use it (lv1 start 53 s, beforeboss 36 s, end 41 s, gameover 56 s, lv2 end
  39 s); `^ID^Cinematic^Next` chains lv1 start (1265) into 1266 and
  beforeboss (1254) into 1256; `level end` marks both level-end scripts.
- `ShowMessage` ($MessageFace, $LEVEL_STRINGID, Timer, $MessageFacePosition,
  modal): subtitles. 28 lines across the two levels, faces 1 (Spider-Man,
  16 lines), 3, 4, 5 and 9; strings live in the level's own xlsStrings table
  (FORMAT_STRINGS.md).
- `SetVisible` with an `ObjectID` attribute (hides scene objects by id, e.g.
  the five sand piles 786-790 at the Level 1 end), `MustBeVisibleRoom`,
  `ShowStream` (room streaming hints).

**Script length.** A script's last stamp is not its length: the prologue's
last command is at 41.8 s while its camera and hero files run 53 s. The
runtime ends a script when its last stamp, its camera track and Spider-Man's
own animation have all played out.

**Command census (Level 1, 78 scripts, 145 with Level 2):** MoveObject 369,
SetAnim 290, ChangeCamera 145, SoundControl 84, SetVisible 69, DisableAI 63,
InterfaceControl 53, EnableAI 39, IfObjectDestroyed 20, PlayDAEAnim 18,
ShowMessage 18, SetSlowMotion 18, DisableTrigger 18, SetCameraArea 14,
Tutorial 14, StartCinematic 13, IfEnemyDead 9, EnableTrigger 7, PlayEffect 7,
Transport 6, ShowHealth 6, StartQTE 6, Save 6, ShakeCamera 5, PlayDAECamera 4,
GetDamage 4, plus singletons. Honoured after Milestone 21: MoveObject,
SetAnim, SoundControl, SetVisible (both forms), DisableAI/EnableAI (queried),
PlayDAEAnim (hero and actors), PlayDAECamera (track, chain, level end),
ChangeCamera, ShowMessage, StartQTE. Not yet: StartCinematic, Tutorial,
InterfaceControl, IfObjectDestroyed/IfEnemyDead gating, SetSlowMotion,
ShakeCamera, DisableTrigger/EnableTrigger, Save, PlayEffect, Transport.
