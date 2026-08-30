#!/usr/bin/env python3
"""Extract Gameloft GBMP packs used by Spider-Man: Total Mayhem.
They are ZIP containers whose local-file signature PK\x03\x04 was replaced by GBMP.
"""
from pathlib import Path
import argparse, zipfile, tempfile, shutil

def patched_bytes(src: Path) -> bytes:
    data = bytearray(src.read_bytes())
    magic=b'GBMP'; repl=b'PK\x03\x04'; start=0; n=0
    while True:
        i=data.find(magic,start)
        if i<0: break
        data[i:i+4]=repl; start=i+4; n+=1
    if not n:
        raise ValueError(f'{src}: no GBMP signatures found')
    return bytes(data)

def extract(src: Path, dest: Path):
    dest.mkdir(parents=True, exist_ok=True)
    raw=patched_bytes(src)
    with tempfile.NamedTemporaryFile(suffix='.zip') as tf:
        tf.write(raw); tf.flush()
        with zipfile.ZipFile(tf.name) as z:
            bad=z.testzip()
            if bad: raise ValueError(f'{src}: CRC failure in {bad}')
            z.extractall(dest)
            return len(z.infolist()), sum(i.file_size for i in z.infolist())

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('input', type=Path, help='A .pack file or directory containing .pack files')
    ap.add_argument('output', type=Path)
    a=ap.parse_args()
    packs=[a.input] if a.input.is_file() else sorted(a.input.glob('*.pack'))
    if not packs: raise SystemExit('No .pack files found')
    for p in packs:
        out=a.output/p.stem
        count,size=extract(p,out)
        print(f'{p.name}: {count} files, {size} bytes -> {out}')
if __name__=='__main__': main()
