#!/usr/bin/env python3
"""Structural parser for the BRES/BDAE 0.0.0.324 resources used by Total Mayhem.
This intentionally does not guess undocumented vertex layouts. It reliably exposes
header fields, relocation entries, string table, and data-region metadata.
"""
from pathlib import Path
import argparse, struct, json, re, hashlib

def cstrings(blob: bytes, base: int):
    out=[]; i=0
    while i < len(blob):
        j=blob.find(b'\0',i)
        if j<0: j=len(blob)
        b=blob[i:j]
        if b and all((32 <= x < 127) or x in (9,10,13) for x in b):
            try: out.append({'offset':base+i,'text':b.decode('utf-8')})
            except: pass
        i=j+1
    return out

def parse(path: Path):
    b=path.read_bytes()
    if len(b)<32: raise ValueError('too small')
    sig,endian,version,header_size,file_size,num_offsets,offset_table,string_off,data_off=struct.unpack_from('<4sHH6I',b,0)
    if sig != b'BRES': raise ValueError('not a BRES/BDAE resource')
    if file_size != len(b): raise ValueError(f'header file size {file_size} != actual {len(b)}')
    if offset_table + 4*num_offsets != string_off:
        raise ValueError('unexpected 0.0.0.324 relocation-table layout')
    if not (0 <= string_off <= data_off <= len(b)): raise ValueError('invalid sections')
    relocs=list(struct.unpack_from('<'+'I'*num_offsets,b,offset_table)) if num_offsets else []
    strings=cstrings(b[string_off:data_off],string_off)
    version_strings=[s['text'] for s in strings if re.fullmatch(r'\d+,\d+,\d+,\d+',s['text'])]
    return {
      'path':str(path),'size':len(b),'sha256':hashlib.sha256(b).hexdigest(),
      'header':{'signature':'BRES','endian_check':endian,'version_field':version,'header_size':header_size,
                'file_size':file_size,'num_relocations':num_offsets,'relocation_table_offset':offset_table,
                'string_table_offset':string_off,'data_offset':data_off},
      'detected_resource_version':version_strings[0] if version_strings else None,
      'relocations':relocs,
      'strings':strings,
      'data_size':len(b)-data_off,
    }

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('input',type=Path); ap.add_argument('-o','--output',type=Path)
    a=ap.parse_args(); result=parse(a.input)
    text=json.dumps(result,indent=2,ensure_ascii=False)
    if a.output: a.output.write_text(text,encoding='utf-8')
    else: print(text)
if __name__=='__main__': main()
