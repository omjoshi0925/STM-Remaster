# Level scripting: triggers, cinematics, camera areas

The original levels are not hard-coded: `.irr` scenes author named volumes and
markers that the engine reacts to. Level 1 places 49 `Trigger`, 20
`TriggerRestore`, 20 `RestorePoint`, 77 `Cinematic`, 44 `CameraArea` and 193
`CamCtrlPoint` nodes.

## Trigger
A named node whose transform positions a volume. The **name carries the
meaning**, and the suffix after `Trigger_` is the tag the runtime matches:

    Trigger_Lv1_Start   Trigger_3thugs     Trigger_opendoor
    Trigger_Lv1_Boss    Trigger_if_BOSS_DIE  Trigger_red_orb_get
    Trigger_jump_tutor  Trigger_swing_tutor  Trigger_webswing
    Trigger_sense       Trigger_hidearrow    Trigger_dead

Tutorial, cinematic, encounter and progression triggers all share the shape.
The authored **extent is not stored on the node**, so the runtime uses a
conservative box (documented in `Level.cpp`); refining it is future work.

## Cinematic
`Cinematic_<tag>` nodes carry `!ScriptFile`, e.g.
`.\cinematics\levelnew_01_1162_cinematic.cff`. They pair with the trigger of
the same tag: `Trigger_3thugs` <-> `Cinematic_3thugs`. 35 of Level 1's
triggers name a cinematic this way. The `.cff` format itself is not decoded
yet - the runtime reports the path when a trigger fires.

## CameraArea and CamCtrlPoint
`CameraArea` nodes are authored camera volumes; each `CamCtrlPoint` names its
owner through an attribute of the form `!^Owner^CameraArea` holding the area's
node id. The area's extent is likewise not on the node, so the runtime derives
it from the bounding box of its own control points plus padding - which places
the Level 1 spawn inside a volume and covers 10 of the 16 checkpoints.

## TriggerRestore and RestorePoint
Paired recovery markers (20 of each in Level 1): the runtime respawns at the
RestorePoint nearest the last checkpoint.

## Runtime
`Script.hpp` exposes `TriggerRuntime`: bind a level, call `update(hero)` each
frame, receive the volumes newly entered (tag plus cinematic path). Each fires
once per level. `isCompletionTag` recognises the end-of-level vocabulary
(`if_boss_die`, `end`, `win`); the caller still confirms the boss is down.
