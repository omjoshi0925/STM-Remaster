# The Android HD image

Beyond the engine symbol table (docs/ENGINE_MAP.md), the Android build
provides: editor.pack with Gameloft's own level-editor gizmo meshes
(spawnPoint, waypoint, camera, sound, InfluenceBall), a data.save specimen
and levelRanks.dat for the save and rank formats, logo.mp4, and HD packs
whose sizes differ from iOS (useful for diffing with Tools/compare_packs.py).
Textures there may use different encodings, so treat it as reference, not
as a drop-in for Assets/.
