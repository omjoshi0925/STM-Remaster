# Level scripting: triggers, cinematics, camera areas

The original levels are not hard-coded: `.irr` scenes author named volumes and
markers that the engine reacts to. Level 1 places 64 `Trigger`, 20
`TriggerRestore`, 20 `RestorePoint`, 77 `Cinematic`, 44 `CameraArea` and 193
`CamCtrlPoint` nodes.

## Trigger (authored model, Milestone 22)
A node whose transform positions a volume. The name is only a label: what a
trigger does is in its attributes.

    Enabled                true/false    about half start disabled and are armed by a script
    AutoDisabled           true/false    fire once, then switch off
    ^OutToIn^Cinematic     node id       script on entering (-1 none)
    ^InToOut^Cinematic     node id       script on leaving
    ^WhileIn^Cinematic     node id       script every frame while inside
    ^WhileOut^Cinematic    node id       script every frame while outside
    IsOBBox, MaterialType  editor fields

Level 1 (64 Trigger nodes): 33 enabled at start, 25 AutoDisabled, 27 with an
enter link, 6 WhileIn, 12 WhileOut; every link names a Cinematic node the
level holds. The name-based pairing used until Milestone 21
(`Trigger_3thugs` <-> `Cinematic_3thugs`) disagreed with the links on 31 of
the 64 and armed volumes the original keeps disabled, so it is gone.
`Trigger_Lv1_Start` itself is a disabled WhileIn trigger whose script is a
leftover; the level actually starts from the SpiderMan node (below).

While* scripts repeat: they carry If* gates (IfEnemyDead, IfObjectDestroyed,
IfHealthTo) and, once the gate holds, usually `DisableTrigger` themselves and
`EnableTrigger` the next beat. That is how "kill all thugs then the door
opens" is authored. The authored **extent is not stored on the node**, so the
runtime uses a conservative box (documented in `Level.cpp`).

## SpiderMan node links
The `SpiderMan` spawn node names two scripts: `^Link^Cinematic` plays when the
level starts (Level 1: 1265, the prologue) and `^EndGame^Cinematic` when the
level ends (Level 1: 1267, the epilogue with the girl and the camera flash).
Level 2 has -1 for both; its intro is a WhileIn trigger at the spawn.

## Boss node links
`Boss_*` spawns name their phase scripts: `^ToStage2^Cinematic` (Level 2
Rhino: 20065, gated IfHealthTo 66 %) and `^ToStage3^Cinematic` (20087, 33 %).
Level 1 authors the same beats as WhileOut triggers `Trigger_60%` /
`Trigger_33%`. Not yet honoured by the runtime.

## Cinematic
`Cinematic_<name>` nodes carry `!ScriptFile`, e.g.
`.\cinematics\levelnew_01_1162_cinematic.cff`; scripts reach each other by
node id (StartCinematic, PlayDAECamera `^ID^Cinematic^Next`, StartQTE
success/fail). `Tools/script_graph.py` prints the whole graph; in Level 1,
67 of 74 Cinematic nodes are reachable from the spawn node, the triggers and
those hand-overs. The `.cff` format is in FORMAT_CFF.md.

## CheckPoint
`CheckPoint` nodes (16 in Level 1) carry `Enabled`, `SavePosition` and
`^Link^WayPoint`; scripts' `Save` names one by id to set the respawn point.

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
frame, receive `ScriptEvent`s (ENTER / EXIT once per crossing, WHILE_IN /
WHILE_OUT every call) for enabled volumes, each with the Cinematic id and
path its link names. The caller checks the script's If* gates, starts it, and
calls `consume(index)` so AutoDisabled triggers switch off; `setEnabled(id,
on)` serves the scripts' EnableTrigger/DisableTrigger. `isCompletionTag` is
legacy recognition; the authored end is PlayDAECamera "level end" / LevelEnd.
