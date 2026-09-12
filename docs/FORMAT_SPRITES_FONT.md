# UI atlases and fonts (measured tables)

`interface.tga` (512x512 RGBA4444) holds the entire in-game HUD. The
`.bsprite` module table is still undecoded; the rectangles below were
**measured on the decoded image and verified visually**
(docs/interface_atlas_sheet.png, docs/hud_sprite_verification.png) and are
the source of truth for `Renderer.mm`:

| element            | x,y,w,h            |
|--------------------|--------------------|
| pause bubble       | 478,194,30,36      |
| Spider-Man portrait| 467,65,41,62       |
| health frame       | 279,446,139,18     |
| health fill (green)| 92,476,133,16      |
| web-strand meter   | 0,270,212,32       |
| joystick knob      | 177,212,50,52      |
| joystick ring      | 345,4,58,58        |
| button face (red)  | 59,212,57,52       |
| fist glyph / hot   | 355,303,38,40 / 395,303,35,40 |
| dodge glyph        | 471,303,30,36      |
| web glyph          | 230,302,38,40      |
| spider token       | 262,212,52,52      |

Also present: boss portraits, tutorial dialog box, COMBO word art in six
languages, red score digits, small yellow digits.

## font_outline_big.tga
The outlined UI font. 55 glyphs across four rows (punctuation y=3 h=29;
digits y=33 h=23; A-O y=73 h=23; P-Z y=113 h=23), segmented from the atlas
alpha and proven by rendering (docs/font_proof.png). The full table is
embedded as `kFont[]` in `Renderer.mm`; regenerate with
`Tools/inspect_atlas.py` + the segmentation snippet in that file's header.
`interface_2.tga` is a 1024x1024 RGBA4444 sheet with tutorial art.
