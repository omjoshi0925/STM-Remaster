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
| trigger_unit | volume containment, fire-once, completion rule |
| config_parser_unit | odd-length strings do not drop stat records |
| gameflow_unit | checkpoints, comic nodes, death, completion |
| wav_unit | PCM16 WAV passthrough, duration |
