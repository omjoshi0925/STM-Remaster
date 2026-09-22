#!/usr/bin/env python3
"""Compare two extracted pack trees (e.g. iOS vs Android HD): files only in
one, and size differences for shared files. Useful for format diffing.
usage: compare_packs.py <dirA> <dirB>"""
import os, sys
a, b = sys.argv[1], sys.argv[2]
def walk(root):
    out = {}
    for r, _, fs in os.walk(root):
        for f in fs:
            p = os.path.join(r, f); out[os.path.relpath(p, root).lower()] = os.path.getsize(p)
    return out
A, B = walk(a), walk(b)
onlyA = sorted(set(A) - set(B)); onlyB = sorted(set(B) - set(A))
diff = sorted(k for k in set(A) & set(B) if A[k] != B[k])
print('only in A (%d):' % len(onlyA)); [print('  ' + k) for k in onlyA[:40]]
print('only in B (%d):' % len(onlyB)); [print('  ' + k) for k in onlyB[:40]]
print('size differs (%d):' % len(diff)); [print('  %-50s %8d %8d' % (k, A[k], B[k])) for k in diff[:40]]
