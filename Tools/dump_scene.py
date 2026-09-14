#!/usr/bin/env python3
"""List the authored nodes in a level's .irr scenes by !GameType, with the
attributes that drive the runtime (mesh, cinematic script, camera owner).

usage: dump_scene.py <Assets dir> <levelnew_01> [--type Trigger] [--attrs]
"""
import glob, os, re, sys
from collections import Counter

def nodes(level_dir):
    for f in sorted(glob.glob(os.path.join(level_dir, '*.irr'))):
        text = open(f, 'rb').read().decode('utf-16-le', errors='replace')
        for m in re.finditer(r'<attributes>(.*?)</attributes>', text, re.S):
            attrs = dict(re.findall(r'name="([^"]+)" value="([^"]*)"', m.group(1)))
            if '!GameType' in attrs:
                yield os.path.basename(f), attrs

def main():
    assets, level = sys.argv[1], sys.argv[2]
    want = None
    if '--type' in sys.argv: want = sys.argv[sys.argv.index('--type') + 1]
    show = '--attrs' in sys.argv
    census = Counter()
    for fname, a in nodes(os.path.join(assets, level)):
        gt = a['!GameType']
        census[gt] += 1
        if want and gt != want: continue
        if want or show:
            keep = {k: v for k, v in a.items()
                    if k in ('Name', 'Position', 'Scale') or k.startswith('!')}
            print('%-22s %-26s %s' % (gt, a.get('Name', ''), keep if show else ''))
    if not want:
        print('\n%s node census:' % level)
        for k, v in census.most_common():
            print('  %-24s %d' % (k, v))
    print('\n%d nodes total' % sum(census.values()))

if __name__ == '__main__':
    if len(sys.argv) < 3: print(__doc__); sys.exit(1)
    main()
