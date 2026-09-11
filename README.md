# STM-Remaster

A from-scratch native ARM64 rebuild of **Spider-Man: Total Mayhem** (Gameloft,
2010, iOS) as a modern Metal app for current iPhones, driven entirely by the
original game's own data files.

**This repository contains no game assets.** It holds original code, format
documentation, and tests. To run anything you must own the original game and
extract its packs into `Assets/` yourself (see *Getting started*). All asset
containers (`*.pack`, `*.ipa`, `*.apk`, media) are gitignored.

## What works today
- BDAE `0.0.0.324` model format decoded: meshes, dynamic vertex layouts,
  skinning, animation clips, scene graphs (`FORMAT_BDAE324.md`)
- Full levels assembled from the original `.irr` scenes: geometry, collision,
  navmesh, spawns, checkpoints, 232 prop placements, 93 bonuses (Level 1)
- Texture binding solved (effect image-index arrays + name classification,
  confirmed against the Android build's `CMaterial::prepareMaterial`):
  textured city, lightmaps on the second UV set, alpha test
- Game flow: boot videos, chapter card, checkpoints/respawn, completion,
  Level 1 <-> Level 2; mid-level comic beats from the original Comic nodes
- Combat: config-driven enemy stats/AI (melee + ranged), 3-hit combo,
  knockback, web attack with web-power meter, bosses spawn with real assets
- Authentic HUD from `interface.tga` (every sprite rectangle verified
  visually - `docs/`) and all UI text in the original outlined font
- 11 host-side test suites validate parsers and gameplay logic against the
  real game data before anything ships to a device (`hosttests/`)

## Getting started
1. Clone, and place your legally obtained packs in
   `<workbench>/OriginalPacks/` (see `GIT_COMMANDS.md` for the layout).
2. Extract packs into `Assets/` (each `.pack` is a ZIP with `GBMP` local
   signatures; swap to `PK\x03\x04` and unzip) - the CHANGELOG documents the
   exact commands per milestone.
3. `cmake -G Xcode NativePort` into `build-ios/`, open, set your team, run.
4. Verify your extraction: `hosttests/run_host_tests.sh <Assets path>`.

## Layout
- `NativePort/Sources/` - the engine: BDAE loader, level assembly, combat,
  game flow, Metal renderer
- `hosttests/` - data-driven verification suites (run on macOS, no device)
- `Tools/` - format explorers: BDAE dumper, libspiderman.so disassembler,
  atlas inspector
- `docs/` - visual proofs (atlas sheets, HUD sprite verification, font proof)
- `FORMAT_BDAE324.md` - the reverse-engineered format reference
- `CHANGELOG.md` - per-milestone ledger: implemented / verified locally /
  device-pending / known limitations / not yet implemented

## Not yet implemented
Audio (VoxSoundManager), `.cff` in-engine cinematics, original boss phases &
QTEs, wall traversal & web swing, camera areas, room streaming, menus beyond
the flow screens, saves/ranks. See the CHANGELOG ledger for the live list.
