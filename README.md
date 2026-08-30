# STM-Remaster

Native ARM64 iOS reimplementation of **Spider-Man: Total Mayhem** (Gameloft, 2010),
rebuilding the original 32-bit game engine for modern iPhones using Metal —
reverse-engineered BDAE assets, animation, level data, and gameplay systems.
No emulation, no original code: the engine is new, the data is original.

## Current state (Milestone 5)

Verified headlessly against real game data (all suites in `hosttests/` pass):

- **BDAE 0.0.0.324 loader** — geometry, per-vertex colours, skeletons, skinning
  (max-3 influences), scene instancing, full animation library (242 Spider-Man
  clips on a shared timeline), documented in `FORMAT_BDAE324.md`.
- **Entire Level 1** — all room scenes auto-discovered and merged: 29,439 visual
  triangles in uint16-safe batches, 3,180 collision triangles, 332 navmesh
  triangles, 55 markers, 78 enemy placements, original spawn point.
- **Baked lighting** — the level's per-vertex colour channel (300+ distinct
  colours) drives the night-time street look.
- **Characters** — Spider-Man with idle/walk/run state machine and navmesh-gated
  movement; thug enemy archetypes (bat / knife / molotov / gun / hammer / big)
  load, bind all animation channels, and idle-animate at their original spawns.
- **Metal renderer** — GPU skinning, PVRTC textures from the original packs,
  vertex-lit level batches, per-enemy bone buffers.

**Not yet device-confirmed:** everything in the previous line renders in Xcode
builds but the Milestone 5 frame has not been visually verified on the phone.
**Known open problem:** the static effect→image texture binding for level
geometry (see `FORMAT_BDAE324.md` §Open questions) — level renders vertex-lit
untextured until solved.

## Repository layout

    NativePort/          CMake iOS app: sources + plist + icons (no game data)
    hosttests/           Headless verification suites + run_host_tests.sh
    scripts/             apply_to_workbench.sh - sync repo -> local workbench -> Xcode
    Tools/               Python: unpack .pack (GBMP), parse BDAE/irr, prep from IPA
    analysis/            ARMv7 binary reconnaissance (symbols, ObjC metadata)
    FORMAT_BDAE324.md    The reverse-engineered format, with evidence
    CHANGELOG.md         Milestone history
    GIT_COMMANDS.md      Copy-paste workflows (push / pull / build)

## The game assets are NOT in this repo

`Assets/` and `OriginalPacks/` are gitignored: the content is Gameloft/Marvel
copyrighted material (and exceeds GitHub's 100 MB file limit). They live only in
the local workbench, extracted from a legally owned IPA via
`Tools/prepare_from_ipa.py`. This repo contains only original code and research.

## Build

    git clone https://github.com/omjoshi0925/STM-Remaster.git
    cd STM-Remaster
    ./scripts/apply_to_workbench.sh ~/Downloads/SpiderMan_Native_ARM64_Port_Workbench

Xcode opens; select the **SpiderManTotalMayhem** scheme, your iPhone, Run.
Host verification (no Xcode needed): `./hosttests/run_host_tests.sh <Assets path>`.
