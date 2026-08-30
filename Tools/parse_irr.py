#!/usr/bin/env python3
"""Parse Total Mayhem Irrlicht .irr scene/room files, including multi-root room files."""
from pathlib import Path
import argparse, re, xml.etree.ElementTree as ET, json

def attrs(elem):
    out={}
    if elem is None: return out
    for ch in elem:
        name=ch.attrib.get('name')
        if not name: continue
        if 'value' in ch.attrib: value=ch.attrib['value']
        elif 'count' in ch.attrib:
            value=[ch.attrib.get(f'value{i}') for i in range(int(ch.attrib['count']))]
        else: value=dict(ch.attrib)
        out[name]={'type':ch.tag,'value':value}
    return out

def parse(path: Path):
    text=path.read_text(encoding='utf-16')
    text=re.sub(r'<\?xml[^>]*\?>','',text)
    root=ET.fromstring('<synthetic_root>'+text+'</synthetic_root>')
    links=[x.attrib.get('fileName') for x in root.iter('link') if x.attrib.get('fileName')]
    nodes=[]
    for n in root.iter('node'):
        a=attrs(n.find('attributes')); u=attrs(n.find('userData/attributes'))
        def v(d,k): return d.get(k,{}).get('value')
        nodes.append({'node_type':n.attrib.get('type',''),'id':v(a,'Id'),'name':v(u,'Name'),
                      'game_type':v(u,'!GameType'),'position':v(a,'Position'),'rotation':v(a,'Rotation'),
                      'scale':v(a,'Scale'),'visible':v(a,'Visible'),'mesh':v(u,'MeshFile'),
                      'attributes':a,'user_data':u})
    return {'path':str(path),'links':links,'nodes':nodes}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('input',type=Path); ap.add_argument('-o','--output',type=Path)
    a=ap.parse_args(); result=parse(a.input); s=json.dumps(result,indent=2,ensure_ascii=False)
    if a.output: a.output.write_text(s,encoding='utf-8')
    else: print(s)
if __name__=='__main__': main()
