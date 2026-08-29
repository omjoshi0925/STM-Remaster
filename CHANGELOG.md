# Milestone 4 — skinned Spider-Man in original Level 1 Room 1

Applies on top of the Milestone 3 compile-fix state.

---

## What was implemented

**New portable C++ (no Metal, no Objective-C — these compile and run on your Mac
as host tests):**

- `BDAEModel.hpp/.cpp` — full BDAE `0,0,0,324` loader: header, string table,
  visual-scene node hierarchy, `instance_geometry` resolution, vertex attribute
  table, skin controller, animation channels and clips, keyframe sampling with
  slerp, cross-fade blending, and skinning-matrix computation.
- `Level.hpp/.cpp` — Irrlicht `.irr` room parser (UTF-16LE XML), placed
  triangle-soup assembly for visual / collision / navmesh, downward ground
  raycast, and walkability queries.
- `Character.hpp/.cpp` — locomotion state machine (idle / walk / run) with
  cross-fade, yaw turning, navmesh-gated movement with X/Y slide, ground snap,
  and spawn projection.

**Replaced:**

- `Renderer.mm` — GPU skinning (38 bone matrices in a buffer, 4 influences per
  vertex), separate skinned and static pipelines, level rendering, follow
  camera, virtual-stick movement on the left half and camera orbit on the right.
- `BDAE324.cpp/.hpp` are superseded and moved to `Sources/_superseded/`. CMake
  globs only `Sources/*.cpp`, so they drop out of the build but stay on disk.

**Format work behind it** is written up in `FORMAT_BDAE324.md`.

---

## Two real bugs found in the existing loader

1. **Submesh stride was 32 bytes; it is 64.** This was invisible on Spider-Man
   (one submesh) but on level geometry it accepted garbage submeshes — Room 1
   read as 25,996 triangles instead of the true 1,871.
2. **Vertex attributes were assumed to be pos/normal/uv at fixed slots.** Level
   geometry and navmeshes ship `pos3, colour, uv2` with *no normals*, which is
   why `geometry01.bdae` and `Nav01.bdae` produced nothing at all. Attributes
   are now read from the offset and type tables, and missing normals are
   generated from faces.

---

## What was verified locally, against your actual assets

Three host test suites, **39 assertions, 0 failures**. Re-run them yourself:

```
hosttests/run_host_tests.sh /Users/joshum/Downloads/SpiderMan_Native_ARM64_Port_Workbench
```

**Skeleton / skin / animation**
- 641 vertices, 920 triangles, 32-byte stride, 46 scene nodes, 38 Bone sids
- every skin joint resolves to a scene node; max 3 influences per vertex
- all 641 vertex weight sets sum to 1.0, worst deviation **0.000000**
- `world × inverseBind × bindShape` equals the bind shape matrix at rest,
  worst element error **0.0032**
- bind-pose skinned bbox matches the authored bbox to **0.031** units
- 46 channels, 242 clips, all channels bind to nodes
- bone lengths stay rigid through `idle_stand` and `run`, worst relative drift
  **0.000001**
- `idle_stand` gives a humanoid 135.8-unit silhouette with feet at Z ≈ −1.5

**Level 1 Room 1**
- 60 Irrlicht nodes parsed; Geometry, Collisions and NavMesh all resolve
- 1,871 visual triangles, 148 collision triangles, 12 navmesh triangles
- ground query returns hits across the room footprint

**Integrated simulation** (360 frames at 60 Hz, headless)
- spawn read from the original `SpiderMan` node at (14701.2, −9696.7, 9.6) and
  projected onto the navmesh
- run speed **not invented**: measured from the authored clip. The left foot
  travels 110.2 units over the 0.80 s cycle → ~276 units/s
- state sequence `idle_stand → walk → run → idle_stand`
- travelled 884 units with **0** frames off walkable ground
- skinned height stays humanoid throughout

**Independent cross-check.** All three `MeleeThugEnemy` spawns in Room 1 land on
the navmesh (walkable = 1), and `Boss_Sandman` at Z = 858 correctly does not —
he arrives from above. The game's own enemy placements agreeing with the game's
own walkable surface is strong evidence the transforms and queries are right.

---

## What to expect on the iPhone

Spider-Man standing correctly posed — not the malformed Milestone 2 pose — inside
Room 1 of the original first level, playing the original `idle_stand`, and
switching to `walk` and `run` as you drag. Left half of the screen is a virtual
stick, right half orbits the camera. He should not walk off the navmesh.

The status label reports mesh, bone and clip counts plus level triangle counts,
so if something failed to load you will see which.

---

## What is NOT verified, and what is still missing

**Not verified.** The Metal side has had **zero** execution. I cannot compile
Objective-C++ or Metal here. `Renderer.mm` is careful but unproven:

- the shader source has never been through the Metal compiler
- CPU/GPU struct layouts were checked by hand and by a host program
  (`GPUSkinVertex` = 56 bytes, `w` at offset 40, matching `packed_float4`), but
  not on a device. `packed_float4` for weights is deliberate — plain `float4`
  is 16-byte aligned in Metal and would silently corrupt every weight
- camera distance (430), height offset (95) and FOV (58°) are guesses that will
  probably need tuning against a real room
- the +90° model yaw offset is derived from toe-vs-heel positions in the bind
  pose. If he runs backwards, flip that one constant

**Known limitations.**

- Only Room 1 of level 1 loads. The other 22 rooms parse but are not assembled.
- Collision is navmesh containment plus a collision-mesh fallback, not a capsule
  sweep. There is no step height, no slope limit, no ceiling test.
- Room 1's navmesh is only 12 triangles, so walkable area is coarse.
- The ground query is a linear scan over triangles. Fine at this size; it needs
  spatial partitioning before whole levels.
- The single-component root translation track picks its axis by nearest rest
  value. It works for these clips but is a heuristic, not a decode.
- Level geometry renders untextured grey. Materials, texture binding per
  submesh, vertex colour and the `.light` lightmaps are all unused.
- No enemies, no combat, no HUD, no audio, no cinematics, no triggers, no
  checkpoints, no saves, no menus, no jumping, no wall crawling, no web
  mechanics. Enemy and marker positions are parsed and available but nothing is
  spawned.

**This is not a playable port of Total Mayhem.** It is a correct character and
level foundation with verified data decoding. The remaining list in your brief
is essentially untouched from item 14 onward.

---

## Rollback

The apply script copies `NativePort/Sources` to `Sources.backup-<timestamp>`
before touching anything. To revert: delete `Sources`, rename the backup back to
`Sources`, move `_superseded/BDAE324.*` up one level, and re-run the Milestone 3
`apply_fix.command`.

---

## Suggested next step

Get this on the device and tell me what you see. The three things most likely to
need a fix are the shader compiling at all, the model yaw offset, and camera
framing — all small, and all much easier to settle from one screenshot than from
more analysis here.
