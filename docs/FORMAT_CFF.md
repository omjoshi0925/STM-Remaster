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
Not yet honoured: PlayDAEAnim camera animations, object-thread motion,
QTE branching, SetVisible, PlayEffect.
