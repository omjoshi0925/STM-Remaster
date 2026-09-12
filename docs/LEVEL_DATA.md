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
