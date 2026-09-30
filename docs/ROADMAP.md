# Roadmap (data pointers per item)

Ordered by leverage; each item lists where its data/reference lives.

1. **Audio moments** - the slot tables are decoded (Milestone 18) and the
   runtime uses them; what remains is firing every slot at its original
   moment (land, jump swoosh, wall climb, swing start and end) as those
   mechanics arrive, and 3D panning.
2. **In-engine cinematics** - `.cff` files per level, `camera_lv1_*` BDAEs,
   `CCinematicThread` (110 methods) as reference; `Cinematic` nodes (77 in
   L1) place them.
3. **Original boss phases / QTEs** - `CBoss`, `QTE` config, boss meshes and
   `*_lv1_boss` cinematic rigs.
4. **Wall traversal & web swing** - MC_STATE's `k_state_move_onwall` family,
   `WebGrabPoint` nodes, Player state machine symbols.
5. **Camera areas** - `CameraArea` nodes (authored camera volumes; the
   current orbit camera is not original).
6. **Script commands still unhonoured** - after Milestone 22 (the authored
   trigger graph, gates, hand-overs, Save, GetDamage, KillObject, ShowHealth,
   LevelEnd, InterfaceControl, ShakeCamera, Tutorial, the epilogue) what
   remains: boss phase scripts (^ToStage2/3^Cinematic on Boss nodes, gated
   IfHealthTo 66/33 - small, next), SetSlowMotion (parsed), StartProgress /
   StopProgress and StopAction (boss runs between WayPoints), StartSlide
   (rope slides), Transport, Physics, Throwing, PlayEffect. Reference:
   `CCinematicThread` in ENGINE_MAP.md; `Tools/script_graph.py` shows where
   each sits in the level.
6. **ControlEnable scripts during play** - tutorial hints and short beats
   author ControlEnable=true; running them without pausing play needs the
   script clock decoupled from the CINEMATIC phase.
7. **Subtitle portraits** - $MessageFace 3/4/5/9 (girl, cop, Sandman...) need
   their portrait art; interface_2.tga and the GS screens are where to look.
7. **Menus** - `mainmenu.tga` + GS_* screen definitions + `interface_2.tga`.
8. **Saves / ranks** - `LevelRank.bin`, `data.save` + `levelRanks.dat`
   specimens from the Android image.
9. **Destruction states** - `break_*wall_anim` meshes; debris for
   destructibles.
10. **Room streaming** - rooms are separate meshes already; stream by
    checkpoint proximity for memory headroom on bigger levels.
