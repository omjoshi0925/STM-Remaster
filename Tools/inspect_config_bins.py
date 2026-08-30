#!/usr/bin/env python3
from pathlib import Path
import argparse, json, re, collections
PAT=re.compile(rb'[A-Za-z_][A-Za-z0-9_./\\-]{3,}')
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('root',type=Path); ap.add_argument('output',type=Path)
    a=ap.parse_args(); out=[]
    for p in sorted((a.root/'configs').glob('*.bin')):
        b=p.read_bytes(); ss=[]
        seen=set()
        for m in PAT.finditer(b):
            s=m.group().decode('ascii','ignore')
            if s not in seen: seen.add(s); ss.append({'offset':m.start(),'text':s})
        out.append({'file':p.name,'bytes':len(b),'strings':ss})
    a.output.write_text(json.dumps({'tables':out},indent=2),encoding='utf-8')
    print(f'{len(out)} config tables')
if __name__=='__main__':main()
