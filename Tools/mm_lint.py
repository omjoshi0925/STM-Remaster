#!/usr/bin/env python3
# Tools/mm_lint.py - quick static checks for Renderer.mm that run anywhere (no SDK).
# usage: Tools/mm_lint.py [NativePort/Sources/Renderer.mm]
# Static checks for Renderer.mm (no Metal compiler here): bracket balance outside
# raw strings/comments/strings, includes exist, every bdae::Type is declared in an
# included header, no obviously undeclared ivars, nowMs not used before declared.
import re, sys, os
p = sys.argv[1] if len(sys.argv) > 1 else "NativePort/Sources/Renderer.mm"; src = open(p).read(); d = os.path.dirname(p)
# strip R"( ... )" raw strings, comments and string literals
body = re.sub(r'R"\((.*?)\)"', '""', src, flags=re.S)
body = re.sub(r'//[^\n]*', '', body)
body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
body = re.sub(r'@?"(\\.|[^"\\\n])*"', '""', body)
body = re.sub(r"'(\\.|[^'\\\n])'", "''", body)
ok = True
for o, c in ['{}', '()', '[]']:
    if body.count(o) != body.count(c):
        print(f"UNBALANCED {o}{c}: {body.count(o)} vs {body.count(c)}"); ok = False
for inc in re.findall(r'#include "([^"]+)"', src):
    if not os.path.exists(os.path.join(d, inc)): print("MISSING include", inc); ok = False
decl = ""
for inc in re.findall(r'#include "([^"]+)"', src):
    q = os.path.join(d, inc)
    if os.path.exists(q): decl += open(q).read()
    # one level of nested includes
    for inc2 in re.findall(r'#include "([^"]+)"', open(q).read() if os.path.exists(q) else ""):
        q2 = os.path.join(d, inc2)
        if os.path.exists(q2): decl += open(q2).read()
for t in sorted(set(re.findall(r'bdae::([A-Za-z_]\w*)', src))):
    if not re.search(r'\b(struct|class|enum|using)\s+' + t + r'\b', decl) and not re.search(r'\b' + t + r'\s*\(', decl):
        print("UNDECLARED bdae::" + t); ok = False
# nowMs before declaration inside drawInMTKView
m = re.search(r'- \(void\)drawInMTKView:(.*?)\n- \(', src, flags=re.S)
if m:
    fn = m.group(1); dpos = fn.find('uint32_t nowMs'); upos = re.search(r'\bnowMs\b', fn).start()
    if dpos < 0 or upos < dpos: print("nowMs used before declaration in drawInMTKView"); ok = False
# every ivar-looking identifier _xxx used must be declared in the @implementation block
st = src.find('@interface TMRenderer ()')
ivblock = src[st:src.find('\n}\n', st)]
declared = set(re.findall(r'\b(_[a-zA-Z]\w*)\s*(?:;|,|\[|=|\))', ivblock))
declared |= set(re.findall(r'\*\s*(_[a-zA-Z]\w*)', ivblock))
used = set(re.findall(r'(?<![\w.>])(_[a-zA-Z]\w*)\b', body[body.find('@implementation'):]))
skip = {'_white'}  # declared elsewhere maybe
missing = [u for u in sorted(used - declared) if u not in skip and not u.startswith('_cmd') and u not in ('_Nonnull','_Nullable')]
# locals that legitimately start with an underscore are rare; anything listed here needs a look
if missing: print("identifiers used but not declared as ivars (check manually):", missing); ok = False
print("mm_lint", "OK" if ok else "FAILED"); sys.exit(0 if ok else 1)
