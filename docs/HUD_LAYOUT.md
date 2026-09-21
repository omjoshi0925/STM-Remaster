# HUD layout

All positions are in a 768 pt tall reference space, scaled by screen height,
right-side elements anchored to the screen's right edge. Sprites are the
measured interface.tga rectangles in docs/FORMAT_SPRITES_FONT.md.

| element | position | size |
|---|---|---|
| pause bubble | 11, 16 | 48 x 56 |
| portrait | 74, 20 | 80 x 120 |
| health frame | 170, 34 | 404 x 40 |
| health fill | 178, 40 | 386 x 28, scaled by HP |
| web meter | 172, 80 | 356 x 34, scaled by web power |
| joystick ring | 175, H-135 | radius 92 |
| joystick knob | ring centre + stick x 0.75 R | radius 64 |
| punch button | W-266, 511 | radius 70 |
| dodge button | W-96, 511 | radius 70 |
| web button | W-319, 665 | radius 70 |
| boss bar | centred, y 128 | 520 x 28 |
| combo art | right, y 0.28 H | height 62 |

Popups rise 90 pt over 1.3 s in the outlined font.
