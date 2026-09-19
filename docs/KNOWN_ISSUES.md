# Known issues

Open
- Movement reported as not working on one device build; not yet reproduced, needs a description of what dragging does and the on-screen fps value.
- A handful of Level 1 texture names do not ship under those names in the iOS packs (Car_01, 42_mall_glass, 05_atlas_A); those surfaces fall back to baked vertex colour.
- Trigger and camera-area extents are not stored on the nodes; volumes use conservative boxes and control-point bounds.
- Comic pages for levels after the first use estimated start pages.
- Audio event choices per moment are ours; the slot linkage in BehaviorSoundMapList.bin and MC_SOUND.bin is undecoded.

Fixed
- Red circles wallpapering the scene: wrong atlas rectangles, replaced with measured ones.
- HUD never appearing: interface.tga is RGBA4444, loader only handled PVRTC.
- Level too dark: baked vertex colours are a 2x modulate.
- Enemies dropping the wrong stats rows: 2-byte string alignment in the config parser.
