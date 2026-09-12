# Engine map from the Android build

`libspiderman.so` (Android HD 8 MB) ships a **full symbol table: 56,151
named functions** - a readable reference for every system we still have to
build. No original source exists in this repo; these are observations of the
binary's structure.

Class inventory (method counts): `Player` 277, `CLevel` 154,
`CCinematicThread` 110, `CEnemy` 94, `VoxSoundManager` 76, `CBoss` 60, plus
`IBehaviorBase`-family AI, `CAIEntityManager`, `CTutorial`, `CTrain`,
`CImageFileList` (animated textures).

## Texture binding (solved, cross-checked with data)
`CColladaFactory::createMaterial` -> `CMaterial::CMaterial` ->
`CMaterial::prepareMaterial(IRootSceneNode*)`: per texture layer, a param
struct (name at +0, type flag 1 at +0xC) resolves the image **by name**
through `IRootSceneNode::getLibraryImage` ->
`CColladaDatabase::constructImage/getImage`. Layers are then classified by
image-name substrings: `lightmap` -> lightmap layer (material 0x1B, second
UV set), `alphatest` -> alpha test; one hardcoded case for the
`levelnew_01_01` atlas. On disk this corresponds to effect record fields
+76/+80 (per-layer UV-set indices) and +84/+88 (per-layer image indices) -
see FORMAT_BDAE324.md.

## Reading the binary
`Tools/disasm_libspiderman.py` (capstone + pyelftools) resolves call
targets, Thumb/ARM literal pools, and **GOT-relative strings** (position-
independent code hides string operands as `GOT + negative offset`; compute
against the `.got` section address). That technique is how the three
classification strings above were recovered.

## Leads for future systems
Audio: `VoxSoundManager` + `VoxSounds` configs + sounds.pack. Cinematics:
`CCinematicThread` + `.cff` files + `camera_lv1_*` BDAEs. Boss logic:
`CBoss` + per-boss meshes (`sandman_lv1_boss`, `Rhino_Lv1_End`). Save/rank:
`data.save` specimen + `levelRanks.dat` in the Android image.
