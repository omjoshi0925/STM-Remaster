# Glossary

- BDAE / BRES: Gameloft binary model container, version 0.0.0.324 here
- .pack / GBMP: ZIP archive with local headers renamed GBMP
- BTEX: texture container wrapping PVRTC or uncompressed pixels
- PVRTC: PowerVR compressed texture format, must be square on Metal
- RGBA4444: 16-bit uncompressed pixels, used by UI atlases and fonts
- .irr: UTF-16 XML scene file, one per room plus a root
- !GameType: node attribute naming what a scene node is to the runtime
- Trigger / Cinematic / CameraArea: authored scripting nodes, see FORMAT_TRIGGERS.md
- .cff: cinematic script referenced by Cinematic nodes, not yet decoded
- Vox / VoxSounds: the original audio event system and its event table
- IMA ADPCM: 4-bit audio compression used by every clip in sounds.pack
- .bsprite: sprite layout descriptor for a .tga atlas
- xlsStrings: localisation tables, keys paired to values by line
- navmesh: walkable surface mesh per room, drives ground and movement
- lightmap: baked lighting texture sampled on the second UV set
- archetype: enemy type keyed by scene GameType and stats row name
- ledger: the five-section status block in CHANGELOG.md
