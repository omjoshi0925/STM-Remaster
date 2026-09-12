# Texture formats

## BTEX container
Header: magic `BTEXpvr` (7 bytes), `u32 headerSize @8`, `u32 width @12`,
`u32 height @16`, `u32 flags @24`, `u32 dataLength @28`, `u32 bpp @32`.
Payload at `headerSize`, optionally preceded by an 8-byte `PVR!` stamp.
`flags & 0xff` is a legacy PVR pixel-format code; `flags & 0x8000` = alpha.

Formats present in the 1.0.1 packs:
| code | bpp | meaning        | used by                              |
|------|-----|----------------|--------------------------------------|
| 0x19 | 4   | PVRTC 4bpp     | all level/character textures (square)|
| 0x10 | 16  | RGBA4444       | UI atlases, fonts (interface.tga...) |
| 0x11 | 16  | RGBA5551       | occasional UI                        |
| 0x13 | 16  | RGB565         | occasional                           |
| 0x12 | 32  | RGBA8888       | rare                                 |

RGBA4444 channel order is R in the high nibble .. A in the low nibble
(validated against interface.tga alpha statistics).

**Metal constraint:** PVRTC textures must be square; the packs comply for
0x19 assets. Uncompressed variants are expanded to RGBA8 at load.

## Plain Truevision TGA
Comic pages (`comic_N.tga`, 512x512x24 RLE type 10) and paper/title art are
ordinary TGA files despite the shared extension - detect by the missing
`BTEX` magic. Handle types 2 and 10, 24/32-bit, both origins.

## Name resolution
Image entries carry a *filename* field (3ds Max slot id like
`_7_window01.tga`) and a *path* field whose basename is the real on-disk
file (`07_window01.tga`). Resolve: path basename, then filename, then fuzzy
(`%s_%d` variants: strip trailing variant digits; strip leading pack/slot
digits and suffix-match). This mirrors the engine's loose resolution.
