#!/usr/bin/env python3
"""Print a level's authored script graph: the SpiderMan node's start and
epilogue scripts, boss nodes' ^ToStage2/3^ phase scripts, every trigger with its Enabled/AutoDisabled flags and its
four Cinematic links, and for each script the control-flow commands it carries
(If* gates, StartCinematic, Enable/DisableTrigger, Save, LevelEnd, Tutorial,
PlayDAECamera next). Ends with the Cinematic nodes nothing reaches.
usage: script_graph.py <Assets dir> <levelnew_NN>"""
import glob, os, re, sys
def load16(path):
    b = open(path, 'rb').read()
    return b[2:].decode('utf-16-le', errors='replace') if b[:2] == b'\xff\xfe' else b.decode('utf-16-le', errors='replace')
def nodes(level_dir):
    out = {}
    for f in glob.glob(os.path.join(level_dir, '*.irr')):
        s = load16(f)
        for m in re.finditer(r'<node type="[^"]*">\s*<attributes>(.*?)</attributes>(.*?)</node>', s, re.S):
            a, whole = m.group(1), m.group(0)
            idm = re.search(r'<int name="Id" value="(-?\d+)"', a)
            if not idm: continue
            d = dict(re.findall(r'<(?:string|int|bool|float) name="([^"]+)" value="([^"]*)"', whole))
            out[int(idm.group(1))] = d
    return out
def commands(text):
    for th in re.finditer(r'<cinematicThread type="(\d+)" name="([^"]*)" object="(-?\d+)">(.*?)</cinematicThread>', text, re.S):
        for tm in re.finditer(r'<time stamp="(\d+)">(.*?)</time>', th.group(4), re.S):
            for cm in re.finditer(r'<command name="(\w+)" id="\d+">(.*?)</command>', tm.group(2), re.S):
                yield int(tm.group(1)), cm.group(1), dict((k, v) for _, k, v in re.findall(r'<(\w+) name="([^"]+)" value="([^"]*)"', cm.group(2)))
def summary(path):
    if not os.path.exists(path): return 'MISSING', set()
    parts, reached = [], set()
    dur = 0
    for stamp, cmd, a in commands(load16(path)):
        dur = max(dur, stamp)
        if cmd in ('IfObjectDestroyed', 'IfEnemyDead'): parts.append('%s %s' % (cmd, a.get('ObjectID', a.get('IDEnemy'))))
        elif cmd == 'IfHealthTo': parts.append('IfHealthTo %s<=%s%%' % (a.get('IDEnemy'), a.get('Health')))
        elif cmd == 'StartCinematic': parts.append('@%d StartCinematic %s' % (stamp, a.get('CinematicID'))); reached.add(int(a.get('CinematicID', -1)))
        elif cmd in ('EnableTrigger', 'DisableTrigger'): parts.append('@%d %s %s' % (stamp, cmd, a.get('^ID^Trigger')))
        elif cmd == 'Save': parts.append('@%d Save %s' % (stamp, a.get('^ID^CheckPoint')))
        elif cmd in ('LevelEnd', 'GameEnd'): parts.append('@%d %s' % (stamp, cmd))
        elif cmd == 'Tutorial': parts.append('@%d Tutorial %s' % (stamp, a.get('Content$Tutorial_STRINGID')))
        elif cmd == 'PlayDAECamera':
            nxt = int(a.get('^ID^Cinematic^Next', -1)); parts.append('camera %s next %d%s' % (os.path.basename(a.get('CameraAnimFile', '').replace('\\', '/')), nxt, ' LEVEL END' if a.get('level end') == 'true' else ''))
            if nxt >= 0: reached.add(nxt)
        elif cmd == 'StartQTE':
            for k in ('^ID^Cinematic^Success', '^ID^Cinematic^Fail'):
                if k in a: reached.add(int(a[k]))
            parts.append('@%d StartQTE -> %s/%s' % (stamp, a.get('^ID^Cinematic^Success'), a.get('^ID^Cinematic^Fail')))
    return ('%.1fs: ' % (dur / 1000.0)) + ('; '.join(parts) if parts else 'presentation only'), reached
def main():
    if len(sys.argv) < 3: print(__doc__); sys.exit(1)
    root, lv = sys.argv[1], sys.argv[2]
    ldir = os.path.join(root, lv)
    n = nodes(ldir)
    cine = {i: d for i, d in n.items() if d.get('!GameType') == 'Cinematic'}
    path = lambda i: os.path.join(ldir, cine[i].get('!ScriptFile', '').replace('\\', '/').lstrip('./')) if i in cine else ''
    reached = set()
    spidey = next((d for d in n.values() if d.get('!GameType') == 'SpiderMan'), {})
    for k, label in (('^Link^Cinematic', 'level start'), ('^EndGame^Cinematic', 'epilogue')):
        i = int(spidey.get(k, -1))
        if i >= 0:
            s, r = summary(path(i)); reached.add(i); reached |= r
            print('%s: %d %s  [%s]' % (label, i, cine.get(i, {}).get('Name', '?'), s))
    # boss nodes name their phase scripts (^ToStage2^Cinematic at 66 %, ^ToStage3^ at 33 %)
    for i, d in sorted(n.items()):
        for k in ('^ToStage2^Cinematic', '^ToStage3^Cinematic'):
            v = int(d.get(k, -1))
            if v >= 0:
                s, r = summary(path(v)); reached.add(v); reached |= r
                print('boss %d %s %s: %d %s  [%s]' % (i, d.get('Name', ''), k.strip('^').split('^')[0], v, cine.get(v, {}).get('Name', '?'), s))
    print()
    for i, d in sorted(n.items()):
        if d.get('!GameType') not in ('Trigger', 'TriggerRestore'): continue
        links = [(k, int(d.get(k, -1))) for k in ('^OutToIn^Cinematic', '^InToOut^Cinematic', '^WhileIn^Cinematic', '^WhileOut^Cinematic')]
        live = [(k.strip('^').split('^')[0], v) for k, v in links if v >= 0]
        if d.get('!GameType') == 'TriggerRestore' and not live: continue   # death-recovery volumes carry no scripts
        print('trigger %-6d %-28s enabled=%-5s auto=%-5s %s' % (i, d.get('Name', ''), d.get('Enabled', '?'), d.get('AutoDisabled', '?'), ' '.join('%s->%d' % kv for kv in live) or '(no links)'))
        for _, v in live:
            s, r = summary(path(v)); reached.add(v); reached |= r
            print('           %d %s  [%s]' % (v, cine.get(v, {}).get('Name', '?'), s))
    # scripts reached transitively through StartCinematic / Next / QTE
    frontier = set(reached)
    while frontier:
        nxt = set()
        for i in frontier:
            if i in cine:
                _, r = summary(path(i)); nxt |= (r - reached)
        reached |= nxt; frontier = nxt
    # scripts that end the level (LevelEnd / GameEnd, or a level-end camera)
    enders = [i for i in sorted(cine) if re.search(r'LevelEnd|GameEnd|LEVEL END', summary(path(i))[0])]
    print('\nlevel ends: ' + (', '.join('%d %s%s' % (i, cine[i].get('Name', ''), '' if i in reached else ' (unreached)') for i in enders) or 'none (checkpoint completion)'))
    orphans = sorted(i for i in cine if i not in reached)
    print('\n%d Cinematic nodes, %d reached from the spawn node, boss stages, triggers, StartCinematic, Next and QTE branches; %d unreached:' % (len(cine), len(reached & set(cine)), len(orphans)))
    for i in orphans: print('   %d %s %s' % (i, cine[i].get('Name', ''), summary(path(i))[0]))
if __name__ == '__main__':
    main()
