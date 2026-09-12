# .pack container format

Every Gameloft data archive in the game (`levelnew_NN.pack`, `entities.pack`,
`configs.pack`, `sprites.pack`, `comic1/2.pack`, `xlsStrings.pack`,
`sounds.pack`, `photo1/2.pack`) is a **standard ZIP archive with the local
file header signature rewritten**: every `PK\x03\x04` becomes ASCII `GBMP`.
The central directory is untouched, which is why some generic unzippers can
list but not extract them.

Recovery is a byte-for-byte swap:

    python3 - <<'PY'
    d = open('name.pack','rb').read()
    open('name.zip','wb').write(d.replace(b'GBMP', b'PK\x03\x04'))
    PY
    unzip -o name.zip -d out/

Notes
- The swap is safe: `GBMP` does not occur as payload inside the deflate
  streams of any pack in the 1.0.1 iOS build (verified by round-tripping all
  20 packs).
- Pack contents use mixed path casing; resolve case-insensitively
  (`resolveCaseInsensitive` in `Level.cpp`).
- The original app mounts every pack into one namespace: level scenes freely
  reference meshes and textures that ship in *other* level packs, so tooling
  and the runtime must search all mounted roots.
