#!/usr/bin/env python3
"""Decode Gameloft BTEX textures (PVRTC4 skipped, uncompressed 16/32-bit and
plain TGA supported) to PNG contact sheets with a coordinate grid, and crop
labelled rectangles for verification. This is how the HUD sprite table in
Renderer.mm was measured and proven (docs/hud_sprite_verification.png).

usage: inspect_atlas.py sheet <file.tga> [out.png]
       inspect_atlas.py crop  <file.tga> <name:x,y,w,h> [...]  [out.png]
"""
import struct, sys
from PIL import Image, ImageDraw

def decode(path):
    b = open(path, 'rb').read()
    if b[:4] != b'BTEX':                      # plain Truevision TGA
        img = Image.open(path); return img.convert('RGBA')
    hs, w, h = struct.unpack_from('<III', b, 8)
    flags, _, bpp = struct.unpack_from('<III', b, 24)
    p = hs + 8 if b[hs:hs+4] == b'PVR!' else hs
    fmt = flags & 0xff
    img = Image.new('RGBA', (w, h))
    if fmt == 0x10 and bpp == 16:
        px = struct.unpack_from('<%dH' % (w*h), b, p)
        img.putdata([((v>>12)*17, ((v>>8)&15)*17, ((v>>4)&15)*17, (v&15)*17) for v in px])
    elif fmt == 0x12 and bpp == 32:
        img.frombytes(b[p:p+w*h*4]); img = img.convert('RGBA')
    else:
        raise SystemExit('unsupported BTEX fmt %#x bpp %d (PVRTC: use the game)' % (fmt, bpp))
    return img

def sheet(img, out):
    W, H = img.size
    bg = Image.new('RGBA', (W, H), (110,110,110,255))
    for y in range(0, H, 32):
        for x in range(0, W, 32):
            if (x//32 + y//32) % 2: bg.paste((140,140,140,255), (x,y,x+32,y+32))
    bg.alpha_composite(img)
    big = bg.resize((W*2, H*2), Image.NEAREST); d = ImageDraw.Draw(big)
    for g in range(0, W, 64):
        d.line([(g*2,0),(g*2,H*2)], fill=(0,255,255,160)); d.line([(0,g*2),(W*2,g*2)], fill=(0,255,255,160))
        d.text((g*2+2,2), str(g), fill=(255,255,0,255)); d.text((2,g*2+2), str(g), fill=(255,255,0,255))
    big.save(out); print(out)

def crops(img, specs, out):
    S, cw, ch = 3, 200, 200
    cols = 4
    rows = (len(specs)+cols-1)//cols
    sh = Image.new('RGBA', (cols*cw, rows*ch), (90,90,90,255)); d = ImageDraw.Draw(sh)
    for i, spec in enumerate(specs):
        name, rect = spec.split(':'); x, y, w, h = map(int, rect.split(','))
        c = img.crop((x, y, x+w, y+h)).resize((w*S, h*S), Image.NEAREST)
        bg = Image.new('RGBA', c.size, (60,60,60,255)); bg.alpha_composite(c)
        cx, cy = (i%cols)*cw, (i//cols)*ch
        sh.paste(bg, (cx+4, cy+18)); d.text((cx+4, cy+2), '%s %s' % (name, (x,y,w,h)), fill=(255,255,0,255))
    sh.save(out); print(out)

if __name__ == '__main__':
    mode, path = sys.argv[1], sys.argv[2]
    img = decode(path)
    if mode == 'sheet': sheet(img, sys.argv[3] if len(sys.argv) > 3 else 'sheet.png')
    else: crops(img, sys.argv[3:-1] if sys.argv[-1].endswith('.png') else sys.argv[3:],
                sys.argv[-1] if sys.argv[-1].endswith('.png') else 'crops.png')
