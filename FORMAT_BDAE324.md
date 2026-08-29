# Gameloft BDAE `0,0,0,324` — verified structure

Every offset below was checked byte-for-byte against the original
Spider-Man: Total Mayhem iOS assets. Each claim has a matching assertion in
`hosttests/`. Nothing here is inferred from a later 64-bit BDAE revision.

**BDAE is binary COLLADA.** Once you see `spiderman-mesh-skin`, `#spiderman-mesh`
and `#_1_-_Default` in the string table the whole layout follows the COLLADA
document model: images, effects, materials, geometries, controllers, animations,
visual scenes.

## File header (32 bytes)

| off | field |
|-----|-------|
| 0   | `BRES` |
| 4   | endian marker (`0xFFFE`) |
| 6   | version field |
| 8   | header size (32) |
| 12  | file size — must equal the real file length |
| 16  | relocation count |
| 20  | relocation table offset |
| 24  | string table offset |
| 28  | data offset |

The relocation table lists the file offset of every pointer field, and it abuts
the string table exactly (`relocOff + 4*numReloc == strOff`). Strings are stored
as a 4-byte length followed by the bytes, padded to a 4-byte boundary.

## Root library table (offsets relative to `dataOff`)

| off | contents |
|-----|----------|
| 0   | `char*` version string, `"0,0,0,324"` |
| 16 / 20  | animation channel count / pointer |
| 28 / 32  | animation clip count / pointer |
| 52 / 56  | image (texture) count / pointer, 20-byte stride |
| 60 / 64  | effect count / pointer |
| 68 / 72  | material count / pointer |
| 76 / 80  | geometry count / pointer, 16-byte stride |
| 84 / 88  | controller (skin) count / pointer |
| 108 / 112 | visual scene count / pointer |
| 116 / 120 | scene count / pointer |

## Geometry (16-byte entry)

`+0` id (`"Box01-mesh"`), `+4` name, `+12` pointer to the mesh record.

Mesh record: `+4` vertex count, `+8` vertex descriptor, `+12` submesh count,
`+16` submesh table, `+20` bbox min (3 floats), `+32` bbox max (3 floats).

### Vertex descriptor

`+0` stride, `+4` attribute count, `+8` byte-offset array, `+12`/`+16` data-type
array, `+24` third array (always 0 here), `+28` vertex data pointer.

The **component count is not stored**. It is the gap from an attribute's offset
to the next attribute in *offset order*, or to the stride for the last one.
Data type `6` = float, `1` = packed ubyte4 colour.

Observed layouts:

| asset | stride | offsets | types | meaning |
|-------|--------|---------|-------|---------|
| `spiderman_mesh` | 32 | 0, 12, 24 | 6, 6, 6 | pos3, normal3, uv2 |
| `collision01` geo0 | 32 | 0, 12, 24 | 6, 6, 6 | pos3, normal3, uv2 |
| `collision01` geo1 | 52 | 0, 12, 48, 24, 32, 40 | 6, 6, 1, 6, 6, 6 | pos3, normal3, colour, uv2 ×3 |
| `Nav01`, `geometry01` | 24 | 0, 20, 12 | 6, 1, 6 | pos3, colour, uv2 — **no normals** |

Level geometry ships no normals (it is vertex-lit; see the `.light` files), so
the loader generates smooth normals from the faces.

### Submesh record — **64 bytes, not 32**

`+4` material name, `+8` triangle count, `+24` index count, `+28` uint16 index
pointer, `+36`..`+59` per-submesh bbox.

A 32-byte stride happens to work for single-submesh models like Spider-Man,
which is why the earlier loader appeared correct while silently producing
garbage submeshes on level geometry (Room 1 read as 25,996 triangles instead of
the real 1,871).

## Visual scene nodes — 80-byte stride

| off | field |
|-----|-------|
| 0   | `char*` id (`"Bip01_Pelvis-node"`) |
| 4   | `char*` name (`"Bip01_Pelvis"`) |
| 8   | `char*` sid (`"Bone19"`, empty for non-joints) |
| 12  | translation, 3 floats |
| 24  | rotation quaternion `(x,y,z,w)`, 4 floats |
| 40  | scale, 3 floats |
| 52  | flag, always 1 |
| 56  | child count |
| 60  | child node array |
| 64  | instance count |
| 68  | instance array, 40-byte stride |

The quaternion offset was pinned down by the fact that all 46 Spider-Man nodes
are **exactly** unit length at `+24` and nowhere else.

**Rotation handedness.** BDAE stores rotations in the 3ds Max row-vector sense.
The matrix must be built from the *conjugate* of the stored quaternion. Proof:
reconstructing each joint's world transform and comparing against the file's own
inverse bind matrices gives a max element error of **6.0e-4** with the conjugate
and 50–150 without it.

Instances carry a COLLADA URL string (`#Box01-mesh`), not a pointer, so
geometry is resolved by matching against the geometry entry's id.

## Skin controller

| off | field |
|-----|-------|
| 4   | id (`"spiderman-mesh-skin"`) |
| 12  | source url (`"#spiderman-mesh"`) |
| 16  | bind shape matrix, 16 floats, column-major |
| 80 / 84 | joint count / joint sid array |
| 88 / 92 | inverse-bind float count (`joints*16`) / matrix array |
| 96 / 100 | weight palette count / float array |
| 104 / 108 | vertex count / uint8 influence-count array |
| 112 / 116 | uint16 element count / `(jointIndex, weightIndex)` pair array |

Spider-Man: 38 joints, 12 distinct weights, 641 vertices, 974 influences
(1948 uint16s), max 3 influences per vertex. Weights sum to 1.0 with **zero**
deviation.

Joints map to nodes by `sid`, so no matrix matching is needed. Exactly 38 nodes
carry a Bone sid; the 8 that do not are `Bip01`, `Bip01_Footsteps`,
`Dummy_center`, `spiderman`, and the four `FX_*` helpers.

Skinning matrix: `world(joint) * inverseBind(joint) * bindShape`.

## Animation

`spiderman_anim.bdae` holds 46 channels and 242 clips.

**Channel entry, 36 bytes:** `+0` name (`"<node-id>-rotation"` or
`"-translation"`), `+8` pointer to the source record.

**Source record:** `+4` key count, `+8` uint32 millisecond time array, `+16`
total float count, `+20` float value array.

Components per key = `floatCount / keyCount`, so the format is self-describing:
4 = quaternion, 3 = vec3, 1 = single axis. Spider-Man has 43 rotation channels
and 3 translation channels; only `Bip01`, `Dummy_center` and `FX_RH` translate.

**Clip entry, 12 bytes:** `+0` name, `+4` start ms, `+8` end ms — indices into
one shared timeline. The timeline runs 0 → 163,533 ms, which matches the maximum
clip end exactly. Times are strictly monotonic.

Sample clips: `idle_stand` 3133–4466, `idle_stand_non_combat` 4500–7166,
`run` 8033–8833, `walk` 15033–16100, `punch_right_to_idle` 32400–32866.

## `.pack` archives

ZIP containers with the local-file signature `PK\x03\x04` replaced by `GBMP`.
Confirmed by signature counts: `entities.pack` has 531 `GBMP` markers and
exactly 531 `PK\x01\x02` central-directory records, and zero `PK\x03\x04`.

## `.irr` room scenes

UTF-16LE Irrlicht scene XML. Each `<node>` carries `Id`, `Position`,
`Rotation` (quaternion), `Scale`, `AbsoluteTransformation` (16 floats) and
`Visible`, plus a `<userData>` block with `Name`, `!GameType`, `MeshFile` and
`#ParentID`.

Level 1 `!GameType` histogram: 193 CamCtrlPoint, 156 DestroyableObject,
93 Bonus, 77 Cinematic, 49 Trigger, 46 AnimatedObject, 44 CameraArea,
27 MeleeThugEnemy_bat, 23 each of Geometry / Collisions / NavMesh / WayPoint,
21 MeleeThugEnemy_knife, 16 each CheckPoint / WebGrabPoint, 11 RangeThug_molotov,
10 Hostage, 3 Boss_Rhino, 2 Boss_Sandman, 1 SpiderMan, 1 SpawnPoint.

`MeshFile` values use Windows separators and inconsistent case
(`.\meshes_bin\nav01.bdae` for a file actually named `Nav01.bdae`), so paths
need case-insensitive resolution on a case-sensitive filesystem.

## World conventions

Z-up, roughly 1 unit = 1 cm (Spider-Man's bbox is 174 tall). Level 1 Room 1
spans X 5320..23034, Y −20530..−2331.

The rig's forward axis is **−Y**: at the bind pose `Bip01_L_Foot` is at
y = +4.95 and `Bip01_L_Toe0` at y = −8.30. A yaw of 0 meaning "facing +X"
therefore needs a +90° model correction.
