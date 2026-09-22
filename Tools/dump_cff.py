#!/usr/bin/env python3
"""Print a .cff cinematic script as a timeline, or census a directory.
usage: dump_cff.py <file.cff>            timeline of one script
       dump_cff.py <dir> --census        command vocabulary across a directory"""
import glob, os, re, sys
from collections import Counter
def load(path):
    b = open(path, 'rb').read()
    return b[2:].decode('utf-16-le', errors='replace') if b[:2] == b'\xff\xfe' else b.decode('utf-16-le', errors='replace')
def threads(text):
    for th in re.finditer(r'<cinematicThread type="(\d+)" name="([^"]*)" object="(\d+)">(.*?)</cinematicThread>', text, re.S):
        ttype, name, obj, body = th.groups()
        cmds = []
        for tm in re.finditer(r'<time stamp="(\d+)">(.*?)</time>', body, re.S):
            for cm in re.finditer(r'<command name="(\w+)" id="(\d+)">(.*?)</command>', tm.group(2), re.S):
                attrs = re.findall(r'<(\w+) name="([^"]+)" value="([^"]*)"', cm.group(3))
                cmds.append((int(tm.group(1)), cm.group(1), attrs))
        yield int(ttype), name, int(obj), cmds
def main():
    p = sys.argv[1]
    if '--census' in sys.argv:
        c = Counter(); n = 0
        for f in glob.glob(os.path.join(p, '*.cff')):
            n += 1
            for _, _, _, cmds in threads(load(f)): c.update(k for _, k, _ in cmds)
        for k, v in c.most_common(): print('%-16s %d' % (k, v))
        print('\n%d scripts' % n); return
    for ttype, name, obj, cmds in threads(load(p)):
        print('thread type=%d %s (object %d)' % (ttype, name, obj))
        for stamp, cmd, attrs in cmds:
            short = ', '.join('%s=%s' % (k, v[:32]) for _, k, v in attrs if k in ('pos', '$Anim', '$VoxSounds', 'target', 'Distance', 'AnimFile', 'QTEID', '^ID^CameraArea', 'Visible'))
            print('  %6d ms  %-14s %s' % (stamp, cmd, short))
if __name__ == '__main__':
    if len(sys.argv) < 2: print(__doc__); sys.exit(1)
    main()
