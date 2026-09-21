#!/usr/bin/env python3
"""Print format, channels, rate and duration for WAVs under a directory.
usage: wav_info.py <dir> [--limit N]"""
import os, struct, sys
root = sys.argv[1]; limit = int(sys.argv[sys.argv.index('--limit') + 1]) if '--limit' in sys.argv else 10**9
n = 0
for r, _, fs in os.walk(root):
    for f in sorted(fs):
        if not f.lower().endswith('.wav') or n >= limit: continue
        b = open(os.path.join(r, f), 'rb').read(64)
        if b[:4] != b'RIFF': continue
        fmt, ch = struct.unpack_from('<HH', b, 20); rate = struct.unpack_from('<I', b, 24)[0]; bits = struct.unpack_from('<H', b, 34)[0]
        print('%-44s fmt=%-2d ch=%d rate=%5d bits=%d' % (os.path.relpath(os.path.join(r, f), root), fmt, ch, rate, bits)); n += 1
print('\n%d files' % n)
