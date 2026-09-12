#!/usr/bin/env python3
"""List the game's sound events from configs/VoxSounds.bin, optionally
checking that every referenced clip exists under Assets/sounds/.

usage: dump_vox.py <Assets dir> [--check] [--filter SUBSTRING]
"""
import os, struct, sys

def strings_and_numbers(b):
    """Yields (string, [numbers]) records from a Gameloft field-stream .bin.
    Strings may start two bytes into a 4-byte phase, so both are probed."""
    def at(o):
        if o + 2 > len(b): return None
        L = struct.unpack_from('<H', b, o)[0]
        if 2 <= L <= 80 and o + 2 + L <= len(b) and all(32 <= c < 127 for c in b[o+2:o+2+L]):
            return b[o+2:o+2+L].decode()
        return None
    off, cur, nums = 4, None, []
    while off < len(b) - 1:
        s = at(off)
        if s is None and at(off + 2):
            off += 2; s = at(off)
        if s is not None:
            if cur is not None: yield cur, nums
            cur, nums = s, []
            off += 2 + struct.unpack_from('<H', b, off)[0]
            continue
        if off + 4 <= len(b):
            nums.append(struct.unpack_from('<I', b, off)[0])
        off += 4
    if cur is not None: yield cur, nums

def main():
    assets = sys.argv[1]
    check = '--check' in sys.argv
    filt = None
    if '--filter' in sys.argv:
        filt = sys.argv[sys.argv.index('--filter') + 1].upper()
    b = open(os.path.join(assets, 'configs', 'VoxSounds.bin'), 'rb').read()
    events, pending = [], None
    for name, nums in strings_and_numbers(b):
        if name.lower().endswith('.wav'):
            if pending: events.append((pending, name, nums[0] if nums else 0))
            pending = None
        else:
            pending = name
    missing = 0
    for ev, path, flags in events:
        if filt and filt not in ev.upper(): continue
        mark = ''
        if check:
            ok = os.path.exists(os.path.join(assets, 'sounds', path))
            mark = '' if ok else '   MISSING'
            missing += 0 if ok else 1
        print('%-34s %-40s flags=%#x%s' % (ev, path, flags, mark))
    print('\n%d events%s' % (len(events), (', %d missing clips' % missing) if check else ''))
    return 1 if missing else 0

if __name__ == '__main__':
    if len(sys.argv) < 2: print(__doc__); sys.exit(1)
    sys.exit(main())
