#!/usr/bin/env python3
"""Prepare legal user-supplied assets from a Total Mayhem IPA for the native-port workbench."""
from pathlib import Path
import argparse, zipfile, tempfile, shutil, subprocess, sys
HERE=Path(__file__).resolve().parent

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('ipa',type=Path); ap.add_argument('workspace',type=Path)
    a=ap.parse_args(); out=a.workspace.resolve(); packs=out/'OriginalPacks'; assets=out/'Assets'; gen=out/'Generated'; loose=assets/'loose'
    for d in (packs,assets,gen,loose): d.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(a.ipa) as z:
        names=[n for n in z.namelist() if n.startswith('Payload/') and n.endswith('.pack')]
        if not names: raise SystemExit('No .pack assets found in IPA')
        for n in names:
            dest=packs/Path(n).name
            with z.open(n) as s, dest.open('wb') as d: shutil.copyfileobj(s,d)
        # Preserve loose game resources but intentionally omit the old ARM executable, signatures, and provisioning metadata.
        app_prefix=names[0].rsplit('/',1)[0]+'/'
        skip={'Spiderman','Info.plist','ResourceRules.plist','PkgInfo'}
        for n in z.namelist():
            if not n.startswith(app_prefix) or n.endswith('/') or n.endswith('.pack'): continue
            rel=Path(n[len(app_prefix):])
            if not rel.parts or rel.name in skip or '_CodeSignature' in rel.parts: continue
            dest=loose/rel; dest.parent.mkdir(parents=True,exist_ok=True)
            with z.open(n) as src, dest.open('wb') as dst: shutil.copyfileobj(src,dst)
        # Copy the original application icon into the new ARM64 app's root resources.
        native_res=out/'NativePort'/'Resources'; native_res.mkdir(parents=True,exist_ok=True)
        for icon in ('Icon.png','Icon@2x.png'):
            n=app_prefix+icon
            if n in z.namelist():
                with z.open(n) as src, (native_res/icon).open('wb') as dst: shutil.copyfileobj(src,dst)
    subprocess.check_call([sys.executable,str(HERE/'unpack_gbmp.py'),str(packs),str(assets)])
    subprocess.check_call([sys.executable,str(HERE/'catalog_assets.py'),str(assets),str(gen/'asset_manifest.json')])
    subprocess.check_call([sys.executable,str(HERE/'extract_cinematics.py'),str(assets),str(gen/'cinematic_manifest.json')])
    subprocess.check_call([sys.executable,str(HERE/'inspect_config_bins.py'),str(assets),str(gen/'config_strings.json')])
    print('Prepared assets at',out)
if __name__=='__main__': main()
