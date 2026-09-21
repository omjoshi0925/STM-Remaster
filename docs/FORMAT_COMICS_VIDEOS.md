# Comics and videos

comic1.pack and comic2.pack hold the story pages as plain Truevision TGA
(type 10 RLE, 512x512, 24-bit) named comic_N.tga, 150 per pack. Levels place
Comic trigger nodes (Level 1: 18) that pop pages mid-level; the exact
node-to-page table is undecoded, so the runtime shows pages sequentially per
level. The collection screen (GS_ComicCollection.json) lists 28 volume
titles by string key.

The boot sequence is two loose files in the app bundle, not pack contents:
Gameloft-Logo.m4v then Spiderman-Trailer.m4v (a comic-art motion piece).
Both are skippable in the original and in the port.
