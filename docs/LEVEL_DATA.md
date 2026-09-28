# Level scene data (.irr)

Each level pack holds ~23 UTF-16 `.irr` XML scenes (a root plus one per
room). Nodes carry `MeshFile`, absolute transforms, and a `!GameType`
attribute that drives the runtime.

## GameType inventory (levels 1-2, representative)
Geometry / Collisions / NavMesh (per-room meshes); `SpiderMan`/`SpawnPoint`;
enemy spawns (`MeleeThugEnemy_bat|knife`, `MeleeThug_gun`,
`RangeThug_molotov|hammer|big`); bosses as placements (`Boss_Sandman` L1,
`Boss_Rhino` L2); `CheckPoint` (16/15 - they trace the level path);
`Trigger` 49, `TriggerRestore`/`RestorePoint`, `Cinematic` 77/64, `Comic`,
`CameraArea` (authored camera control), `WebGrabPoint`, `Hostage`,
`DestroyableObject` 156, `AnimatedObject` 46, `Car` 10, `StaticObject`,
`Bonus` 93 (position-only).

## Comic trigger nodes per level
L1:18 L2:20 L3:26 L4:29 L5:16 L6:14 L7:15 L8:14 L9:4 L10:7 L11:12 L12:6
(total 181 across ~300 comic pages in comic1+comic2 - nodes show variable
page counts; the node-to-page table is still undecoded, the runtime uses
sequential pages per level).

## Props
Prop nodes reference meshes via `MeshFile` with `..\`-style relative paths
into `entities/meshes_bin` or the level's own `meshes_bin`; a few reference
sibling level packs (all packs are mounted together in the original).
`break_*wall` destructibles ship only `_anim` variants (destruction states).

## Per-level extras
`meshes_bin` also carries the skybox (`lvl01_sky.bdae` + `01_sky_2.tga`),
cinematic rigs (`camera_lv1_*`, `woman_*_gameover`, boss intro meshes), and
water planes.

## Cinematic actors (Milestone 21)
Object-thread PlayDAEAnims and the scene nodes they animate. Generated with
`Tools/list_cine_actors.py <Assets> levelnew_NN --md`; every file ships.

Level 1:

| script | object | node | type | mesh | at | animation | ships as |
|---|---|---|---|---|---|---|---|
| levelnew_01_1238_cinematic.cff | 1272 | fake_sandman | AnimatedObject | sandman_mesh.bdae | 0.0 s | sandman_lv1_end.bdae | Sandman_Lv1_End.bdae |
| levelnew_01_1238_cinematic.cff | 1273 | CI_Car | AnimatedObject | car_mesh_lv1_end.bdae | 0.0 s | car_lv1_end.bdae | Car_Lv1_End.bdae |
| levelnew_01_1238_cinematic.cff | 1275 | CI_Rhino | AnimatedObject | rhino_mesh.bdae | 28.6 s | rhino_lv1_end.bdae | Rhino_Lv1_End.bdae |
| levelnew_01_1254_cinematic.cff | 1272 | fake_sandman | AnimatedObject | sandman_mesh.bdae | 15.7 s | sandman_471_lv1_beforeboss.bdae | sandman_471_Lv1_beforeboss.bdae |
| levelnew_01_1265_cinematic.cff | 1257 | CI_thug_big | AnimatedObject | thug_gun_mesh.bdae | 16.2 s | thug_gun_lv1_start.bdae | thug_gun_lv1_start.bdae |
| levelnew_01_1265_cinematic.cff | 1258 | CI_thug1 | AnimatedObject | thug_bat_mesh.bdae | 16.2 s | thug_bat01_lv1_start.bdae | thug_bat01_lv1_start.bdae |
| levelnew_01_1265_cinematic.cff | 1259 | CI_thug2 | AnimatedObject | thug_knife_mesh.bdae | 16.2 s | thug_bat02_lv1_start.bdae | thug_bat02_lv1_start.bdae |
| levelnew_01_1265_cinematic.cff | 1260 | CI_girl | AnimatedObject | girl_mesh.bdae | 16.2 s | woman_lv1_start.bdae | woman_lv1_start.bdae |
| levelnew_01_1265_cinematic.cff | 1261 | CI_Cop | AnimatedObject | police02_mesh.bdae | 0.0 s | cop_lv1_start.bdae | cop_lv1_start.bdae |
| levelnew_01_1265_cinematic.cff | 1262 | CI_Car | AnimatedObject | car_plice.bdae | 35.3 s | car_plice_lv1_start.bdae | car_plice_lv1_Start.bdae |
| levelnew_01_1265_cinematic.cff | 1277 | CI_web | AnimatedObject | web_rope_ci_mesh.bdae | 0.0 s | web_rope_ci_lv1_start.bdae | web_rope_ci_lv1_start.bdae |
| levelnew_01_1267_cinematic.cff | 1268 | camera_CI | AnimatedObject | woman_camera_mesh_lv1_gameover.bdae | 0.0 s | woman_camera_anim_lv1_gameover.bdae | Woman_Camera_Anim_Lv1_Gameover.bdae |
| levelnew_01_1267_cinematic.cff | 1269 | Women_CI | AnimatedObject | girl_mesh.bdae | 0.0 s | woman_lv1_gameover.bdae | Woman_Lv1_Gameover.bdae |
| levelnew_01_1267_cinematic.cff | 1278 | CI_web2 | AnimatedObject | web_rope_ci_mesh.bdae | 0.0 s | web_rope_ci_gameover.bdae | web_rope_ci_gameover.bdae |

Level 2:

| script | object | node | type | mesh | at | animation | ships as |
|---|---|---|---|---|---|---|---|
| levelnew_02_20077_cinematic.cff | 20055 | Boss_Rhino | Boss_Rhino | rhino_mesh.bdae | 0.0 s | rhino_lv2_end.bdae | Rhino_lv2_end.bdae |
| levelnew_02_20077_cinematic.cff | 1212 | CI_Car | AnimatedObject | car_lv2_end_mesh.bdae | 16.0 s | car_480_lv2_end.bdae | car_480_lv2_end.bdae |
| levelnew_02_20077_cinematic.cff | 1214 | CI_Cop1 | AnimatedObject | police01_mesh.bdae | 19.4 s | cop01_582_lv2_end.bdae | cop01_582_lv2_end.bdae |
| levelnew_02_20077_cinematic.cff | 1215 | CI_Cop2 | AnimatedObject | police01_mesh.bdae | 19.4 s | cop02_582_lv2_end.bdae | cop02_582_lv2_end.bdae |
