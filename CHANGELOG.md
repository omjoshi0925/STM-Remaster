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

## Milestone 5 — Full Level 1, baked lighting, living enemies (2026-08-29)

**Engine (host-verified, 4 suites green):**
- `LevelRoom::loadFullLevel` auto-discovers every room scene: all of level 1
  loads as 93,273 verts / 29,439 tris in uint16-safe visual batches, with
  merged collision (3,180 tris) and navmesh (332 tris).
- Per-vertex colour decoding (ubyte4 attribute) — the level's baked night
  lighting, 300+ distinct colours.
- Enemy roster from the original scenes: 78 placements parsed with type + yaw;
  33/34 ground placements land on the merged navmesh (cross-validation).
- Thug archetypes (bat, knife, molotov + gun/hammer/big mappings) load, bind
  100% of animation channels, and pass humanoid-proportion checks;
  `skinnedAnchor` grounds clips authored at scene offsets (thug idle floats at
  Z≈319 in file space).
- Hero free-roam across the merged level: 5,988 units, 0 off-navmesh frames,
  reaches an original checkpoint by straight-line steering.

**Renderer (compiles against the same sources; not yet visually confirmed):**
- Vertex-lit level batches (uchar4 colour in a 36-byte static vertex,
  layout-verified), far plane extended for full-level scale.
- Up to 40 idle-animating thugs at original spawn points with per-instance
  bone-buffer slots and phase-offset idles, textured from the original packs.

**Format doc:** image/effect/material libraries decoded; texture-binding open
question documented with evidence; config tables inventoried (combat/AI are
data). **Fixed:** enemy clip scene-offset grounding; visual batch splitting.

### Milestone 5.1 — first-device-frame fixes (2026-08-29)
The first rendered M5 frame on iPhone 14 Pro surfaced three bugs invisible to
host tests: (1) MSL rounds a struct containing `float2` to 40 bytes while the
CPU wrote 36-byte static vertices — level geometry exploded into spikes; fixed
with `packed_float2`. (2) The movement forward vector was the negation of the
camera forward — controls felt inverted. (3) Bone matrices were written into a
single MTLBuffer every frame while the GPU still read the previous frame —
torn/folded skinned characters; fixed with triple-buffered bone slots and an
in-flight semaphore. Also: CMakeLists now pins CLANG_ENABLE_OBJC_ARC=YES and
Renderer.mm sheds its vestigial Milestone-2 GameRuntime integration.

## Milestone 6 — the game fights back (2026-08-29)
**Config tables decoded** (u16-length-prefixed strings mixed with u32/float
fields; records begin at UPPERCASE names): EnemysAttributeConfigs.bin yields
per-enemy HP / move speed / vision 7200 / melee range / ranged distance
(THUG_KNIFE 40hp 150u/s 200; THUG_GUN ranged 1200; RHINO 100hp 3000);
AttackIntervalTimeConfigs.bin yields the common 2000 ms melee interval.
**Combat layer (Combat.hpp/.cpp, host-verified end to end):** enemy AI state
machine (idle → chase on navmesh → attack → cooldown → hurt → dead) using
config stats and original clips (idle_at1_idle, idle_hurt_idle,
air_to_onground death); hero punch combo (idle_to_punch_right →
punch_right_to_idle) with front-arc hit detection. Sim proves: chase closes
900→180 units on the real navmesh, attacks respect the 2000 ms interval, a
knife thug dies in exactly ceil(40/10)=4 punches, corpses persist on the death
clip's final frame, punches miss behind the hero.
**Renderer wiring (not yet device-verified):** enemies live in the combat sim
(positions, facing, current clip), tap-to-punch on the move side, HP + alive
count HUD. Placeholder: enemy melee damage fixed at 5 until
EnemysAttackConfigs.bin per-attack damage is decoded.

## Milestone 7 — the original HUD (2026-08-29)
sprites.pack opened: 217 files including interface/mainmenu/level-select/title
atlases, .bsprite module tables, and the game fonts. GS_*.json screen files are
JSON-with-//-comments — all 14 menu screens now parse (buttonConfig /
spriteConfig / textConfig: the menu system is data-driven). New UIKitData
parses .bsprite module tables (validated: 64 interface modules, all inside the
512x512 atlas). Renderer gains a 2D sprite pass (alpha-blended, fully packed
20-byte vertex, frame-ring buffered): health bar tied to hero HP, virtual
stick that appears under the thumb, punch buttons bottom-right, pause
(top-left corner tap), and a module contact-sheet overlay (tap below pause) so
module indices can be mapped from one device screenshot. Module-to-element
mapping is shape-heuristic until that screenshot; menus themselves not yet
built. Requires Assets/sprites/ extracted from sprites.pack.

## Milestone 8 — two levels, start to finish (2026-08-29)

### IMPLEMENTED
GameFlow session system: TITLE -> PLAYING -> DEAD/respawn-at-checkpoint ->
COMPLETE -> next level, cycling Level 1 <-> Level 2 with full unload/reload of
geometry, enemies, and combat state. Original CheckPoint markers drive respawn
and completion. StringTable reads xlsStrings (592 localized entries). Ranged
enemies (gun, molotov, hammer, big) engage from their config rangedRange
instead of walking to melee. Boss placements spawn: Sandman (Level 1) and
Rhino (Level 2) with their original meshes, rigs, config stats (SANDMAN/RHINO
rows now parse - stats table recovers all 25 records), and real attack clips.
Title/death/complete overlays composite the original paper-title art.

### VERIFIED LOCALLY (8/8 host suites green)
Level 2 loads end to end (23k tris, 79 enemies, 15 checkpoints, spawn on
navmesh); every enemy placement in BOTH levels maps to a runtime archetype;
full checkpoint chain completes a level; death returns to the last checkpoint;
gun thug lands ranged hits from ~900 units without ever closing to melee;
both bosses load, animate, and fit the 40-bone budget (21 and 36 joints).

### REQUIRES DEVICE VALIDATION
Level switching at runtime (buffer teardown/rebuild), flow overlays and paper
art, boss rendering scale/anchoring, ranged attack feel, checkpoint radius.

### KNOWN LIMITATIONS
Completion = visiting all original checkpoints (the real trigger/cinematic
completion chain is not yet decoded). Bosses use the generic melee/ranged AI,
not original boss phases/QTEs. Title/results text uses the system font with
original paper art (original font-atlas rendering and level-name string keys
not yet mapped). Defeated enemies stay dead through respawns (in-memory
checkpoints only). Enemy damage still flat 5.

### NOT YET IMPLEMENTED
Cinematics (.cff), audio, comic intro sequences, Trigger/TriggerRestore/
CameraArea/WebGrabPoint/Hostage/DestroyableObject runtimes, room streaming,
level textures (binding still unresolved), menus beyond the flow overlays,
wall-crawling, QTEs, score/rank.

## Milestone 9 — the living-level engine layer (2026-08-29)

### IMPLEMENTED
Level parser now yields the full original prop inventory: 232 placements in
Level 1 (156 destructibles, 46 animated objects, 10 cars, 10 hostages, 10
statics) plus 93 Bonus pickup positions, each with mesh path (normalized,
case-resolved, sibling-pack fallback) and absolute world transform.
EnemysAttackConfigs.bin parses: 90 named attack rows with real damage values.
Hero combat is a 3-hit combo chain on original clips (punch_right ->
far_attack -> backflip_kick, 10/10/15) with tap-to-chain windows. Enemies take
navmesh-gated knockback on every hit. takeHit carries attacker position.

### VERIFIED LOCALLY (9/9 suites green)
All 232 props parse; 50/52 unique prop meshes load through the BDAE loader
(8 skinned); combo chains through all three stages and drops a 40 hp thug;
knockback shoves 70 units and stays on walkable ground; attack table rows
recover plausible damage (with named samples); all prior suites unchanged.

### KNOWN LIMITATIONS
Enemy->attack-row linkage undecoded (representative melee damage still used).
break_bankwall/break_wall ship only as *_anim variants (destruction states -
future). Bonus visuals unidentified (position-only). Hero punch damage values
are placeholders pending the MC attack table.

### NOT YET IMPLEMENTED (next: Milestone 9.1 renderer wiring)
Rendering of props/bonuses/sky, destructible break + score, spider-sense
rings, bonus pickup, hit-reaction visuals. Then: textures, cinematics, audio,
boss phases, camera areas, wall traversal.

## Milestone 9.2 — binary reconnaissance (2026-08-30)
Android HD build analyzed: libspiderman.so keeps 56k named symbols — a map of
the whole engine (Player 277 methods, CLevel, CEnemy, CBoss, CCinematicThread,
VoxSoundManager, IBehavior* AI, CTutorial). Texture-binding mechanism located
in CMaterial::prepareMaterial: name-based image lookup + name-substring layer
classification (lightmap / alphatest). Tools/disasm_libspiderman.py added
(capstone+pyelftools, resolves calls, literal pools and GOT-relative strings).
editor.pack (Gameloft editor gizmo meshes) catalogued for marker visuals.

## Milestone 10 — the textured city (2026-08-30)

### IMPLEMENTED
The effect->texture binding, found by cross-reading libspiderman.so and the
data: effect record +76/+80 = per-layer UV-set indices, +84/+88 = per-layer
image indices into the file's image table; the image entry's *path* field
carries the real on-disk file name (the filename field is Max's slot id). A
second layer whose name contains "lightmap" is the lightmap on UV set 1;
names containing "alphatest" are alpha-tested (both rules confirmed in
CMaterial::prepareMaterial). Loader now resolves diffuse/lightmap per submesh,
reads the second UV set, and the level builds texture-keyed visual batches.
Renderer: 44-byte static vertex with uv2, per-batch diffuse + lightmap (M2
modulate) + alpha-test, texture index across every */textures_bin under
Assets (the original mounts all packs), cached PVRTC loads.

### VERIFIED LOCALLY (10/10 suites green)
958 layer references in Level 1, zero out-of-range indices; thug -> thug.tga;
39 batches preserve all 29,439 triangles; 36 distinct original textures; the
lightmap layer carries a distinct second UV set; 23/38 diffuse files resolve
with only four packs mounted (more with all level packs extracted).

### REQUIRES DEVICE VALIDATION
PVRTC decode of every level texture format variant, lightmap intensity (M2
assumed), alpha-test threshold, texture memory with all packs mounted.

### KNOWN LIMITATIONS
Spider-Man's own effect carries no layer arrays (engine falls back to image 0;
the renderer already does the same). Textures shipped only in level packs not
extracted into Assets render white with baked vertex colour until extracted.

### Milestone 10.1 — first textured frame follow-ups (2026-08-30)
Device log confirmed the binding (all 284 textures indexed, 13 unresolved
by exact name). Texture loader now decodes every format the packs ship:
PVRTC4 (square, compressed upload), uncompressed BTEX RGBA4444 / RGBA5551 /
RGB565 / RGBA8888 (expanded to RGBA8 — interface.tga and mainmenu.tga are
RGBA4444, which is why the HUD never drew), and plain Truevision TGA types
2/10 (paper_title art). Fuzzy name resolution mirrors the engine's %s_%d
variants (trailing variant digits, leading pack/slot digits). Diffuse layers
that declare UV set 1 are sampled with the second UV set. Health bar moved
below the debug label.

## Milestone 11 — the original presentation (2026-08-30)

### IMPLEMENTED
Boot flow now mirrors the original: comic-panel montage (the real comic pages
from comic1/comic2.pack, plain TGA, slow push-in + drift, tap to advance,
SKIP zone top-right, auto-advance) -> chapter card with the original level
name from xlsStrings (STR_LEVELNEW_n_NAME) -> gameplay. Skybox: the level's
lvl01_sky.bdae with its own textures, drawn first and following the camera.
Daylight: baked vertex colours applied as a 2x modulate on textured level
batches (measured mean 0.66), untouched for characters and lightmapped
geometry. HUD rebuilt from the atlas's real sprites (rectangles measured by
alpha-blob detection on the decoded RGBA4444 interface.tga): pause, portrait,
health bar bg/fill, web meter, joystick puck + ring, three action buttons in
the reference layout.

### KNOWN LIMITATIONS
Level 1's montage starts at page 1 (certain); later levels' start pages are
estimated from Comic-node share. The 18 in-level Comic trigger nodes do not
yet pop pages mid-level. Chapter card and SKIP use the system font (original
font atlases are RGBA4444 and now decode; glyph tables next). No in-engine
intro cinematic yet (camera_lv1_* / .cff). Button glyphs not yet overlaid.
Gameloft-Logo.m4v / Spiderman-Trailer.m4v boot videos not played.

## Milestone 12 — the streets fill up (2026-08-30)

### IMPLEMENTED
All original prop placements render: 232 in Level 1 (lampposts, cars, buses,
garbage cans, hostages, billboards...) as static textured geometry, archetypes
shared per mesh file, real textures through the Milestone 10 binding, alpha
test where the texture name says so, 9 km distance cull. Destructibles break
(disappear) when a punch lands in front arc (+50). The 93 original Bonus
positions show as floating spider tokens and collect on contact (+25).
Spider-sense: a pulsing red ring over every enemy that has noticed Spider-Man.
Score in the HUD line (+10 per hit).

### REQUIRES DEVICE VALIDATION
Prop scale/orientation from the scene transforms, per-frame cost of ~500
small draws, skinned props (hostages) in bind pose, bonus token look.

### KNOWN LIMITATIONS
No break animation/debris for destructibles (break_*wall_anim assets exist
for that); bonus visuals are a stand-in token; hostages are static.

### Milestone 12.1 — real boot, hardening (2026-08-30)
Correction: the original does not run a comic montage at boot; it plays
Gameloft-Logo.m4v then Spiderman-Trailer.m4v (the comic-art motion piece).
Boot now plays both with AVPlayer (tap = skip clip) from Assets/videos/, then
the chapter card. Comic pages remain for mid-level beats (Comic nodes) and
the collection. Input hardening after a movement report: pause zone shrunk
to 44 pt, contact-sheet toggle narrowed, pause reset on level load, FPS and
PAUSED readout in the debug line.

### Milestone 12.2 — authentic HUD restoration (2026-08-30)
Root cause of the "red circles everywhere": drawHUD used atlas rectangles
chosen by a blob scan; the rectangle used for bonus tokens (and the portrait)
was actually a red button face beside a "6+" glyph, drawn at all 93 Bonus
positions. Fix: the interface.tga atlas was decoded to an image and inspected
(docs/interface_atlas_sheet.png, docs/hud_sprite_verification.png); every HUD
element now uses its verified original sprite: pause bubble (478,194),
portrait (467,65), health frame (279,446) + green honeycomb fill (92,476),
web-strand meter (0,270), blue knob (177,212), ticked ring (345,4), red button
face (59,212), white fist/dodge/web glyphs (355,303 / 471,303 / 230,302) with
the red fist for the pressed state, gold spider token (262,212) for bonuses.
Score popups ("+N") use the original outlined font font_outline_big.tga
(digit table segmented from the atlas). Layout mirrors the reference frame in
768-pt space anchored to the screen edges; button touch circles match.
Developer status line is off unless TM_DEBUG_HUD=1. Assets/bundle audit:
interface.tga, interface.bsprite, font_outline_big.tga are all in the
SpiderManTotalMayhem resources; the break was renderer-side only.

### Milestone 12.3 — the original font (2026-08-30)
font_outline_big.tga glyph table segmented from the atlas (55 glyphs,
uppercase + digits + punctuation) and proven by rendering (docs/font_proof.png).
All flow text now uses it: chapter card (original level name from xlsStrings),
TAP TO START/RETRY/CONTINUE, SKIP, SPIDER-MAN IS DOWN, LEVEL COMPLETE, the
"+N" popups (corrected '+' glyph) and a "N COMBOS!" banner. UILabels remain
only as a fallback if the font atlas fails to load.


## Milestone 13 — ten focused commits (2026-09-10)
Per-enemy damage on EnemyStats (bosses 12); corpses settle away after death;
checkpoint reach feedback + health restore (assumption documented); web
attack on the web button driven by a regenerating web-power meter; PAUSED
overlay in the original font; spider-sense limited to 2600 u with distance
fade; mid-level comic beats from the original 18 Comic trigger nodes
(sequential pages after the intro — node-to-page table still undecoded);
milestone 12 host suite (11 suites total); Tools/inspect_atlas.py; README.

## Milestone 14 — the original sound (2026-09-10)

### IMPLEMENTED
Audio, end to end from the original data. `VoxSounds.bin` decoded: a 493-row
event table mapping names (`M_DOWNTOWN_CALM`, `SFX_PUNCH_IMPACT_1`) to clip
paths plus flags (`Audio.hpp/cpp`). `sounds.pack` ships 501 RIFF WAVs in IMA
ADPCM (format 17, 4-bit, 22-32 kHz); a decoder expands them to PCM16 in
plain C++. `TMAudioManager` (AVAudioEngine: one looping music node, an
8-voice effect pool, PCM buffer cache) plays by event name. Hooks: level
music on start, punch impact/swoosh (impact_2 during combos), hero hurt,
death + M_LOSE, web throw, bonus pickup (SFX_ORBS_COLLECT), checkpoint
(SFX_SPIDER_LOGO_IN), music stop on completion.

### VERIFIED LOCALLY (13 suites green)
All 493 events parse; all 493 referenced clips exist on disk; music bed
decodes to 123.44 s at 32 kHz stereo; effects decode at 22.05 kHz mono with
real signal (rms 3447, peak 20475 on the punch impact); every event name the
renderer uses is checked to exist in the table.

### REQUIRES DEVICE VALIDATION
Playback latency, mixing balance (music at 0.55), memory of decoded buffers
(a full music bed is ~15 MB of PCM), AVAudioSession behaviour with the boot
videos.

### KNOWN LIMITATIONS
Event choices for gameplay moments are ours (BehaviorSoundMapList.bin and
MC_SOUND.bin hold the original per-state mappings and are not decoded yet);
music is one looping bed per level rather than the original calm/action
crossfade; no 3D panning; enemy voice/death clips not yet wired per
archetype.

### ASSET NOTE
Requires `sounds.pack` extracted to `Assets/sounds/` (146 MB; the app bundle
grows accordingly).
