#!/usr/bin/env python3
"""List xlsStrings keys with the length of their value in a language.
usage: list_strings.py <Assets dir> [LANG=EN] [--filter SUBSTRING]"""
import os, sys
assets = sys.argv[1]
lang = 'EN'
filt = None
for a in sys.argv[2:]:
    if a == '--filter': filt = sys.argv[sys.argv.index(a) + 1].upper()
    elif not a.startswith('-') and a != filt: lang = a
keys = [l.rstrip('\r\n') for l in open(os.path.join(assets, 'xlsStrings', 'MAIN.map'))]
vals = [l.rstrip('\r\n') for l in open(os.path.join(assets, 'xlsStrings', 'MAIN_%s.data' % lang), errors='replace')]
n = 0
for i, k in enumerate(keys):
    if filt and filt not in k.upper(): continue
    v = vals[i] if i < len(vals) else ''
    print('%-40s %3d chars' % (k, len(v))); n += 1
print('\n%d keys (%d total, %s)' % (n, len(keys), lang))
