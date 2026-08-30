#!/usr/bin/env python3
from pathlib import Path
import argparse, re, json, xml.etree.ElementTree as ET

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('root',type=Path); ap.add_argument('output',type=Path)
    a=ap.parse_args(); items=[]
    for p in sorted(a.root.rglob('*.cff')):
        try:
            text=p.read_text(encoding='utf-16')
            text=re.sub(r'<\?xml[^>]*\?>','',text)
            r=ET.fromstring('<root>'+text+'</root>')
            tags={}
            for e in r.iter(): tags[e.tag]=tags.get(e.tag,0)+1
            items.append({'file':str(p.relative_to(a.root)),'bytes':p.stat().st_size,'tags':tags})
        except Exception as e: items.append({'file':str(p.relative_to(a.root)),'error':str(e)})
    a.output.write_text(json.dumps({'cinematics':items},indent=2),encoding='utf-8')
    print(f'{len(items)} cinematic files')
if __name__=='__main__':main()
