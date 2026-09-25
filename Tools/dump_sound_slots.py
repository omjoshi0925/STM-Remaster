#!/usr/bin/env python3
"""Print the original sound slot tables resolved to event names.
usage: dump_sound_slots.py <Assets dir> [enemy|hero]"""
import os, struct, sys
def rows(path):
    b = open(path, 'rb').read()
    def at(o):
        if o + 2 > len(b): return None
        L = struct.unpack_from('<H', b, o)[0]
        if 2 <= L <= 80 and o + 2 + L <= len(b) and all(32 <= c < 127 for c in b[o+2:o+2+L]): return b[o+2:o+2+L].decode()
        return None
    off, cur, nums, out = 4, None, [], []
    while off < len(b) - 1:
        s = at(off)
        if s is None and at(off + 2): off += 2; s = at(off)
        if s is not None:
            if cur is not None: out.append((cur, nums))
            cur, nums = s, []; off += 2 + struct.unpack_from('<H', b, off)[0]; continue
        if off + 4 <= len(b): nums.append(struct.unpack_from('<I', b, off)[0])
        off += 4
    if cur is not None: out.append((cur, nums))
    return out
assets = sys.argv[1]; which = sys.argv[2] if len(sys.argv) > 2 else 'both'
events = [n for n, _ in rows(os.path.join(assets, 'configs', 'VoxSounds.bin')) if not n.lower().endswith('.wav')]
name = lambda i: events[i] if i < len(events) else ('-' if i == 0xffffffff else '?%d' % i)
if which in ('enemy', 'both'):
    print('BehaviorSoundMapList (slot: per-archetype column)')
    for slot, nums in rows(os.path.join(assets, 'configs', 'BehaviorSoundMapList.bin')):
        print('  %-22s %s' % (slot, '  '.join(name(i) for i in nums[:8])))
if which in ('hero', 'both'):
    print('MC_SOUND (hero slot: variants)')
    for slot, nums in rows(os.path.join(assets, 'configs', 'MC_SOUND.bin')):
        u = []
        for v in nums: u += [v & 0xffff, v >> 16]
        cnt = u[4] if len(u) > 4 else 0
        idx = [x for x in u[5:5 + cnt] if x != 0xffff]
        print('  %-32s %s' % (slot, ', '.join(name(i) for i in idx)))
