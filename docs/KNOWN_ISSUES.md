# Known issues

Open
- Movement reported as not working on one device build; not yet reproduced, needs a description of what dragging does and the on-screen fps value.
- A handful of Level 1 texture names do not ship under those names in the iOS packs (Car_01, 42_mall_glass, 05_atlas_A); those surfaces fall back to baked vertex colour.
- Trigger and camera-area extents are not stored on the nodes; volumes use conservative boxes and control-point bounds.
- Comic pages for levels after the first use estimated start pages.
- Audio event choices per moment are ours; the slot linkage in BehaviorSoundMapList.bin and MC_SOUND.bin is undecoded.
- Cinematic playback assumes ChangeCamera dir points from camera to target; if scenes look reversed, negate it.
- Cinematic actors and the hero's per-cinematic animation draw in the authored world space with no gameplay placement applied; if a character appears offset from the scene on the device, the scale or origin assumption of the world-space files is what to check (thug_bat01_lv1_start puts Bip01 331 units from the spawn on the host).
- Subtitle speaker faces 3, 4, 5 and 9 have no portrait yet (only Spider-Man's HUD portrait, face 1, is drawn); the text shows for every face.
- PlayDAECamera farPlane (10000/20000) is recorded but not applied; our far plane is larger, so distant geometry stays visible where the original faded it.
- Scripts that other scripts start (StartCinematic, 13 uses), Tutorial prompts, InterfaceControl, IfEnemyDead/IfObjectDestroyed gating, SetSlowMotion and ShakeCamera are parsed but not honoured.
- Actors bound by one script persist for the level on their last frame; a later script that re-animates the same object replaces the actor (sandman 1272 in beforeboss then end). Two PlayDAEAnims for one object inside a single script would keep only the later one; no shipped script does that.
- Hostage props still draw in bind pose; they are not AnimatedObjects and no script animates them.

Fixed
- xlsStrings read as line-paired text gave 448 "empty" strings and garbage values: the .data is a u32 offset table of UTF-16 strings; 589 of 591 MAIN strings decode (Milestone 21).
- Camera tracks never engaged: scripts request them with PlayDAECamera on the Basic thread, not PlayDAEAnim (Milestone 21).
- Enemy/actor DAE animations were reported, not played; the hero's cinematic animation was drawn with the gameplay placement on top of its world-space pose (Milestone 21).
- Skinned AnimatedObjects (CI_thug1, CI_Rhino...) stood in the world in bind pose before their script; they now wait for it (Milestone 21).
- Skipping a cinematic left hidden objects hidden and the hero where the skip happened; skip now settles the scene like a natural end (Milestone 21).
- The suite runner had dropped the strings, asset audit and format regression suites; they run again (Milestone 21).
- Red circles wallpapering the scene: wrong atlas rectangles, replaced with measured ones.
- HUD never appearing: interface.tga is RGBA4444, loader only handled PVRTC.
- Level too dark: baked vertex colours are a 2x modulate.
- Enemies dropping the wrong stats rows: 2-byte string alignment in the config parser.
