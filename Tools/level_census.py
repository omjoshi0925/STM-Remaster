#!/usr/bin/env python3
"""GameType census across every extracted level.
usage: level_census.py <Assets dir>"""
import glob, os, re, sys
from collections import Counter
assets = sys.argv[1]
levels = sorted(d for d in os.listdir(assets) if d.startswith('levelnew_'))
types = set(); rows = {}
for lv in levels:
    c = Counter()
    for f in glob.glob(os.path.join(assets, lv, '*.irr')):
        s = open(f, 'rb').read().decode('utf-16-le', errors='replace')
        c.update(re.findall(r'name="!GameType" value="([^"]*)"', s))
    rows[lv] = c; types |= set(c)
print('%-22s' % 'GameType' + ''.join('%7s' % lv[-2:] for lv in levels))
for t in sorted(types, key=lambda t: -sum(rows[lv][t] for lv in levels)):
    print('%-22s' % t + ''.join('%7d' % rows[lv][t] for lv in levels))
