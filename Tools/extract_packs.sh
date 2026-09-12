#!/bin/bash
# Extracts Gameloft .pack archives into an Assets/ tree.
# A .pack is a ZIP whose local file headers read GBMP instead of PK\x03\x04.
#
# usage: extract_packs.sh <OriginalPacks dir> <Assets dir> [pack ...]
#        (no pack names = extract every .pack found)
set -e
SRC="${1:?usage: extract_packs.sh <OriginalPacks dir> <Assets dir> [pack ...]}"
DST="${2:?usage: extract_packs.sh <OriginalPacks dir> <Assets dir> [pack ...]}"
shift 2 || true
if [ "$#" -gt 0 ]; then PACKS="$*"; else
  PACKS="$(cd "$SRC" && ls *.pack 2>/dev/null | sed 's/\.pack$//')"
fi
[ -n "$PACKS" ] || { echo "no .pack files in $SRC"; exit 1; }
mkdir -p "$DST"
for p in $PACKS; do
  f="$SRC/$p.pack"
  [ -f "$f" ] || { echo "skip $p (not found)"; continue; }
  tmp="$(mktemp -t "${p}XXXX").zip"
  python3 -c "
import sys
d = open(sys.argv[1],'rb').read()
open(sys.argv[2],'wb').write(d.replace(b'GBMP', b'PK\x03\x04'))" "$f" "$tmp"
  mkdir -p "$DST/$p"
  unzip -o -q "$tmp" -d "$DST/$p"
  rm -f "$tmp"
  echo "extracted $p -> $DST/$p ($(find "$DST/$p" -type f | wc -l | tr -d ' ') files)"
done
