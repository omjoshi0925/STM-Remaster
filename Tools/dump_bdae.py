#!/usr/bin/env python3
"""Inspect a BDAE 0.0.0.324 (Gameloft BRES) file: header, tables, images,
materials, scene nodes, skin and animation clips. Reference implementation
of the notes in FORMAT_BDAE324.md.

usage: dump_bdae.py <file.bdae> [--images] [--clips] [--nodes] [--effects]
"""
import struct, sys

class BDAE:
    def __init__(self, path):
        self.b = open(path, 'rb').read()
        if self.b[:4] != b'BRES':
            raise SystemExit('%s: not a BRES/BDAE file' % path)
        (self.endian, self.ver, self.headerSize, self.fileSize,
         self.numReloc, self.relocOff, self.strOff,
         self.dataOff) = struct.unpack_from('<HHIIIIII', self.b, 4)

    def u32(self, o): return struct.unpack_from('<I', self.b, o)[0]
    def f32(self, o): return struct.unpack_from('<f', self.b, o)[0]

    def cstr(self, o):
        """String at an absolute offset inside the string region."""
        if o == 0 or o >= len(self.b): return None
        if not (self.strOff <= o < self.dataOff): return None
        e = self.b.index(b'\0', o)
        return self.b[o:e].decode('latin1')

def main():
    path = sys.argv[1]
    f = sys.argv[2:]
    d = BDAE(path); D = d.dataOff
    print('%s  %d bytes  (header %d, %d relocations, strings @%#x, data @%#x)'
          % (path, len(d.b), d.headerSize, d.numReloc, d.strOff, D))
    tables = [
        ('clips',     28, 32, 12),
        ('nodes',     36, 40, 80),
        ('images',    52, 56, 20),
        ('effects',   60, 64, 92),
        ('materials', 68, 72, None),
        ('geometry',  76, 80, 16),
    ]
    counts = {}
    for name, co, po, stride in tables:
        c, p = d.u32(D + co), d.u32(D + po)
        counts[name] = (c, p)
        print('  %-10s count %-5d table @%#x%s'
              % (name, c, p, '' if stride is None else '  stride %d' % stride))
    if '--images' in f:
        c, p = counts['images']
        print('  images (filename = Max slot id, path basename = real file):')
        for i in range(c):
            e = p + i * 20
            print('    %2d  %-26s  %s' % (i, d.cstr(d.u32(e)), d.cstr(d.u32(e + 8))))
    if '--effects' in f:
        c, p = counts['effects']
        print('  effects (+76/+80 UV sets, +84/+88 image indices):')
        for i in range(min(c, 24)):
            e = p + i * 92
            n = d.u32(e + 84); ptr = d.u32(e + 88)
            idx = [d.u32(ptr + k * 4) for k in range(n)] if 0 < n < 8 else []
            print('    %-30s layers=%d images=%s' % (d.cstr(d.u32(e)), n, idx))
    if '--clips' in f:
        c, p = counts['clips']
        print('  animation clips: %d' % c)
        for i in range(min(c, 40)):
            print('    %s' % d.cstr(d.u32(p + i * 12)))
        if c > 40: print('    ... %d more' % (c - 40))
    if '--nodes' in f:
        c, p = counts['nodes']
        print('  scene nodes: %d (80-byte records, conjugate quaternions)' % c)
        for i in range(min(c, 30)):
            e = p + i * 80
            print('    %2d  %-28s t=(%.1f, %.1f, %.1f)'
                  % (i, d.cstr(d.u32(e)), d.f32(e + 48), d.f32(e + 52), d.f32(e + 56)))

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    main()
