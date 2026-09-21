#!/usr/bin/env python3
"""Resolve a texture name the way the runtime does, across every mounted pack.
usage: find_texture.py <Assets dir> <name.tga> [...]"""
import os, re, sys
assets = sys.argv[1]
idx = {}
for d in os.listdir(assets):
    tb = os.path.join(assets, d, 'textures_bin')
    if os.path.isdir(tb):
        for f in os.listdir(tb): idx.setdefault(f.lower(), os.path.join(tb, f))
def resolve(name):
    low = name.lower()
    if low in idx: return idx[low], 'exact'
    stem, ext = os.path.splitext(low)
    s2 = re.sub(r'[0-9]+$', '', stem)
    if s2 + ext in idx: return idx[s2 + ext], 'variant digits stripped'
    core = re.sub(r'^[_0-9]+', '', low)
    if len(core) > 5:
        for k, v in idx.items():
            if k.endswith(core): return v, 'prefix digits ignored'
    return None, 'unresolved'
for n in sys.argv[2:]:
    p, how = resolve(n)
    print('%-28s %-24s %s' % (n, how, p or ''))
