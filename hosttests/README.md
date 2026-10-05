# Host test suites

Run against an extracted Assets tree, no device needed:

    hosttests/run_host_tests.sh ~/path/to/Assets

| suite | verifies |
|---|---|
| bdae_selftest | BDAE parse, skin bind pose, bbox against the authored one |
| leveltest | full level assembly: rooms, collision, navmesh, spawn |
| milestone4 | walking on the navmesh, ground snapping |
| milestone5 | enemy placements, skinned anchors, level batching |
| milestone6 | config-driven combat: stats, AI states, hero punches |
| milestone7 | sprites pack, bsprite modules, GS screens |
| milestone8 | level flow, checkpoints, both levels, ranged enemies, bosses |
| milestone9 | props, damage table, combo chain, knockback |
| milestone10 | texture binding on data, lightmaps, second UV set |
| milestone12 | comic beats, checkpoint events, corpse timing |
| milestone14 | audio event table, ADPCM decode, per-archetype sounds |
| script_test | trigger volumes, cinematics, camera areas, completion rule |
| asset_audit | every texture, prop, cinematic and UI reference resolves |
| format_regression | locks documented byte-level facts |

A suite prints PASS/FAIL per check and ends with PASSED or FAILED.

Asset-free unit suites (`hosttests/run_unit_tests.sh`, also run by CI):

| suite | verifies |
|---|---|
| math_unit | matrix identity, translation, composition, rotation |
| trigger_unit | volume containment, enter/while edges, Enabled/AutoDisabled, EnableTrigger by id, consume, boss phase thresholds |
| config_parser_unit | odd-length strings do not drop stat records |
| gameflow_unit | checkpoints, comic nodes, death, completion, authored-end levels |
| wav_unit | PCM16 WAV passthrough, duration |

| cinematic_test | every .cff parses, sample values, sounds exist, trigger links |
| cinematic_unit | parser on a synthetic script |
| sound_slots_test | BehaviorSoundMapList and MC_SOUND decode to VoxSounds rows |
| camera_track_test | camera BDAEs decode to moving eye and target; every PlayDAECamera request loads, chains resolve, level ends flagged |
| dae_anim_test | per-cinematic animation files bind to their meshes with real motion |
| cine_actor_test | every object-thread PlayDAEAnim in Levels 1 and 2 maps to a scene node, its file ships, loads and animates in world space |
| cine_actor_unit | actor clock, enemy mesh table, scene-id lookups, file-variant resolution |
| dialogue_test | every ShowMessage resolves to text in the level's string table; faces and timers |
| strings_test | xlsStrings offset-table decode (591 strings), chapter names, per-level subtitle tables |
| script_flow_test | the authored script graph: trigger links, StartCinematic hand-overs, arming, saves, gates and tutorial cards resolve; start and epilogue scripts from the SpiderMan node |
