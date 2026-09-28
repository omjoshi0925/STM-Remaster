#!/usr/bin/env python3
"""List the cinematic actors of a level: every PlayDAEAnim an object thread
issues, the scene node it names (name, !GameType, MeshFile), the animation
file requested, and whether that file ships in the level pack (exact name or
the "_<digits>_" variant the runtime accepts).
usage: list_cine_actors.py <Assets dir> <levelnew_NN> [--md]"""
import glob, os, re, sys
def load16(path):
    b = open(path, 'rb').read()
    return b[2:].decode('utf-16-le', errors='replace') if b[:2] == b'\xff\xfe' else b.decode('utf-16-le', errors='replace')
def scene_nodes(level_dir):
    nodes = {}
    for f in glob.glob(os.path.join(level_dir, '*.irr')):
        s = load16(f)
        for m in re.finditer(r'<node type="[^"]*">\s*<attributes>(.*?)</attributes>(.*?)</node>', s, re.S):
            a, whole = m.group(1), m.group(0)
            idm = re.search(r'<int name="Id" value="(-?\d+)"', a)
            if not idm: continue
            d = {}
            for k in ('Name', '!GameType', 'MeshFile'):
                mm = re.search(r'<string name="%s" value="([^"]*)"' % re.escape(k), whole)
                if mm: d[k] = mm.group(1)
            nodes[int(idm.group(1))] = d
    return nodes
def collapse(name):   # web_rope_ci_0_lv1_start -> web_rope_ci_lv1_start
    return re.sub(r'_\d+(?=_)', '', name.lower())
def resolve(dir_, base):
    files = os.listdir(dir_) if os.path.isdir(dir_) else []
    for f in files:
        if f.lower() == base.lower(): return f
    cands = [f for f in files if collapse(f) == collapse(base)]
    return cands[0] if len(cands) == 1 else None
def main():
    if len(sys.argv) < 3: print(__doc__); sys.exit(1)
    root, lv = sys.argv[1], sys.argv[2]
    md = '--md' in sys.argv
    ldir = os.path.join(root, lv)
    nodes = scene_nodes(ldir)
    rows = []
    for f in sorted(glob.glob(os.path.join(ldir, 'cinematics', '*.cff'))):
        s = load16(f)
        for th in re.finditer(r'<cinematicThread type="(\d+)" name="([^"]*)" object="(-?\d+)">(.*?)</cinematicThread>', s, re.S):
            ttype, _, obj, body = th.groups()
            if ttype == '3': continue   # the player thread is Spider-Man's own path
            for tm in re.finditer(r'<time stamp="(\d+)">(.*?)</time>', body, re.S):
                for cm in re.finditer(r'<command name="PlayDAEAnim" id="\d+">(.*?)</command>', tm.group(2), re.S):
                    af = re.search(r'name="AnimFile" value="([^"]*)"', cm.group(1))
                    base = os.path.basename(af.group(1).replace('\\', '/')) if af else '?'
                    n = nodes.get(int(obj), {})
                    shipped = resolve(os.path.join(ldir, 'meshes_bin'), base)
                    rows.append((os.path.basename(f), int(obj), n.get('Name', '?'), n.get('!GameType', '?'),
                                 os.path.basename(n.get('MeshFile', '?').replace('\\', '/')), int(tm.group(1)), base, shipped or 'MISSING'))
    if md:
        print('| script | object | node | type | mesh | at | animation | ships as |')
        print('|---|---|---|---|---|---|---|---|')
        for r in rows: print('| %s | %d | %s | %s | %s | %.1f s | %s | %s |' % (r[0], r[1], r[2], r[3], r[4], r[5] / 1000.0, r[6], r[7]))
    else:
        for r in rows: print('%-34s obj %-6d %-14s %-16s %-36s %6.1fs  %-36s -> %s' % (r[0], r[1], r[2], r[3], r[4], r[5] / 1000.0, r[6], r[7]))
    missing = sum(1 for r in rows if r[7] == 'MISSING')
    print('\n%d actor animations, %d missing' % (len(rows), missing))
if __name__ == '__main__':
    main()
