# Known issues

Open
- Movement reported as not working on one device build; not yet reproduced, needs a description of what dragging does and the on-screen fps value. Note: the device build was broken from Milestone 15 to 22.1 (see Fixed), so any device run in that window was an older binary.
- A handful of Level 1 texture names do not ship under those names in the iOS packs (Car_01, 42_mall_glass, 05_atlas_A); those surfaces fall back to baked vertex colour.
- Trigger and camera-area extents are not stored on the nodes; volumes use conservative boxes and control-point bounds. With the authored links now driving the scripts, a box that is too small or too large shows up as a beat that fires early, late or not at all: report the trigger name from the console line.
- An enter edge that happens while another script is running is dropped (the original may queue it); While* triggers re-report, enter-only ones do not until re-entry.
- SetSlowMotion is parsed but not applied; Transport, Physics, Throwing, StopAction, StartProgress/StopProgress, StartSlide, StartTimer, Restore, PlayEffect and the boss nodes' ^ToStage2/3^ phase scripts are not honoured.
- Scripts with ControlEnable=true still pause play (the letterbox is dropped, control is not returned); tutorial hints during play therefore freeze the action for their timer.
- ShakeCamera amplitude is a guessed scale (MaxOff x 6 world units); the checkpoint-visited completion rule still exists beside the authored LevelEnd and can complete a level first.
- Comic pages for levels after the first use estimated start pages.
- Audio event choices per moment are ours; the slot linkage in BehaviorSoundMapList.bin and MC_SOUND.bin is undecoded.
- Cinematic playback assumes ChangeCamera dir points from camera to target; if scenes look reversed, negate it.
- Cinematic actors and the hero's per-cinematic animation draw in the authored world space with no gameplay placement applied; if a character appears offset from the scene on the device, the scale or origin assumption of the world-space files is what to check (thug_bat01_lv1_start puts Bip01 331 units from the spawn on the host).
- Subtitle speaker faces 3, 4, 5 and 9 have no portrait yet (only Spider-Man's HUD portrait, face 1, is drawn); the text shows for every face.
- PlayDAECamera farPlane (10000/20000) is recorded but not applied; our far plane is larger, so distant geometry stays visible where the original faded it.
- Actors bound by one script persist for the level on their last frame; a later script that re-animates the same object replaces the actor (sandman 1272 in beforeboss then end). Two PlayDAEAnims for one object inside a single script would keep only the later one; no shipped script does that.
- Hostage props still draw in bind pose; they are not AnimatedObjects and no script animates them.

Fixed
- iOS build broken since Milestone 15: the enemy update loop lost its braces (hit handling outside the loop); M22 dropped startCinematic's levelDir; GNU typeof; unit suites missing <unistd.h> on macOS. CI caught all of it from its first run but reported nothing readable; fixed in 22.1 with error annotations and a real device-build workflow.
- Triggers were paired with scripts by name and all treated as enabled; the authored links (^OutToIn^ etc.), Enabled and AutoDisabled flags now drive them, and the level start comes from the SpiderMan node (Milestone 22).
- Commands at stamp 0 (StartCinematic, EnableCameraArea, many SoundControl) never fired: windows were (t0, t1] (Milestone 22).
- Zero-length scripts were rejected, so every tutorial card and hand-over beat was skipped (Milestone 22).
- SKIP never drew during a script with the font atlas loaded: the case sat in the non-cinematic branch (Milestone 22).
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
