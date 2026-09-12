# Roadmap (data pointers per item)

Ordered by leverage; each item lists where its data/reference lives.

1. **Audio slot linkage** - the clips play (Milestone 14/15), but which event
   fires at which moment is still our choice. `BehaviorSoundMapList.bin` (63
   enemy/boss slots) and `MC_SOUND.bin` (38 hero slots) name the original
   vocabulary; their numeric linkage to VoxSounds rows is undecoded. The
   `VoxSoundManager` symbols in the Android binary are the reference.
2. **In-engine cinematics** - `.cff` files per level, `camera_lv1_*` BDAEs,
   `CCinematicThread` (110 methods) as reference; `Cinematic` nodes (77 in
   L1) place them.
3. **Original boss phases / QTEs** - `CBoss`, `QTE` config, boss meshes and
   `*_lv1_boss` cinematic rigs.
4. **Wall traversal & web swing** - MC_STATE's `k_state_move_onwall` family,
   `WebGrabPoint` nodes, Player state machine symbols.
5. **Camera areas** - `CameraArea` nodes (authored camera volumes; the
   current orbit camera is not original).
6. **Trigger runtime** - `Trigger`/`TriggerRestore`/`RestorePoint` nodes;
   likely drives real level completion (current completion = full
   checkpoint chain, documented assumption).
7. **Menus** - `mainmenu.tga` + GS_* screen definitions + `interface_2.tga`.
8. **Saves / ranks** - `LevelRank.bin`, `data.save` + `levelRanks.dat`
   specimens from the Android image.
9. **Destruction states** - `break_*wall_anim` meshes; debris for
   destructibles.
10. **Room streaming** - rooms are separate meshes already; stream by
    checkpoint proximity for memory headroom on bigger levels.
