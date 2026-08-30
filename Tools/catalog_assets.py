#!/usr/bin/env python3
from pathlib import Path
import argparse, collections, hashlib, json, re, struct, xml.etree.ElementTree as ET
from parse_bdae324 import parse as parse_bdae
from parse_irr import parse as parse_irr

def sha(p):
    h=hashlib.sha256();
    with p.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('root',type=Path); ap.add_argument('output',type=Path)
    a=ap.parse_args(); root=a.root
    files=[p for p in root.rglob('*') if p.is_file()]
    ext=collections.Counter(p.suffix.lower() or '<none>' for p in files)
    groups=collections.Counter(p.relative_to(root).parts[0] for p in files)
    game_types=collections.Counter(); scene_nodes=0; irr_errors=[]
    bdae_versions=collections.Counter(); bdae_errors=[]
    for p in files:
        if p.suffix.lower()=='.irr':
            try:
                sc=parse_irr(p); scene_nodes += len(sc['nodes'])
                for n in sc['nodes']:
                    if n.get('game_type'): game_types[n['game_type']]+=1
            except Exception as e: irr_errors.append({'file':str(p.relative_to(root)),'error':str(e)})
        elif p.suffix.lower()=='.bdae':
            try:
                m=parse_bdae(p); bdae_versions[m.get('detected_resource_version') or 'unknown']+=1
            except Exception as e: bdae_errors.append({'file':str(p.relative_to(root)),'error':str(e)})
    manifest={'root':str(root),'total_files':len(files),'total_bytes':sum(p.stat().st_size for p in files),
              'extensions':dict(ext.most_common()),'groups':dict(groups.most_common()),
              'scene_nodes':scene_nodes,'game_types':dict(game_types.most_common()),
              'bdae_versions':dict(bdae_versions),'irr_errors':irr_errors,'bdae_errors':bdae_errors}
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(f"{len(files)} files; {scene_nodes} scene nodes; {len(game_types)} game types; BDAE {dict(bdae_versions)}")
if __name__=='__main__': main()
